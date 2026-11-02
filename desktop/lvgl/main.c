#include "lvgl.h"
#include "app_settings.h"
#include "departures_service.h"
#include "transport_screen.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
static bool s_smoke;
static lv_indev_data_t s_test_pointer;

static void test_pointer_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    *data = s_test_pointer;
}

static void test_tap(lv_indev_t *indev, int32_t x, int32_t y)
{
    s_test_pointer.point.x = x;
    s_test_pointer.point.y = y;
    s_test_pointer.state = LV_INDEV_STATE_PRESSED;
    lv_indev_read(indev);
    s_test_pointer.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(indev);
}

static bool press_widget(lv_indev_t *indev, lv_obj_t *obj)
{
    lv_area_t area;
    lv_obj_update_layout(lv_screen_active());
    lv_obj_get_coords(obj, &area);
    lv_point_t point = {(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
    lv_obj_t *hit = lv_indev_search_obj(lv_screen_active(), &point);
    if (hit != obj) {
        fprintf(stderr, "Pointer did not hit %s at (%ld,%ld)\n",
                lv_obj_get_name(obj), (long)point.x, (long)point.y);
        return false;
    }
    s_test_pointer.point = point;
    s_test_pointer.state = LV_INDEV_STATE_PRESSED;
    lv_indev_read(indev);
    return true;
}

static lv_obj_t *widget(const char *name)
{
    lv_obj_t *obj = lv_obj_find_by_name(lv_screen_active(), name);
    if (!obj) fprintf(stderr, "Missing widget: %s\n", name);
    return obj;
}
static bool click(const char *name)
{
    lv_obj_t *obj = widget(name);
    return obj && lv_obj_send_event(obj, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK;
}
static bool switch_settings_tab(lv_indev_t *pointer, lv_obj_t *tabs, unsigned index)
{
    lv_obj_t *button = lv_tabview_get_tab_button(tabs, (int32_t)index);
    if (!button || !press_widget(pointer, button)) return false;
    s_test_pointer.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(pointer);
    return lv_tabview_get_tab_active(tabs) == index;
}
static bool tap_named_widget(lv_indev_t *pointer, const char *name)
{
    lv_obj_t *obj = widget(name);
    if (!obj || !press_widget(pointer, obj)) return false;
    s_test_pointer.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(pointer);
    return true;
}
static bool test_update_status(void)
{
    departures_update_t *update = transport_screen_buffer();
    lv_obj_t *label = widget("status");
    if (!update || !label) return false;
    departures_status_t original_status = update->status;
    int64_t original_timestamp = update->last_updated;
    update->last_updated = 1774746000;
    update->status = DEPARTURES_LIVE;
    transport_screen_set_departures(update);
    if (strcmp(lv_label_get_text(label), "Updated 29.03.2026 03:00 | ~ = scheduled")) return false;
    update->status = DEPARTURES_STALE;
    transport_screen_set_departures(update);
    if (!strstr(lv_label_get_text(label), "STALE") ||
        !strstr(lv_label_get_text(label), "29.03.2026 03:00")) return false;
    update->status = DEPARTURES_PARTIAL;
    transport_screen_set_departures(update);
    if (!strstr(lv_label_get_text(label), "PARTIAL") ||
        !strstr(lv_label_get_text(label), "29.03.2026 03:00")) return false;
    update->status = DEPARTURES_CONNECTING;
    transport_screen_set_departures(update);
    if (!strstr(lv_label_get_text(label), "Updating") ||
        !strstr(lv_label_get_text(label), "29.03.2026 03:00")) return false;
    bool original_stale = update->station_stale[0];
    update->station_stale[0] = true;
    transport_screen_set_departures(update);
    if (!strstr(lv_label_get_text(label), "STALE") ||
        !strstr(lv_label_get_text(label), "29.03.2026 03:00")) return false;
    update->station_stale[0] = original_stale;
    update->last_updated = 0;
    update->status = DEPARTURES_NO_CONFIG;
    transport_screen_set_departures(update);
    if (!strstr(lv_label_get_text(label), "Wi-Fi")) return false;
    update->status = original_status;
    update->last_updated = original_timestamp;
    transport_screen_set_departures(update);
    return true;
}
static bool test_interactions(void)
{
    if (!test_update_status()) return false;
    lv_obj_t *heading = widget("heading"), *settings_button = widget("settings_button");
    if (!heading || !settings_button || strcmp(lv_label_get_text(heading), "Public Transport Departures")) return false;
    int32_t button_width = lv_obj_get_width(settings_button), button_height = lv_obj_get_height(settings_button);
    if (!click("settings_button")) return false;
    lv_obj_t *ssid = widget("ssid"), *password = widget("password"), *query = widget("query");
    lv_obj_t *keyboard = widget("keyboard"), *stations = widget("stations");
    lv_obj_t *tabs_settings = widget("settings_tabs"), *back = widget("back");
    lv_obj_update_layout(lv_screen_active());
    if (!ssid || !password || !query || !keyboard || !stations || !tabs_settings || !back ||
        lv_tabview_get_tab_count(tabs_settings) != 2 || lv_obj_get_width(back) != button_width ||
        lv_obj_get_height(back) != button_height) return false;
    lv_indev_t *pointer = lv_indev_create();
    if (!pointer) return false;
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, test_pointer_read);
    lv_obj_t *fields[] = {ssid, password, query};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (i == 2 && !switch_settings_tab(pointer, tabs_settings, 1)) return false;
        if (!press_widget(pointer, fields[i]) ||
            lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) ||
            lv_keyboard_get_textarea(keyboard) != fields[i]) {
            fprintf(stderr, "Keyboard did not open on press for %s\n", lv_obj_get_name(fields[i]));
            return false;
        }
        s_test_pointer.state = LV_INDEV_STATE_RELEASED;
        lv_indev_read(pointer);
        lv_obj_update_layout(lv_screen_active());
        lv_area_t area;
        lv_obj_get_coords(keyboard, &area);
        if (area.x1 < 0 || area.y1 < 0 || area.x2 >= 800 || area.y2 >= 480) {
            fprintf(stderr, "Keyboard outside screen: (%ld,%ld)-(%ld,%ld)\n",
                    (long)area.x1, (long)area.y1, (long)area.x2, (long)area.y2);
            return false;
        }
        lv_area_t field_area;
        lv_obj_get_coords(fields[i], &field_area);
        if (field_area.y2 >= area.y1) {
            fprintf(stderr, "Keyboard overlaps the active field\n");
            return false;
        }
        int32_t width = lv_area_get_width(&area), height = lv_area_get_height(&area);
        lv_textarea_set_text(fields[i], "");
        test_tap(pointer, area.x1 + width * 15 / 100, area.y1 + height / 8);
        if (strlen(lv_textarea_get_text(fields[i])) == 0) {
            fprintf(stderr, "Keyboard input failed for %s\n", lv_obj_get_name(fields[i]));
            return false;
        }
        test_tap(pointer, area.x1 + width * 92 / 100, area.y1 + height * 7 / 8);
        if (!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN)) {
            fprintf(stderr, "Keyboard confirm key did not close it for %s\n",
                    lv_obj_get_name(fields[i]));
            return false;
        }
    }
    /* Switching fields while the keyboard is open must redirect key input. */
    if (!switch_settings_tab(pointer, tabs_settings, 0)) return false;
    if (!press_widget(pointer, ssid)) return false;
    s_test_pointer.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(pointer);
    if (!press_widget(pointer, password) || lv_keyboard_get_textarea(keyboard) != password) return false;
    s_test_pointer.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(pointer);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t keyboard_area;
    lv_obj_get_coords(keyboard, &keyboard_area);
    int32_t keyboard_width = lv_area_get_width(&keyboard_area);
    int32_t keyboard_height = lv_area_get_height(&keyboard_area);
    /* The one-line textarea's Enter key must also dismiss the keyboard. */
    test_tap(pointer, keyboard_area.x1 + keyboard_width * 92 / 100,
             keyboard_area.y1 + keyboard_height * 3 / 8);
    if (!lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN)) {
        fprintf(stderr, "Keyboard Enter key did not close it\n");
        return false;
    }
    /* Tab changes must detach and hide the keyboard. */
    if (!press_widget(pointer, ssid)) return false;
    s_test_pointer.state = LV_INDEV_STATE_RELEASED;
    lv_indev_read(pointer);
    if (!switch_settings_tab(pointer, tabs_settings, 1) ||
        !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN) || lv_keyboard_get_textarea(keyboard)) return false;
    if (!switch_settings_tab(pointer, tabs_settings, 0)) return false;
    if (!tap_named_widget(pointer, "scan")) return false;
    transport_screen_poll();
    lv_obj_t *networks = widget("networks");
    if (!networks) return false;
    lv_obj_send_event(networks, LV_EVENT_VALUE_CHANGED, NULL);
    if (strcmp(lv_textarea_get_text(ssid), "Demo Wi-Fi")) return false;
    lv_textarea_set_text(password, "test-password");
    if (!tap_named_widget(pointer, "save_wifi")) return false;
    app_settings_t settings;
    app_settings_get(&settings);
    if (strcmp(settings.ssid, "Demo Wi-Fi")) return false;
    if (!switch_settings_tab(pointer, tabs_settings, 1)) return false;
    lv_textarea_set_text(query, "offline");
    if (!tap_named_widget(pointer, "search")) return false;
    transport_screen_poll();
    if (!strstr(lv_label_get_text(widget("message")), "failed")) return false;
    lv_textarea_set_text(query, "Alexanderplatz");
    if (!tap_named_widget(pointer, "search")) return false;
    transport_screen_poll();
    if (!tap_named_widget(pointer, "add")) return false;
    /* A service poll before leaving settings must tolerate the new station. */
    transport_screen_poll();
    app_settings_get(&settings);
    if (settings.station_count != 3) return false;
    lv_dropdown_set_selected(stations, 2);
    if (!tap_named_widget(pointer, "up")) return false;
    app_settings_get(&settings);
    if (strcmp(settings.stations[1].id, "900100001")) return false;
    if (!tap_named_widget(pointer, "remove")) return false;
    app_settings_get(&settings);
    if (settings.station_count != 2) return false;
    if (!tap_named_widget(pointer, "back")) return false;
    lv_indev_delete(pointer);
    lv_obj_t *tabs = widget("tabs");
    if (!tabs || lv_tabview_get_tab_count(tabs) != 2) return false;
    departures_update_t *update = transport_screen_buffer();
    if (departures_service_receive(update)) transport_screen_set_departures(update);
    lv_obj_t *tab = lv_obj_get_child(lv_tabview_get_content(tabs), 0);
    lv_obj_update_layout(tab);
    if (lv_obj_get_scroll_bottom(tab) <= 0) return false;
    lv_obj_scroll_to_y(tab, 100, LV_ANIM_OFF);
    int32_t scroll = lv_obj_get_scroll_y(tab);
    transport_screen_set_departures(update);
    return scroll > 0 && lv_obj_get_scroll_y(tab) == scroll;
}
#ifdef _WIN32
static void close_smoke(lv_timer_t *timer)
{
    PostMessageW((HWND)lv_timer_get_user_data(timer), WM_CLOSE, 0, 0);
}
#endif
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--smoke-test")) s_smoke = true;
    else if (argc != 1) { fprintf(stderr, "Usage: lvgl_pc [--smoke-test]\n"); return 2; }
    lv_init();
