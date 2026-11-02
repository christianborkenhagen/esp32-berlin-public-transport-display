#ifndef DEPARTURES_SERVICE_H
#define DEPARTURES_SERVICE_H

#include <stdbool.h>
#include "departures.h"

/* Called once before the LVGL task is started. */
bool departures_service_start(void);
/* Non-blocking; call from the LVGL task only. */
bool departures_service_receive(departures_update_t *update);

#endif
