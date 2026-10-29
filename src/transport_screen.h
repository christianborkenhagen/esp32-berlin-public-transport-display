#ifndef TRANSPORT_SCREEN_H
#define TRANSPORT_SCREEN_H

#include <stdbool.h>
#include "lvgl.h"

/* Mock departures only: no network connection or real-time data. */
lv_obj_t *transport_screen_show(lv_obj_t *return_screen);
void transport_screen_hide(void);
void transport_screen_set_updates_paused(bool paused);

#endif
