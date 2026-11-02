#include "transport_screen.h"
#include "app_settings.h"
#include "departures_service.h"
#include "update_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { lv_obj_t *row, *badge, *line, *direction, *time; } row_t;
static lv_obj_t *s_main, *s_settings_screen, *s_tabs, *s_status, *s_keyboard;
static lv_obj_t *s_tab[APP_MAX_STATIONS], *s_empty[APP_MAX_STATIONS];
static size_t s_view_station_count;
static row_t s_rows[APP_MAX_STATIONS][DEPARTURES_PER_STATION_MAX];
static departures_update_t *s_update;
static app_settings_t s_settings;
static app_service_result_t s_scan, s_search;
static lv_timer_t *s_timer;
typedef struct { const char *name; const lv_obj_class_t *type; } widget_spec_t;
static lv_obj_t *find(lv_obj_t *parent, const char *name) { return lv_obj_find_by_name(parent, name); }

/* The layout is shared by the panel and the desktop simulator. */
static void name_widget(lv_obj_t *obj, const char *name)
{
    if (obj && name) lv_obj_set_name_static(obj, name);
}
static lv_obj_t *create_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen) return NULL;
    lv_obj_set_size(screen, 800, 480);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101923), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}
static lv_obj_t *create_label(lv_obj_t *parent, const char *name, const char *text,
                              int32_t x, int32_t y, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;
    name_widget(label, name);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}