#ifdef _WIN32
    lv_display_t *display = lv_windows_create_display(L"Berlin transport simulator", 800, 480, 100, false, true);
    if (!display || !lv_windows_acquire_pointer_indev(display)) return 1;
    HWND window = lv_windows_get_display_window_handle(display);
    DWORD start = GetTickCount();
    for (;;) {
        lv_lock(); bool ready = lv_display_get_buf_active(display) != NULL; lv_unlock();
        if (ready) break;
        if (!IsWindow(window) || GetTickCount() - start > 5000) return 1;
        lv_timer_handler(); Sleep(5);
    }
    lv_lock();
#else
    lv_display_t *display = lv_sdl_window_create(800, 480);
    if (!display || !lv_sdl_mouse_create()) return 1;
    lv_sdl_window_set_title(display, "Berlin transport simulator");
#endif
    (void)app_settings_init();
    (void)departures_service_start();
    if (!transport_screen_show_live()) { fprintf(stderr, "UI creation failed\n"); return 1; }
    departures_update_t *update = transport_screen_buffer();
    if (departures_service_receive(update)) transport_screen_set_departures(update);
    if (s_smoke && (lv_tabview_get_tab_count(lv_obj_find_by_name(lv_screen_active(), "tabs")) != 2 || update->departures.count != 24)) {
        fprintf(stderr, "Smoke test failed\n"); return 1;
    }
    if (s_smoke && !test_interactions()) {
        fprintf(stderr, "Simulator interaction test failed\n"); return 1;
    }
#ifdef _WIN32
    if (s_smoke) { lv_timer_t *timer = lv_timer_create(close_smoke, 2000, window); lv_timer_set_repeat_count(timer, 1); }
    lv_unlock();
    while (IsWindow(window)) {
        lv_lock();
        if (departures_service_receive(update)) transport_screen_set_departures(update);
        lv_unlock();
        uint32_t wait = lv_timer_handler(); Sleep(wait > 10 ? 10 : (wait ? wait : 1));
    }
#else
    unsigned smoke_ticks = 0;
    while (!s_smoke || smoke_ticks++ < 200) {
        if (departures_service_receive(update)) transport_screen_set_departures(update);
        lv_timer_handler();
        lv_delay_ms(10);
    }
    lv_sdl_quit();
#endif
    if (s_smoke) puts("Simulator smoke test passed");
    return 0;
}
