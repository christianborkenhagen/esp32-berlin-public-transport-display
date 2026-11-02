#include "lvgl_psram_pool.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char pool[32768];
static bool allocation_fails;
static unsigned allocations;

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    assert(size == sizeof(pool));
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    allocations++;
    return allocation_fails ? NULL : pool;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    allocation_fails = !strcmp(argv[1], "fail");
    assert(allocation_fails || !strcmp(argv[1], "success"));
    assert(!app_lvgl_reserve_pool(0));
    assert(allocations == 0);
    if (allocation_fails) {
        assert(!app_lvgl_reserve_pool(sizeof(pool)));
        assert(allocations == 1);
    } else {
        assert(app_lvgl_reserve_pool(sizeof(pool)));
        assert(app_lvgl_get_pool(sizeof(pool)) == pool);
        assert(!app_lvgl_reserve_pool(sizeof(pool)));
        assert(allocations == 1);
    }
    puts("LVGL PSRAM pool test passed");
    return 0;
}
