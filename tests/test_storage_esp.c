#include "app_settings.h"
#include "nvs_sec_provider.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int key_missing, key_derivation_bad, partition_bad, corrupt;
static int open_fails, write_fails, commit_fails, opens, writes, commits, closes;
static app_settings_t stored, staged;
static int have_blob;
bool esp_efuse_get_key_dis_read(int block) { assert(block == 0); return !key_missing; }
bool esp_efuse_get_key_dis_write(int block) { assert(block == 0); return !key_missing; }
const char *esp_err_to_name(esp_err_t err)
{
    return err == ESP_ERR_NVS_INVALID_STATE ? "ESP_ERR_NVS_INVALID_STATE" : "test error";
}
esp_err_t nvs_sec_provider_register_hmac(const nvs_sec_config_hmac_t *config, nvs_sec_scheme_t **out)
{
    static nvs_sec_scheme_t scheme;
    assert(config->hmac_key_id == HMAC_KEY0); *out = &scheme; return ESP_OK;
}
esp_err_t nvs_flash_read_security_cfg_v2(nvs_sec_scheme_t *s, nvs_sec_cfg_t *keys)
{
    assert(s && keys); memset(keys, 1, sizeof(*keys)); return key_derivation_bad ? ESP_FAIL : ESP_OK;
}
esp_err_t nvs_flash_secure_init_partition(const char *label, nvs_sec_cfg_t *keys)
{
    assert(!strcmp(label, "app_settings") && keys && keys->bytes[0] == 1);
    return partition_bad ? ESP_FAIL : ESP_OK;
}
esp_err_t nvs_open_from_partition(const char *partition, const char *name, int mode, nvs_handle_t *out)
{
    assert(!strcmp(partition, "app_settings") && !strcmp(name, "app"));
    if (mode == NVS_READONLY && !have_blob && !corrupt) return ESP_ERR_NVS_NOT_FOUND;
    if (mode == NVS_READWRITE) {
        opens++;
        if (open_fails) return ESP_ERR_NVS_INVALID_STATE;
    }
    *out = 1; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len)
{
    assert(h && !strcmp(key, "settings_v1"));
    if (corrupt) { *len = 1; return ESP_OK; }
    assert(*len == sizeof(stored)); memcpy(out, &stored, *len); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t len)
{
    assert(h && !strcmp(key, "settings_v1") && len == sizeof(staged));
    writes++;
    if (write_fails) return ESP_ERR_NVS_INVALID_STATE;
    memcpy(&staged, value, len); return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h)
{
    assert(h); commits++; if (commit_fails) return ESP_ERR_NVS_INVALID_STATE;
    stored = staged; have_blob = 1; return ESP_OK;
}
void nvs_close(nvs_handle_t h) { assert(h); closes++; }
int main(int argc, char **argv)
{
    assert(argc == 2);
    key_missing = !strcmp(argv[1], "missing-key");
    key_derivation_bad = !strcmp(argv[1], "bad-key");
    partition_bad = !strcmp(argv[1], "bad-partition");
    corrupt = !strcmp(argv[1], "corrupt");
    open_fails = !strcmp(argv[1], "open-fails");
    write_fails = !strcmp(argv[1], "write-fails");
    commit_fails = !strcmp(argv[1], "commit-fails");
    assert(key_missing || key_derivation_bad || partition_bad || corrupt ||
           open_fails || write_fails || commit_fails || !strcmp(argv[1], "success"));
    if (open_fails || write_fails || commit_fails) {
        app_settings_defaults(&stored);
        strcpy(stored.ssid, "previous");
        have_blob = 1;
    }
    bool ok = app_settings_init();
    app_settings_t settings; app_settings_get(&settings);
    assert(settings.station_count == 2);
    if (key_missing || key_derivation_bad || partition_bad || corrupt) {
        assert(!ok && !app_settings_available());
        assert(!app_settings_save(&settings));
        assert(writes == 0 && commits == 0);
    } else {
        assert(ok && app_settings_available());
        strcpy(settings.ssid, "test");
        app_settings_t actual;
        if (open_fails || write_fails || commit_fails) {
            assert(!app_settings_save(&settings));
            assert(!app_settings_available());
            app_settings_get(&actual); assert(!strcmp(actual.ssid, "previous"));
            assert(opens == 1);
            assert(writes == !open_fails);
            assert(commits == !!commit_fails);
            assert(closes == 1 + !open_fails); /* Includes the initial read handle. */
            /* A failure never causes an automatic retry or a subsequent write. */
            assert(!app_settings_save(&settings));
            assert(opens == 1 && writes == !open_fails && commits == !!commit_fails);
        } else {
            assert(app_settings_save(&settings));
            app_settings_get(&actual); assert(!strcmp(actual.ssid, "test"));
            assert(have_blob && !strcmp(stored.ssid, "test"));
            /* Reinitialize from fake NVS contents to exercise the reboot load path. */
            assert(app_settings_init());
            app_settings_get(&actual); assert(!strcmp(actual.ssid, "test"));
        }
    }
    puts("Encrypted storage fail-closed test passed"); return 0;
}
