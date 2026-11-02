#include "departures_service.h"
#include "departures_merge.h"
#include "departures_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#error "The departure service requires the ESP-IDF certificate bundle"
#endif
#define RESPONSE_LIMIT (64 * 1024)
#define WIFI_CONNECTED BIT0
#define WIFI_READY BIT1
#define SETTINGS_CHANGED BIT0
#define HTTP_TIMEOUT_MS 15000
#define REFRESH_SECONDS 60
static const char *TAG = "departures";
static EventGroupHandle_t s_wifi_events;
static SemaphoreHandle_t s_snapshot_lock;
static QueueHandle_t s_updates, s_commands, s_results;
static departures_update_t *s_snapshot;
static TaskHandle_t s_task;
typedef enum { CMD_SCAN, CMD_SEARCH } command_type_t;
typedef struct { command_type_t type; char query[96]; } command_t;

static void publish(departures_update_t *update, departures_status_t status)
{
    update->status = status;
    xSemaphoreTake(s_snapshot_lock, portMAX_DELAY);
    *s_snapshot = *update;
    xSemaphoreGive(s_snapshot_lock);
    bool ready = true;
    (void)xQueueOverwrite(s_updates, &ready);
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED);
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED);
}
static bool wifi_init(void)
{
    const char *stage = "nvs_flash_init";
    esp_err_t err = nvs_flash_init(); /* Never erase the default NVS partition. */
    if (err != ESP_OK) goto fail;
    stage = "esp_netif_init";
    if ((err = esp_netif_init()) != ESP_OK) goto fail;
    stage = "esp_event_loop_create_default";
    if ((err = esp_event_loop_create_default()) != ESP_OK) goto fail;
    stage = "esp_netif_create_default_wifi_sta";
    if (!esp_netif_create_default_wifi_sta()) { err = ESP_FAIL; goto fail; }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    stage = "esp_wifi_init";
    if ((err = esp_wifi_init(&config)) != ESP_OK) goto fail;
    stage = "register Wi-Fi event handler";
    if ((err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL)) != ESP_OK) goto fail;
    stage = "register IP event handler";
    if ((err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL)) != ESP_OK) goto fail;
    stage = "esp_wifi_set_storage";
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK) goto fail;
    stage = "esp_wifi_set_mode";
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) goto fail;
    stage = "esp_wifi_start";
    if ((err = esp_wifi_start()) != ESP_OK) goto fail;
    return true;
