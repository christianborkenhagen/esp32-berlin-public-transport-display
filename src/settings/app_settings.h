#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H
#include <stdbool.h>
#include <stddef.h>

#define APP_MAX_STATIONS 4
#define APP_SSID_SIZE 33
#define APP_PASSWORD_SIZE 65
#define APP_STATION_ID_SIZE 32
#define APP_STATION_NAME_SIZE 96

typedef struct {
    char id[APP_STATION_ID_SIZE];
    char name[APP_STATION_NAME_SIZE];
} app_station_t;

typedef struct {
    char ssid[APP_SSID_SIZE];
    char password[APP_PASSWORD_SIZE];
    size_t station_count;
    app_station_t stations[APP_MAX_STATIONS];
} app_settings_t;

bool app_settings_init(void);
void app_settings_defaults(app_settings_t *out);
void app_settings_get(app_settings_t *out);
bool app_settings_save(const app_settings_t *settings);
bool app_settings_available(void);
bool app_settings_valid(const app_settings_t *settings);
#endif
