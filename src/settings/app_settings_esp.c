#include "app_settings.h"
#include <string.h>
#include "esp_efuse.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_sec_provider.h"
#include "sdkconfig.h"

#if CONFIG_MBEDTLS_HARDWARE_AES
#error "Encrypted settings require software AES; disable CONFIG_MBEDTLS_HARDWARE_AES"
#endif

#define SETTINGS_PARTITION "app_settings"
static const char *TAG = "settings";
static app_settings_t s_settings;
static SemaphoreHandle_t s_lock;
static bool s_available;

bool app_settings_init(void)
{
    s_available = false;
    app_settings_defaults(&s_settings);
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;
    if (!esp_efuse_get_key_dis_read(EFUSE_BLK_KEY0) ||
        !esp_efuse_get_key_dis_write(EFUSE_BLK_KEY0)) {
        ESP_LOGE(TAG, "KEY0 is not read- and write-protected; refusing settings access");
        return false;
    }
    nvs_sec_config_hmac_t config = {.hmac_key_id = HMAC_KEY0};
    nvs_sec_scheme_t *scheme = NULL;
    nvs_sec_cfg_t keys = {0};
    const char *stage = "register HMAC provider";
    esp_err_t err = nvs_sec_provider_register_hmac(&config, &scheme);
    /* Read only: generate_keys_v2 can irreversibly burn an eFuse. */
    if (err == ESP_OK) {
        stage = "derive HMAC keys";
        err = nvs_flash_read_security_cfg_v2(scheme, &keys);
    }
    if (err == ESP_OK) {
        stage = "initialize encrypted partition";
        err = nvs_flash_secure_init_partition(SETTINGS_PARTITION, &keys);
    }
    memset(&keys, 0, sizeof(keys));
    /* The provider registers a global copy referencing scheme_data. Keep it
       alive for the process lifetime instead of leaving a dangling pointer. */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cannot %s: %s", stage, esp_err_to_name(err));
        return false;
    }
    nvs_handle_t handle;
    err = nvs_open_from_partition(SETTINGS_PARTITION, "app", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        app_settings_t loaded;
        size_t len = sizeof(loaded);
        err = nvs_get_blob(handle, "settings_v1", &loaded, &len);
        nvs_close(handle);
        if (err == ESP_OK && len == sizeof(loaded) && app_settings_valid(&loaded)) {
            s_settings = loaded;
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            if (err != ESP_OK)
                ESP_LOGE(TAG, "Cannot read saved settings: %s", esp_err_to_name(err));
            else
                ESP_LOGE(TAG, "Invalid saved settings; refusing to overwrite them");
            return false;
        }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Cannot open settings: %s", esp_err_to_name(err));
        return false;
    }
    s_available = true;
    return true;
}

bool app_settings_available(void)
{
    if (!s_lock) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool available = s_available;
    xSemaphoreGive(s_lock);
    return available;
}
void app_settings_get(app_settings_t *out)
{
    if (!out) return;
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_settings;
    if (s_lock) xSemaphoreGive(s_lock);
}

bool app_settings_save(const app_settings_t *settings)
{
    if (!s_lock || !app_settings_valid(settings)) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_available) {
        xSemaphoreGive(s_lock);
        return false;
    }
    nvs_handle_t handle;
    const char *stage = "open namespace for writing";
    esp_err_t err = nvs_open_from_partition(SETTINGS_PARTITION, "app", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        stage = "write settings blob";
        err = nvs_set_blob(handle, "settings_v1", settings, sizeof(*settings));
        if (err == ESP_OK) {
            stage = "commit settings";
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (err == ESP_OK) s_settings = *settings;
    else {
        /* A failed NVS operation may have left flash in an uncertain state.
           Do not make another write attempt in this session. */
        s_available = false;
        ESP_LOGE(TAG, "Cannot %s: %s; further settings writes disabled", stage, esp_err_to_name(err));
        ESP_LOGE(TAG, "Heap at failure: internal free %u, DMA free %u, DMA largest %u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    }
    xSemaphoreGive(s_lock);
    return err == ESP_OK;
}
