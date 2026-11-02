#pragma once
#include "nvs.h"
typedef struct { unsigned char bytes[64]; } nvs_sec_cfg_t;
typedef struct { int unused; } nvs_sec_scheme_t;
esp_err_t nvs_flash_read_security_cfg_v2(nvs_sec_scheme_t *scheme, nvs_sec_cfg_t *keys);
esp_err_t nvs_flash_secure_init_partition(const char *label, nvs_sec_cfg_t *keys);
