#pragma once
#include "nvs_flash.h"
#define HMAC_KEY0 0
typedef struct { int hmac_key_id; } nvs_sec_config_hmac_t;
esp_err_t nvs_sec_provider_register_hmac(const nvs_sec_config_hmac_t *config, nvs_sec_scheme_t **scheme);