fail:
    ESP_LOGE(TAG, "Network initialization failed at %s: %s", stage, esp_err_to_name(err));
    ESP_LOGE(TAG, "Heap at failure: internal free %u, internal largest %u, DMA free %u, DMA largest %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    return false;
}
static bool wifi_connected(void)
{
    return (xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED) != 0;
}
static bool apply_wifi(const app_settings_t *settings)
{
    (void)esp_wifi_disconnect();
    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED);
    if (!settings->ssid[0]) return true;
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, settings->ssid, strlen(settings->ssid));
    memcpy(config.sta.password, settings->password, strlen(settings->password));
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    memset(&config, 0, sizeof(config));
    return err == ESP_OK;
}
static bool connect_wifi(void)
{
    if (wifi_connected()) return true;
    if (esp_wifi_connect() != ESP_OK) return false;
    return (xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED, pdFALSE, pdFALSE,
                               pdMS_TO_TICKS(15000)) & WIFI_CONNECTED) != 0;
}
static bool synchronize_clock(void)
{
    static bool started, synchronized;
    if (synchronized) return true;
    if (!started) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        if (esp_netif_sntp_init(&config) != ESP_OK) return false;
        started = true;
    }
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) == ESP_OK) {
        synchronized = true;
        return true;
    }
    return false;
}
static void *json_alloc(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static char *http_get(const char *url)
{
    char *response = heap_caps_malloc(RESPONSE_LIMIT + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!response) return NULL;
    esp_http_client_config_t config = {.url = url, .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { free(response); return NULL; }
    bool success = false;
    if (esp_http_client_open(client, 0) != ESP_OK) goto cleanup;
    int64_t length = esp_http_client_fetch_headers(client);
    if (length < 0 || length > RESPONSE_LIMIT || esp_http_client_get_status_code(client) != 200) goto cleanup;
    size_t used = 0;
    while (used < RESPONSE_LIMIT) {
        int n = esp_http_client_read(client, response + used, (int)(RESPONSE_LIMIT - used));
        if (n < 0) goto cleanup;
        if (!n) break;
        used += (size_t)n;
    }
    if (!esp_http_client_is_complete_data_received(client)) goto cleanup;
    response[used] = 0;
    success = true;
cleanup:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (!success) { free(response); return NULL; }
    return response;
}
static bool url_encode(char *out, size_t capacity, const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (n + (plain ? 1 : 3) >= capacity) return false;
        if (plain) out[n++] = (char)c;
        else { out[n++] = '%'; out[n++] = hex[c >> 4]; out[n++] = hex[c & 15]; }
    }
    out[n] = 0;
    return true;
}
static bool fetch_departures(const char *station_id, departures_station_t *out)
{
    char encoded[APP_STATION_ID_SIZE * 3], url[256];
    if (!url_encode(encoded, sizeof(encoded), station_id)) return false;
    snprintf(url, sizeof(url), "https://v6.bvg.transport.rest/stops/%s/departures?remarks=false&results=32", encoded);
    char *body = http_get(url);
    if (!body) return false;
    bool ok = departures_parse(body, out);
    free(body);
    return ok;
}
static void scan_networks(void)
{
    app_service_result_t result = {.type = APP_RESULT_SCAN};
    wifi_scan_config_t config = {.show_hidden = false};
    if (esp_wifi_scan_start(&config, true) == ESP_OK) {
        wifi_ap_record_t aps[APP_SEARCH_RESULTS_MAX];
        uint16_t count = APP_SEARCH_RESULTS_MAX;
        if (esp_wifi_scan_get_ap_records(&count, aps) == ESP_OK) {
            result.success = true;
            for (unsigned i = 0; i < count; ++i) {
                if (!aps[i].ssid[0]) continue;
                bool duplicate = false;
                for (size_t j = 0; j < result.count; j++)
                    if (!strcmp(result.items[j].name, (char *)aps[i].ssid)) duplicate = true;
                if (duplicate) continue;
                snprintf(result.items[result.count++].name, APP_STATION_NAME_SIZE, "%s", (char *)aps[i].ssid);
            }
        }
    }
    (void)xQueueOverwrite(s_results, &result);
}
static void search_stations(const char *query)
{
    app_service_result_t result = {.type = APP_RESULT_SEARCH};
    char encoded[sizeof(((command_t *)0)->query) * 3], url[512];
    if (!wifi_connected() || !url_encode(encoded, sizeof(encoded), query)) goto done;
    snprintf(url, sizeof(url), "https://v6.bvg.transport.rest/locations?query=%s&results=12&addresses=false&poi=false", encoded);
    char *body = http_get(url);
    if (!body) goto done;
    cJSON *root = cJSON_ParseWithOpts(body, NULL, true);
    free(body);
    if (!cJSON_IsArray(root)) { cJSON_Delete(root); goto done; }
    result.success = true;
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(item, "id");
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");
        if (!cJSON_IsString(id) || !cJSON_IsString(name) || !cJSON_IsString(type) ||
            (strcmp(type->valuestring, "stop") && strcmp(type->valuestring, "station")) ||
            !id->valuestring[0] || !name->valuestring[0] ||
            strlen(id->valuestring) >= APP_STATION_ID_SIZE || strlen(name->valuestring) >= APP_STATION_NAME_SIZE) continue;
        app_station_t *station = &result.items[result.count++];
        strcpy(station->id, id->valuestring);
        strcpy(station->name, name->valuestring);
        if (result.count == APP_SEARCH_RESULTS_MAX) break;
    }
    cJSON_Delete(root);
done:
    (void)xQueueOverwrite(s_results, &result);
}
static void service_task(void *arg)
{
    (void)arg;
    typedef struct {
        departures_station_t stations[DEPARTURES_STATION_COUNT], fresh;
        bool valid[DEPARTURES_STATION_COUNT], stale[DEPARTURES_STATION_COUNT];
    } state_t;
    state_t *state = heap_caps_calloc(1, sizeof(*state), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    departures_update_t *update = heap_caps_calloc(1, sizeof(*update), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!state || !update) {
        ESP_LOGE(TAG, "Cannot allocate departure service state");
        xSemaphoreTake(s_snapshot_lock, portMAX_DELAY);
        memset(s_snapshot, 0, sizeof(*s_snapshot));
        s_snapshot->status = DEPARTURES_ERROR;
        xSemaphoreGive(s_snapshot_lock);
        bool ready = true;
        (void)xQueueOverwrite(s_updates, &ready);
        free(state);
        free(update);
        vTaskDelete(NULL);
        return;
    }
    if (!wifi_init()) { publish(update, DEPARTURES_ERROR); goto end; }
    xEventGroupSetBits(s_wifi_events, WIFI_READY);
    cJSON_Hooks hooks = {.malloc_fn = json_alloc, .free_fn = free};
    cJSON_InitHooks(&hooks);
    app_settings_t settings;
    app_settings_get(&settings);
    (void)apply_wifi(&settings);
    unsigned wait_seconds = 0, retry_seconds = 15;
    bool was_connected = false;
    for (;;) {
        uint32_t notification = 0;
        (void)xTaskNotifyWait(0, UINT32_MAX, &notification, 0);
        if (notification & SETTINGS_CHANGED) {
            app_settings_get(&settings);
            (void)apply_wifi(&settings);
            memset(state, 0, sizeof(*state));
            memset(update, 0, sizeof(*update));
            wait_seconds = 0; retry_seconds = 15;
            was_connected = false;
        }
        command_t command;
        while (xQueueReceive(s_commands, &command, 0) == pdTRUE) {
            if (command.type == CMD_SCAN) scan_networks();
            else search_stations(command.query);
        }
        if (!wait_seconds) {
            if (!settings.ssid[0]) {
                publish(update, DEPARTURES_NO_CONFIG);
                wait_seconds = 5;
            } else {
                publish(update, DEPARTURES_CONNECTING);
                bool connected = connect_wifi() && synchronize_clock();
                was_connected = wifi_connected();
                bool all_fresh = connected, any_fresh = false, any_valid = false;
                for (unsigned i = 0; i < settings.station_count; ++i) {
                    bool fresh = connected && wifi_connected() && fetch_departures(settings.stations[i].id, &state->fresh);
                    if (fresh) {
                        state->stations[i] = state->fresh; state->valid[i] = true; any_fresh = true;
                        update->last_updated = (int64_t)time(NULL);
                    }
                    else all_fresh = false;
                    state->stale[i] = !fresh;
                    any_valid |= state->valid[i];
                    update->station_valid[i] = state->valid[i];
                    update->station_stale[i] = state->stale[i];
                }
                departures_merge(state->stations, state->stale, &update->departures);
                publish(update, all_fresh ? DEPARTURES_LIVE : !any_valid ? DEPARTURES_ERROR :
                                any_fresh ? DEPARTURES_PARTIAL : DEPARTURES_STALE);
                wait_seconds = all_fresh ? REFRESH_SECONDS : retry_seconds;
                retry_seconds = all_fresh ? 15 : (retry_seconds < 150 ? retry_seconds * 2 : 300);
            }
        }
        if (was_connected && !wifi_connected()) {
            was_connected = false;
            for (unsigned i = 0; i < settings.station_count; ++i) {
                state->stale[i] = true;
                update->station_stale[i] = true;
            }
            departures_merge(state->stations, state->stale, &update->departures);
            publish(update, DEPARTURES_STALE);
            wait_seconds = 0;
        }
        if (wait_seconds) --wait_seconds;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
end:
    free(update); free(state); vTaskDelete(NULL);
}
bool departures_service_start(void)
{
    s_wifi_events = xEventGroupCreate();
    s_snapshot_lock = xSemaphoreCreateMutex();
    s_updates = xQueueCreate(1, sizeof(bool));
    s_commands = xQueueCreate(8, sizeof(command_t));
    s_results = xQueueCreate(1, sizeof(app_service_result_t));
    s_snapshot = heap_caps_calloc(1, sizeof(*s_snapshot), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_wifi_events || !s_snapshot_lock || !s_updates || !s_commands || !s_results || !s_snapshot) return false;
    return xTaskCreate(service_task, "departures", 16384, NULL, 4, &s_task) == pdPASS;
}
bool departures_service_receive(departures_update_t *update)
{
    bool ready;
    if (!update || !s_updates || xQueueReceive(s_updates, &ready, 0) != pdTRUE) return false;
    xSemaphoreTake(s_snapshot_lock, portMAX_DELAY);
    *update = *s_snapshot;
    xSemaphoreGive(s_snapshot_lock);
    return true;
}
bool departures_service_scan(void)
{
    command_t command = {.type = CMD_SCAN};
    return s_wifi_events && (xEventGroupGetBits(s_wifi_events) & WIFI_READY) &&
           s_commands && xQueueSend(s_commands, &command, 0) == pdTRUE;
}
bool departures_service_search(const char *query)
{
    command_t command = {.type = CMD_SEARCH};
    if (!query || !*query || strlen(query) >= sizeof(command.query)) return false;
    strcpy(command.query, query);
    return s_wifi_events && (xEventGroupGetBits(s_wifi_events) & WIFI_READY) &&
           s_commands && xQueueSend(s_commands, &command, 0) == pdTRUE;
}
void departures_service_settings_changed(void)
{
    if (s_wifi_events && (xEventGroupGetBits(s_wifi_events) & WIFI_READY) && s_task)
        (void)xTaskNotify(s_task, SETTINGS_CHANGED, eSetBits);
}
bool departures_service_receive_result(app_service_result_t *result)
{
    return result && s_results && xQueueReceive(s_results, result, 0) == pdTRUE;
}
