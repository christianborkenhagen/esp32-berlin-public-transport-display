#include "departures_service.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "departures_merge.h"
#include "departures_parser.h"
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
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#if !CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#error "The live departure service requires the ESP-IDF certificate bundle"
#endif

#include "wifi_config.h"

#define API_URL_FORMAT "https://v6.bvg.transport.rest/stops/%s/departures?remarks=false&results=8"
#define RESPONSE_LIMIT (32 * 1024)
#define HTTP_TIMEOUT_MS 15000
#define REFRESH_SECONDS 60
#define MAX_RETRY_SECONDS 300
#define WIFI_CONNECTED BIT0

static const char *TAG = "departures";
static QueueHandle_t s_updates;
static EventGroupHandle_t s_wifi_events;

static void publish(departures_update_t *update, departures_status_t status)
{
    update->status = status;
    (void)xQueueOverwrite(s_updates, update);
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED);
        ESP_LOGW(TAG, "Wi-Fi disconnected");
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED);
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
}

static bool wifi_check(esp_err_t err, const char *step)
{
    if (err == ESP_OK) return true;
    ESP_LOGE(TAG, "%s failed: %s", step, esp_err_to_name(err));
    return false;
}

static bool wifi_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        /* Do not erase unrelated user data automatically. */
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
        return false;
    }
    if (!wifi_check(esp_netif_init(), "Network interface initialization") ||
        !wifi_check(esp_event_loop_create_default(), "Event loop creation")) return false;
    if (!esp_netif_create_default_wifi_sta()) {
        ESP_LOGE(TAG, "Wi-Fi station interface creation failed");
        return false;
    }
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGI(TAG, "DMA heap before Wi-Fi: free %u, largest %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    err = esp_wifi_init(&init);
    if (!wifi_check(err, "Wi-Fi driver initialization") ||
        !wifi_check(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL),
                    "Wi-Fi event registration") ||
        !wifi_check(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL),
                    "IP event registration")) {
        return false;
    }
    wifi_config_t config = {0};
    size_t ssid_len = strlen(APP_WIFI_SSID);
    size_t password_len = strlen(APP_WIFI_PASSWORD);
    if (ssid_len == 0 || ssid_len > sizeof(config.sta.ssid) ||
        password_len > sizeof(config.sta.password)) {
        ESP_LOGE(TAG, "Invalid Wi-Fi credential lengths");
        return false;
    }
    memcpy(config.sta.ssid, APP_WIFI_SSID, ssid_len);
    memcpy(config.sta.password, APP_WIFI_PASSWORD, password_len);
    /* RAM only: the credentials are not written to NVS by this application. */
    return wifi_check(esp_wifi_set_storage(WIFI_STORAGE_RAM), "Wi-Fi storage setup") &&
           wifi_check(esp_wifi_set_mode(WIFI_MODE_STA), "Wi-Fi mode setup") &&
           wifi_check(esp_wifi_set_config(WIFI_IF_STA, &config), "Wi-Fi configuration") &&
           wifi_check(esp_wifi_start(), "Wi-Fi start");
}

static bool wifi_connected(void)
{
    return (xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED) != 0;
}

static bool connect_wifi(void)
{
    if (wifi_connected()) return true;
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi connection start failed: %s", esp_err_to_name(err));
        return false;
    }
    return (xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED, pdFALSE, pdFALSE,
                                pdMS_TO_TICKS(15000)) & WIFI_CONNECTED) != 0;
}

static bool synchronize_clock(void)
{
    static bool started;
    static bool synchronized;
    if (synchronized) return true;
    if (!started) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_err_t err = esp_netif_sntp_init(&config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SNTP initialization failed: %s", esp_err_to_name(err));
            return false;
        }
        started = true;
    }
    for (int i = 0; i < 20 && wifi_connected(); ++i) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(1000)) == ESP_OK) {
            synchronized = true;
            ESP_LOGI(TAG, "Clock synchronized");
            return true;
        }
    }
    ESP_LOGW(TAG, "Waiting for network time");
    return false;
}

static void *json_alloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void log_worker_stack_margin(void)
{
    enum { TASK_SNAPSHOT_CAPACITY = 48 };
    TaskStatus_t *tasks = heap_caps_malloc(
        TASK_SNAPSHOT_CAPACITY * sizeof(*tasks), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tasks) return;
    UBaseType_t count = uxTaskGetSystemState(tasks, TASK_SNAPSHOT_CAPACITY, NULL);
    unsigned workers = 0;
    unsigned minimum = (unsigned)-1;
    for (UBaseType_t i = 0; i < count; ++i) {
        if (strcmp(tasks[i].pcTaskName, "swdraw") == 0) {
            unsigned free_stack = (unsigned)tasks[i].usStackHighWaterMark;
            if (free_stack < minimum) minimum = free_stack;
            ++workers;
        }
    }
    if (workers) {
        ESP_LOGI(TAG, "LVGL workers: %u, minimum free stack: %u bytes",
                 workers, minimum);
    } else {
        ESP_LOGW(TAG, "Could not observe LVGL draw workers");
    }
    heap_caps_free(tasks);
}

