#pragma once
#include <stddef.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 100
#define ESP_ERR_NVS_INVALID_STATE 101
#define NVS_READONLY 0
#define NVS_READWRITE 1
const char *esp_err_to_name(esp_err_t err);
esp_err_t nvs_open_from_partition(const char *partition, const char *name, int mode, nvs_handle_t *out);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out, size_t *len);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t len);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
