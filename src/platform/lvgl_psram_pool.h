#ifndef LVGL_PSRAM_POOL_H
#define LVGL_PSRAM_POOL_H

#include <stdbool.h>
#include <stddef.h>

/* Reserve before lv_init; the pool belongs to LVGL for the process lifetime. */
bool app_lvgl_reserve_pool(size_t size);
/* LVGL calls this allocator during lv_init after the successful reservation. */
void *app_lvgl_get_pool(size_t size);

#endif