static bool fetch_departures(unsigned station, departures_station_t *out)
{
    log_worker_stack_margin();
    ESP_LOGI(TAG, "Internal heap before HTTPS: free %u, largest %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "DMA heap before HTTPS: free %u, largest %u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    bool success = false;
    char url[160];
    snprintf(url, sizeof(url), API_URL_FORMAT, departures_station_id(station));
    char *response = heap_caps_malloc(RESPONSE_LIMIT + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!response) {
        ESP_LOGE(TAG, "Could not allocate response buffer");
        return false;
    }
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) goto done;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTPS connection failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    int64_t content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (content_length < 0 || content_length > RESPONSE_LIMIT || status != 200) {
        ESP_LOGW(TAG, "HTTP response rejected: status=%d length=%lld",
                 status, (long long)content_length);
        goto cleanup;
    }
    size_t used = 0;
    while (used < RESPONSE_LIMIT) {
        int n = esp_http_client_read(client, response + used, (int)(RESPONSE_LIMIT - used));
        if (n < 0) {
            ESP_LOGW(TAG, "HTTP response read failed: %d", n);
            goto cleanup;
        }
        if (n == 0) break;
        used += (size_t)n;
    }
    if (!esp_http_client_is_complete_data_received(client)) {
        ESP_LOGW(TAG, "HTTP response incomplete or exceeds %u bytes", RESPONSE_LIMIT);
        goto cleanup;
    }
    response[used] = '\0';
    if (!departures_parse(response, out)) {
        ESP_LOGW(TAG, "Invalid departures response");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Station %s: received %u departures (%u bytes)",
             departures_station_id(station), (unsigned)out->count, (unsigned)used);
    success = true;
cleanup:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
done:
    free(response);
    return success;
}

static void service_task(void *arg)
{
    (void)arg;
    typedef struct {
        departures_station_t stations[DEPARTURES_STATION_COUNT];
        departures_station_t fresh;
        bool valid[DEPARTURES_STATION_COUNT];
        bool stale[DEPARTURES_STATION_COUNT];
    } service_state_t;
    service_state_t *state = heap_caps_calloc(1, sizeof(*state), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    departures_update_t *update = heap_caps_calloc(1, sizeof(*update), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!state || !update) {
        ESP_LOGE(TAG, "Could not allocate departure snapshots");
        if (update) publish(update, DEPARTURES_ERROR);
        free(update);
        free(state);
        vTaskDelete(NULL);
        return;
    }
    if (strlen(APP_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "Configure src/wifi_config.h before building");
        publish(update, DEPARTURES_NO_CONFIG);
        free(update);
        free(state);
        vTaskDelete(NULL);
        return;
    }
    publish(update, DEPARTURES_CONNECTING);
    if (!wifi_init()) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed; see the preceding error");
        publish(update, DEPARTURES_ERROR);
        free(update);
        free(state);
        vTaskDelete(NULL);
        return;
    }
    /* cJSON is used only by this service; keep its parse tree in PSRAM. */
    cJSON_Hooks hooks = {.malloc_fn = json_alloc, .free_fn = free};
    cJSON_InitHooks(&hooks);
    unsigned retry_seconds = 15;
    while (true) {
        bool connected = connect_wifi() && synchronize_clock();
        bool all_fresh = connected;
        bool any_fresh = false;
        bool any_valid = false;
        for (unsigned i = 0; i < DEPARTURES_STATION_COUNT; ++i) {
            bool fresh = connected && wifi_connected() && fetch_departures(i, &state->fresh);
            if (fresh) {
                state->stations[i] = state->fresh;
                state->valid[i] = true;
                any_fresh = true;
            } else {
                all_fresh = false;
                ESP_LOGW(TAG, "Station %s: fetch failed", departures_station_id(i));
            }
            state->stale[i] = !fresh;
            any_valid = any_valid || state->valid[i];
        }
        departures_merge(state->stations, state->stale, &update->departures);
        if (all_fresh) {
            publish(update, DEPARTURES_LIVE);
            retry_seconds = 15;
        } else {
            publish(update, !any_valid ? DEPARTURES_ERROR :
                            any_fresh ? DEPARTURES_PARTIAL : DEPARTURES_STALE);
        }
        unsigned wait_seconds = all_fresh ? REFRESH_SECONDS : retry_seconds;
        if (!all_fresh && retry_seconds < MAX_RETRY_SECONDS) {
            retry_seconds *= 2;
            if (retry_seconds > MAX_RETRY_SECONDS) retry_seconds = MAX_RETRY_SECONDS;
        }
        for (unsigned i = 0; i < wait_seconds; ++i) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (all_fresh && !wifi_connected()) {
                publish(update, DEPARTURES_STALE);
                break;
            }
        }
    }
}

bool departures_service_start(void)
{
    s_updates = xQueueCreate(1, sizeof(departures_update_t));
    s_wifi_events = xEventGroupCreate();
    if (!s_updates || !s_wifi_events) return false;
    return xTaskCreate(service_task, "departures", 8192, NULL, 4, NULL) == pdPASS;
}

bool departures_service_receive(departures_update_t *update)
{
    return update && s_updates && xQueueReceive(s_updates, update, 0) == pdTRUE;
}
