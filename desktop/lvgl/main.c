#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "lvgl.h"
#include "benchmark_screen.h"
#include "transport_screen.h"

static uint32_t s_refresh_count;

static uint32_t get_refresh_count(void)
{
    return s_refresh_count;
}

static void refresh_cb(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_target(event);
    if (lv_display_flush_is_last(display)) s_refresh_count++;
}

static void close_smoke_test(lv_timer_t *timer)
{
    HWND window = lv_timer_get_user_data(timer);
    PostMessageW(window, WM_CLOSE, 0, 0);
}

static void open_transport(lv_event_t *event)
{
    (void)event;
    transport_screen_show(lv_screen_active());
}

static void create_menu(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101923), 0);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "LVGL Windows simulator");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -60);
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 260, 60);
    lv_obj_center(button);
    lv_obj_add_event_cb(button, open_transport, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "Transport screen");
    lv_obj_center(label);
}

int main(int argc, char **argv)
{
    bool benchmark = false;
    bool transport = false;
    bool live_demo = false;
    bool smoke_test = false;
    int scene = -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--benchmark") == 0) benchmark = true;
        else if (strcmp(argv[i], "--transport") == 0) transport = true;
        else if (strcmp(argv[i], "--transport-live-demo") == 0) live_demo = true;
        else if (strcmp(argv[i], "--smoke-test") == 0) smoke_test = true;
        else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            char *end;
            long parsed = strtol(argv[++i], &end, 10);
            if (*end || end == argv[i] || parsed < 0 || parsed > 7) {
                fprintf(stderr, "--scene must be an integer from 0 to 7\n");
                return 2;
            }
            scene = (int)parsed;
        }
        else {
            fprintf(stderr, "Usage: lvgl_pc [--transport|--transport-live-demo|--benchmark] [--scene 0-7] [--smoke-test]\n");
            return 2;
        }
    }
    if ((int)benchmark + (int)transport + (int)live_demo > 1 || (scene >= 0 && !benchmark)) {
        fprintf(stderr, "Choose one screen; --scene requires --benchmark\n");
        return 2;
    }
    lv_init();
    lv_display_t *display = lv_windows_create_display(
        L"LVGL simulator - 800x480", 800, 480, 100, false, true);
    if (!display || !lv_windows_acquire_pointer_indev(display)) {
        fprintf(stderr, "Unable to create LVGL display or pointer input\n");
        return 1;
    }
    HWND window = lv_windows_get_display_window_handle(display);
    /*
     * The Windows driver allocates its framebuffer in an LVGL timer, after
     * create_display returns. Build the screen only once that buffer exists:
     * otherwise its first refresh can be discarded before layout, leaving a
     * static scene with no visible area to invalidate.
     */
    DWORD start_ms = GetTickCount();
    for (;;) {
        lv_lock();
        bool ready = lv_display_get_buf_active(display) != NULL;
        lv_unlock();
        if (ready) break;
        if (!IsWindow(window) || GetTickCount() - start_ms > 5000) {
            fprintf(stderr, "Timed out waiting for the Windows framebuffer\n");
            return 1;
        }
        lv_timer_handler();
        Sleep(5);
    }
    lv_lock();
    if (benchmark) {
        lv_display_add_event_cb(display, refresh_cb, LV_EVENT_FLUSH_START, NULL);
        if (scene >= 0) benchmark_screen_set_initial_scene((unsigned)scene);
        benchmark_screen_create(get_refresh_count);
    } else {
        create_menu();
        if (transport) transport_screen_show(lv_screen_active());
        if (live_demo) {
            lv_obj_t *list = transport_screen_show_live();
            departures_update_t update = {.status = DEPARTURES_DEMO};
            update.departures.count = 12;
            const char *directions[] = {"S+U Warschauer Str.", "Kniprodestr./Danziger Str.", "Example cancellation"};
            int64_t now = (int64_t)time(NULL);
            for (size_t i = 0; i < update.departures.count; ++i) {
                departure_t *item = &update.departures.items[i];
                snprintf(item->line, sizeof(item->line), "M10");
                snprintf(item->direction, sizeof(item->direction), "%s", directions[i % 3]);
                item->when = now + (int64_t)(i + 2) * 60;
                item->station = (unsigned)(i % DEPARTURES_STATION_COUNT);
                item->planned_only = i == 1;
                item->cancelled = i == 2;
            }
            transport_screen_set_departures(&update);
            if (smoke_test) {
                lv_obj_update_layout(list);
                if (lv_obj_get_scroll_bottom(list) <= 0) {
                    fprintf(stderr, "Live departure list is not scrollable\n");
                    lv_unlock();
                    return 1;
                }
                lv_obj_scroll_to_y(list, 200, LV_ANIM_OFF);
                if (lv_obj_get_scroll_y(list) <= 0) {
                    fprintf(stderr, "Live departure list did not scroll\n");
                    lv_unlock();
                    return 1;
                }
                transport_screen_set_departures(&update);
                if (lv_obj_get_scroll_y(list) <= 0) {
                    fprintf(stderr, "Refresh unexpectedly reset the scroll position\n");
                    lv_unlock();
                    return 1;
                }
                lv_obj_scroll_to_y(list, 0, LV_ANIM_OFF);
                puts("Live departure scrolling smoke test passed");
            }
        }
    }
    if (smoke_test) {
        lv_timer_t *timer = lv_timer_create(close_smoke_test, 2000, window);
        lv_timer_set_repeat_count(timer, 1);
    }
    lv_unlock();
    while (IsWindow(window)) {
        uint32_t wait = lv_timer_handler();
        Sleep(wait > 10 ? 10 : (wait ? wait : 1));
    }
    return 0;
}
