#include "lvgl_psram_pool.h"
#include <stdlib.h>
#include "esp_heap_caps.h"

static void *s_pool;
static size_t s_pool_size;

bool app_lvgl_reserve_pool(size_t size)
{
    if (!size || s_pool) return false;
    void *pool = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pool) return false;
    s_pool = pool;
    s_pool_size = size;
    return true;
}

void *app_lvgl_get_pool(size_t size)
{
    /* A configuration mismatch must not turn into a NULL LVGL pool. */
    if (!s_pool || size != s_pool_size) abort();
    return s_pool;
}
