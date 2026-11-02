#ifndef DEPARTURES_SERVICE_H
#define DEPARTURES_SERVICE_H

#include <stdbool.h>
#include "departures.h"
#include "app_settings.h"

#define APP_SEARCH_RESULTS_MAX 12
typedef enum { APP_RESULT_SCAN, APP_RESULT_SEARCH } app_result_type_t;
typedef struct {
    app_result_type_t type;
    bool success;
    size_t count;
    app_station_t items[APP_SEARCH_RESULTS_MAX];
} app_service_result_t;

/* Called once before the LVGL task is started. */
bool departures_service_start(void);
/* Non-blocking; call from the LVGL task only. */
bool departures_service_receive(departures_update_t *update);
bool departures_service_scan(void);
bool departures_service_search(const char *query);
void departures_service_settings_changed(void);
bool departures_service_receive_result(app_service_result_t *result);

#endif
