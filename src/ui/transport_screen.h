#ifndef TRANSPORT_SCREEN_H
#define TRANSPORT_SCREEN_H
#include "lvgl.h"
#include "departures.h"
lv_obj_t *transport_screen_show_live(void);
departures_update_t *transport_screen_buffer(void);
void transport_screen_set_departures(const departures_update_t *update);
void transport_screen_poll(void);
#endif