static lv_obj_t *create_button(lv_obj_t *parent, const char *name, const char *text,
                               int32_t x, int32_t y, int32_t width, int32_t height)
{
    lv_obj_t *button = lv_button_create(parent);
    if (!button) return NULL;
    name_widget(button, name);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_t *label = lv_label_create(button);
    if (!label) { lv_obj_delete(button); return NULL; }
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}
static lv_obj_t *create_textarea(lv_obj_t *parent, const char *name, const char *placeholder,
                                 int32_t x, int32_t y, int32_t width, bool password)
{
    lv_obj_t *field = lv_textarea_create(parent);
    if (!field) return NULL;
    name_widget(field, name);
    lv_obj_set_pos(field, x, y);
    lv_obj_set_size(field, width, 50);
    lv_textarea_set_one_line(field, true);
    lv_textarea_set_password_mode(field, password);
    lv_textarea_set_placeholder_text(field, placeholder);
    return field;
}
static lv_obj_t *create_dropdown(lv_obj_t *parent, const char *name, const char *options,
                                 int32_t x, int32_t y, int32_t width)
{
    lv_obj_t *dropdown = lv_dropdown_create(parent);
    if (!dropdown) return NULL;
    name_widget(dropdown, name);
    lv_obj_set_pos(dropdown, x, y);
    lv_obj_set_width(dropdown, width);
    lv_dropdown_set_options(dropdown, options);
    return dropdown;
}
static lv_obj_t *create_tabs(lv_obj_t *parent, const char *name)
{
    lv_obj_t *tabs = lv_tabview_create(parent);
    if (!tabs) return NULL;
    name_widget(tabs, name);
    lv_obj_set_pos(tabs, 0, 70);
    lv_obj_set_size(tabs, 800, 410);
    lv_tabview_set_tab_bar_position(tabs, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabs, 45);
    return tabs;
}
static lv_obj_t *create_departures_layout(void)
{
    lv_obj_t *screen = create_screen();
    if (!screen) return NULL;
    if (!create_label(screen, "heading", "Public Transport Departures", 20, 12, 0xffffff) ||
        !create_label(screen, "status", "Starting...", 20, 41, 0xb4c1ce) ||
        !create_button(screen, "settings_button", "Settings", 664, 8, 120, 43) ||
        !create_tabs(screen, "tabs")) {
        lv_obj_delete(screen);
        return NULL;
    }
    return screen;
}
static void style_settings_tab(lv_obj_t *tab)
{
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_style_border_width(tab, 0, 0);
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x101923), 0);
}
static lv_obj_t *create_settings_layout(void)
{
    lv_obj_t *screen = create_screen();
    if (!screen) return NULL;
    if (!create_label(screen, NULL, "SETTINGS", 15, 10, 0xffffff) ||
        !create_button(screen, "back", "Back", 664, 8, 120, 43)) goto fail;
    lv_obj_t *message = create_label(screen, "message", "", 15, 48, 0xb4c1ce);
    if (!message) goto fail;
    lv_obj_set_width(message, 760);
    lv_obj_t *tabs = create_tabs(screen, "settings_tabs");
    if (!tabs) goto fail;
    lv_obj_t *wifi = lv_tabview_add_tab(tabs, "Wi-Fi");
    lv_obj_t *stations = lv_tabview_add_tab(tabs, "Stations");
    if (!wifi || !stations) goto fail;
    style_settings_tab(wifi);
    style_settings_tab(stations);
    if (!create_textarea(wifi, "ssid", "Wi-Fi SSID", 15, 20, 235, false) ||
        !create_textarea(wifi, "password", "Password", 260, 20, 235, true) ||
        !create_button(wifi, "scan", "Scan", 505, 20, 100, 50) ||
        !create_button(wifi, "save_wifi", "Save Wi-Fi", 615, 20, 165, 50) ||
        !create_dropdown(wifi, "networks", "No networks scanned", 15, 90, 480) ||
        !create_label(stations, NULL, "Stations (max 4)", 15, 10, 0xffffff) ||
        !create_dropdown(stations, "stations", "No stations", 15, 35, 400) ||
        !create_button(stations, "up", "Up", 425, 35, 80, 50) ||
        !create_button(stations, "down", "Down", 515, 35, 80, 50) ||
        !create_button(stations, "remove", "Remove", 605, 35, 175, 50) ||
        !create_textarea(stations, "query", "Search station name", 15, 105, 400, false) ||
        !create_button(stations, "search", "Search", 425, 105, 170, 50) ||
        !create_dropdown(stations, "results", "No search results", 15, 175, 480) ||
        !create_button(stations, "add", "Add", 505, 175, 100, 50)) goto fail;
    lv_obj_t *keyboard = lv_keyboard_create(screen);
    if (!keyboard) goto fail;
    name_widget(keyboard, "keyboard");
    lv_obj_align(keyboard, LV_ALIGN_TOP_LEFT, 0, 305);
    lv_obj_set_size(keyboard, 800, 175);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    return screen;
fail:
    lv_obj_delete(screen);
    return NULL;
}
static lv_obj_t *create_departure_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    if (!row) return NULL;
    lv_obj_set_size(row, 750, 55);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1c2a36), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *badge = lv_obj_create(row);
    if (!badge) goto fail;
    name_widget(badge, "badge");
    lv_obj_set_pos(badge, 8, 7);
    lv_obj_set_size(badge, 66, 40);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xd52432), 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *line = create_label(badge, "line", "M10", 0, 0, 0xffffff);
    lv_obj_t *direction = create_label(row, "direction", "Destination", 90, 15, 0xffffff);
    lv_obj_t *time = create_label(row, "time", "5 min", 610, 15, 0xffffff);
    if (!line || !direction || !time) goto fail;
    lv_obj_center(line);
    lv_obj_set_width(direction, 480);
    return row;
