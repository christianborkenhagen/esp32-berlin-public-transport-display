#ifndef TRANSPORT_SCREEN_H
#define TRANSPORT_SCREEN_H

#include <stdbool.h>
#include "lvgl.h"
#include "departures.h"

/* All functions must be called on the LVGL thread. */
lv_obj_t *transport_screen_show(lv_obj_t *return_screen);
lv_obj_t *transport_screen_show_live(void);
void transport_screen_set_departures(const departures_update_t *update);
void transport_screen_hide(void);
void transport_screen_set_updates_paused(bool paused);

#endif