fail:
    lv_obj_delete(row);
    return NULL;
}
static bool has_widgets(lv_obj_t *parent, const widget_spec_t *widgets, size_t count)
{
    if (!parent) return false;
    for (size_t i = 0; i < count; i++) {
        lv_obj_t *obj = find(parent, widgets[i].name);
        if (!obj || !lv_obj_has_class(obj, widgets[i].type)) return false;
    }
    return true;
}
static const widget_spec_t row_widgets[] = {
    {"badge", &lv_obj_class}, {"line", &lv_label_class},
    {"direction", &lv_label_class}, {"time", &lv_label_class}
};
static void set_message(const char *text)
{
    if (s_settings_screen) lv_label_set_text(find(s_settings_screen, "message"), text);
}
static void set_if_changed(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}
static void set_options(lv_obj_t *dropdown, const app_station_t *stations, size_t count, const char *empty)
{
    char options[APP_SEARCH_RESULTS_MAX * APP_STATION_NAME_SIZE + 1] = {0};
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        char label[APP_STATION_NAME_SIZE];
        snprintf(label, sizeof(label), "%s", stations[i].name);
        for (size_t j = 0; label[j]; j++) {
            unsigned char c = (unsigned char)label[j];
            if (c < 32 || c == 127) label[j] = ' ';
        }
        int n = snprintf(options + used, sizeof(options) - used, "%s%s", i ? "\n" : "", label);
        if (n < 0 || (size_t)n >= sizeof(options) - used) break;
        used += (size_t)n;
    }
    lv_dropdown_set_options(dropdown, count ? options : empty);
}
static void refresh_stations(void)
{
    app_settings_get(&s_settings);
    set_options(find(s_settings_screen, "stations"), s_settings.stations, s_settings.station_count, "No stations");
}
static bool save_settings(void)
{
    if (!app_settings_save(&s_settings)) {
        set_message("Settings not saved. Check serial log; do not reprovision the key.");
        return false;
    }
    departures_service_settings_changed();
    /* The old snapshots are indexed by the previous station ordering. */
    if (s_update) memset(s_update, 0, sizeof(*s_update));
    set_message("Saved.");
    return true;
}
static void hide_keyboard(void)
{
    /* Detach first: hiding a focused keyboard can focus another text field. */
    lv_keyboard_set_textarea(s_keyboard, NULL);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}
static void settings_tab_changed(lv_event_t *e)
{
    (void)e;
    hide_keyboard();
}
static void keyboard_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY || lv_event_get_code(e) == LV_EVENT_CANCEL) {
        hide_keyboard();
    }
}
static void textarea_ready_event(lv_event_t *e)
{
    if (lv_keyboard_get_textarea(s_keyboard) == lv_event_get_current_target(e)) {
        hide_keyboard();
    }
}
static void textarea_event(lv_event_t *e)
{
    lv_keyboard_set_textarea(s_keyboard, lv_event_get_current_target(e));
    lv_obj_remove_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
}
static void wifi_save_cb(lv_event_t *e)
{
    (void)e;
    const char *ssid = lv_textarea_get_text(find(s_settings_screen, "ssid"));
    const char *password = lv_textarea_get_text(find(s_settings_screen, "password"));
    if (!ssid[0] || strlen(ssid) >= sizeof(s_settings.ssid) || strlen(password) >= sizeof(s_settings.password)) {
        set_message("SSID must be 1-32 bytes; password must be at most 64 bytes."); return;
    }
    app_settings_get(&s_settings);
    strcpy(s_settings.ssid, ssid);
    strcpy(s_settings.password, password);
    (void)save_settings();
}
static void scan_cb(lv_event_t *e)
{
    (void)e;
    set_message(departures_service_scan() ? "Scanning Wi-Fi..." : "Scan unavailable; check network and serial log.");
}
static void network_chosen_cb(lv_event_t *e)
{
    unsigned index = lv_dropdown_get_selected(lv_event_get_target(e));
    if (index < s_scan.count) lv_textarea_set_text(find(s_settings_screen, "ssid"), s_scan.items[index].name);
}
static void search_cb(lv_event_t *e)
{
    (void)e;
    const char *query = lv_textarea_get_text(find(s_settings_screen, "query"));
    if (!query[0]) { set_message("Enter a station name."); return; }
    set_message(departures_service_search(query) ? "Searching stations..." : "Search unavailable; check network and serial log.");
}
static void add_cb(lv_event_t *e)
{
    (void)e;
    app_settings_get(&s_settings);
    unsigned index = lv_dropdown_get_selected(find(s_settings_screen, "results"));
    if (index >= s_search.count) { set_message("Search and select a station first."); return; }
    if (s_settings.station_count == APP_MAX_STATIONS) { set_message("Maximum of four stations."); return; }
    for (size_t i = 0; i < s_settings.station_count; i++) {
        if (!strcmp(s_settings.stations[i].id, s_search.items[index].id)) {
            set_message("Station already added."); return;
        }
    }
    s_settings.stations[s_settings.station_count++] = s_search.items[index];
    if (save_settings()) refresh_stations();
}
static void remove_cb(lv_event_t *e)
{
    (void)e; app_settings_get(&s_settings);
    unsigned selected = lv_dropdown_get_selected(find(s_settings_screen, "stations"));
    if (selected >= s_settings.station_count) return;
    for (size_t i = selected + 1; i < s_settings.station_count; i++) s_settings.stations[i-1] = s_settings.stations[i];
    --s_settings.station_count;
    if (save_settings()) refresh_stations();
}
static void move_cb(lv_event_t *e)
{
    int direction = (int)(intptr_t)lv_event_get_user_data(e);
    app_settings_get(&s_settings);
    unsigned selected = lv_dropdown_get_selected(find(s_settings_screen, "stations"));
    int other = (int)selected + direction;
    if (selected >= s_settings.station_count || other < 0 || (size_t)other >= s_settings.station_count) return;
    app_station_t tmp = s_settings.stations[selected];
    s_settings.stations[selected] = s_settings.stations[other];
    s_settings.stations[other] = tmp;
    if (save_settings()) { refresh_stations(); lv_dropdown_set_selected(find(s_settings_screen, "stations"), other); }
}
static void render(void)
{
    if (!s_main || !s_update) return;
    const char *status, *prefix = "";
    char status_text[128], updated[24];
    bool has_update = update_time_format(s_update->last_updated, updated, sizeof(updated));
    switch (s_update->status) {
    case DEPARTURES_LIVE: status = "Waiting for departures"; break;
    case DEPARTURES_DEMO: status = "Demo data"; prefix = "Demo | "; break;
    case DEPARTURES_STALE: status = "STALE | Retrying"; prefix = "STALE, retrying | "; break;
    case DEPARTURES_PARTIAL: status = "PARTIAL | Retrying"; prefix = "PARTIAL, retrying | "; break;
    case DEPARTURES_NO_CONFIG: status = app_settings_available() ? "Configure Wi-Fi in Settings" : "Encrypted storage unavailable: check serial log"; break;
    case DEPARTURES_ERROR: status = "Connection error | Retrying"; prefix = "Connection error | "; break;
    default:
        status = "Connecting...";
        prefix = "Updating | ";
        for (size_t i = 0; i < APP_MAX_STATIONS; i++)
            if (s_update->station_stale[i]) { prefix = "STALE, retrying | "; break; }
        break;
    }
    if (has_update) {
        snprintf(status_text, sizeof(status_text), "%sUpdated %s | ~ = scheduled", prefix, updated);
        set_if_changed(s_status, status_text);
    } else {
        set_if_changed(s_status, status);
    }
    size_t counts[APP_MAX_STATIONS] = {0};
    int64_t now = (int64_t)time(NULL);
    size_t total = s_update->departures.count;
    if (total > DEPARTURES_MAX) total = DEPARTURES_MAX;
    for (size_t i = 0; i < total; i++) {
        const departure_t *item = &s_update->departures.items[i];
        unsigned st = item->station;
        if (st >= s_settings.station_count || st >= s_view_station_count ||
            counts[st] >= DEPARTURES_PER_STATION_MAX) continue;
        size_t n = counts[st]++;
        row_t *row = &s_rows[st][n];
        if (!row->row) {
            row->row = create_departure_row(s_tab[st]);
            if (!row->row) continue;
            if (!has_widgets(row->row, row_widgets, sizeof(row_widgets) / sizeof(row_widgets[0]))) {
                lv_obj_delete(row->row);
                row->row = NULL;
                continue;
            }
            row->badge = find(row->row, "badge"); row->line = find(row->row, "line");
            row->direction = find(row->row, "direction"); row->time = find(row->row, "time");
            lv_obj_set_y(row->row, (int)n * 58 + 26);
            lv_label_set_long_mode(row->direction, LV_LABEL_LONG_DOT);
        }
        lv_obj_remove_flag(row->row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(row->badge, lv_color_hex(item->line[0] == 'U' ? 0x2357a4 : item->line[0] == 'S' ? 0x268352 : 0xd52432), 0);
        set_if_changed(row->line, item->line); lv_obj_center(row->line);
        set_if_changed(row->direction, item->direction);
        char text[32];
        if (item->cancelled) snprintf(text, sizeof(text), "cancelled");
        else if (item->when <= now) snprintf(text, sizeof(text), "%sdue", item->planned_only ? "~" : "");
        else {
            int64_t minutes = (item->when - now + 59) / 60;
            if (minutes > 9999) minutes = 9999;
            snprintf(text, sizeof(text), "%s%ld min", item->planned_only ? "~" : "", (long)minutes);
        }
        set_if_changed(row->time, text);
    }
    for (size_t st = 0; st < s_settings.station_count && st < s_view_station_count; st++) {
        const char *empty = s_update->station_stale[st] ? "STALE - retrying" :
                            counts[st] ? "" : s_update->station_valid[st] ? "No departures" : "Waiting for departures";
        set_if_changed(s_empty[st], empty);
        for (size_t n = counts[st]; n < DEPARTURES_PER_STATION_MAX; n++)
            if (s_rows[st][n].row) lv_obj_add_flag(s_rows[st][n].row, LV_OBJ_FLAG_HIDDEN);
    }
}
static bool build_main(void);
static void back_cb(lv_event_t *e)
{
    (void)e;
    hide_keyboard();
    lv_obj_t *old = s_main;
    if (!build_main()) {
        set_message("Cannot rebuild departure screen; check serial log.");
        return;
    }
    lv_screen_load(s_main);
    if (old) lv_obj_delete(old);
}
static void settings_cb(lv_event_t *e)
{
    (void)e; app_settings_get(&s_settings);
    lv_textarea_set_text(find(s_settings_screen, "ssid"), s_settings.ssid);
    lv_textarea_set_text(find(s_settings_screen, "password"), s_settings.password);
    refresh_stations();
    set_message(app_settings_available() ? "Save Wi-Fi to apply; station changes are saved immediately." :
                "Encrypted storage unavailable; check serial log.");
    hide_keyboard();
    lv_screen_load(s_settings_screen);
}
static void bind_click(const char *name, lv_event_cb_t cb, void *data)
{
    lv_obj_add_event_cb(find(s_settings_screen, name), cb, LV_EVENT_CLICKED, data);
}
void transport_screen_poll(void)
{
    app_service_result_t result;
    if (departures_service_receive_result(&result)) {
        if (result.type == APP_RESULT_SCAN) {
            s_scan = result; set_options(find(s_settings_screen, "networks"), result.items, result.count, "No networks found");
        } else {
            s_search = result; set_options(find(s_settings_screen, "results"), result.items, result.count, "No stations found");
        }
        set_message(!result.success ? "Request failed; check Wi-Fi and serial log." : result.count ? "Select a result." : "No results found.");
    }
    render();
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    transport_screen_poll();
}
static bool build_main(void)
{
    app_settings_get(&s_settings);
    lv_obj_t *screen = create_departures_layout();
    lv_obj_t *tabs;
    lv_obj_t *pages[APP_MAX_STATIONS] = {0}, *empty[APP_MAX_STATIONS] = {0};
    const widget_spec_t names[] = {
        {"heading", &lv_label_class}, {"status", &lv_label_class},
        {"tabs", &lv_tabview_class}, {"settings_button", &lv_button_class}
    };
    if (!has_widgets(screen, names, sizeof(names) / sizeof(names[0]))) {
        if (screen) lv_obj_delete(screen);
        return false;
    }
    tabs = find(screen, "tabs");
    lv_obj_add_event_cb(find(screen, "settings_button"), settings_cb, LV_EVENT_CLICKED, NULL);
    for (size_t i = 0; i < s_settings.station_count; i++) {
        pages[i] = lv_tabview_add_tab(tabs, s_settings.stations[i].name);
        if (!pages[i]) { lv_obj_delete(screen); return false; }
        lv_obj_t *button = lv_tabview_get_tab_button(tabs, (int32_t)i);
        lv_obj_t *label = lv_obj_get_child(button, 0);
        lv_obj_set_width(label, LV_PCT(90));
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_center(label);
        lv_obj_set_style_bg_color(pages[i], lv_color_hex(0x101923), 0);
        lv_obj_set_style_bg_opa(pages[i], LV_OPA_COVER, 0);
        empty[i] = lv_label_create(pages[i]);
        if (!empty[i]) { lv_obj_delete(screen); return false; }
        lv_obj_set_style_text_color(empty[i], lv_color_hex(0xb4c1ce), 0);
        lv_label_set_text(empty[i], "Waiting for departures");
    }
    s_main = screen;
    s_status = find(screen, "status");
    s_tabs = tabs;
    memcpy(s_tab, pages, sizeof(s_tab));
    memcpy(s_empty, empty, sizeof(s_empty));
    s_view_station_count = s_settings.station_count;
    memset(s_rows, 0, sizeof(s_rows));
    render();
    return true;
}
lv_obj_t *transport_screen_show_live(void)
{
    if (!s_update) s_update = calloc(1, sizeof(*s_update));
    if (!s_update) return NULL;
    s_settings_screen = create_settings_layout();
    const widget_spec_t names[] = {
        {"message", &lv_label_class}, {"keyboard", &lv_keyboard_class},
        {"settings_tabs", &lv_tabview_class},
        {"ssid", &lv_textarea_class}, {"password", &lv_textarea_class}, {"query", &lv_textarea_class},
        {"networks", &lv_dropdown_class}, {"stations", &lv_dropdown_class}, {"results", &lv_dropdown_class},
        {"back", &lv_button_class}, {"save_wifi", &lv_button_class}, {"scan", &lv_button_class},
        {"search", &lv_button_class}, {"add", &lv_button_class}, {"remove", &lv_button_class},
        {"up", &lv_button_class}, {"down", &lv_button_class}
    };
    if (!has_widgets(s_settings_screen, names, sizeof(names) / sizeof(names[0]))) return NULL;
    s_keyboard = find(s_settings_screen, "keyboard");
    lv_obj_t *settings_tabs = find(s_settings_screen, "settings_tabs");
    lv_tabview_set_tab_bar_size(settings_tabs, 45);
    lv_obj_add_event_cb(settings_tabs, settings_tab_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_keyboard, keyboard_event, LV_EVENT_ALL, NULL);
    const char *fields[] = {"ssid", "password", "query"};
    for (unsigned i = 0; i < 3; i++) {
        lv_obj_t *field = find(s_settings_screen, fields[i]);
        lv_obj_add_event_cb(field, textarea_event, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(field, textarea_event, LV_EVENT_FOCUSED, NULL);
        lv_obj_add_event_cb(field, textarea_ready_event, LV_EVENT_READY, NULL);
    }
    lv_obj_add_event_cb(find(s_settings_screen, "networks"), network_chosen_cb, LV_EVENT_VALUE_CHANGED, NULL);
    bind_click("back", back_cb, NULL); bind_click("save_wifi", wifi_save_cb, NULL); bind_click("scan", scan_cb, NULL);
    bind_click("search", search_cb, NULL); bind_click("add", add_cb, NULL); bind_click("remove", remove_cb, NULL);
    bind_click("up", move_cb, (void *)(intptr_t)-1); bind_click("down", move_cb, (void *)(intptr_t)1);
    if (!build_main()) return NULL;
    lv_screen_load(s_main);
    if (!s_timer) s_timer = lv_timer_create(tick, 1000, NULL);
    return s_tabs;
}
departures_update_t *transport_screen_buffer(void) { return s_update; }
void transport_screen_set_departures(const departures_update_t *update)
{
    if (!s_update || !update) return;
    if (s_update != update) *s_update = *update;
    render();
}
