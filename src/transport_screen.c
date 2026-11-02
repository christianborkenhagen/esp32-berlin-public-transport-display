#include <stdio.h>
#include <string.h>
#include <time.h>

#include "transport_screen.h"

#define MOCK_ROW_COUNT 10
#define ROW_HEIGHT 62

typedef struct {
    const char *line;
    const char *destination;
    uint32_t color;
    unsigned minutes;
} mock_departure_t;

static const mock_departure_t s_mock[MOCK_ROW_COUNT] = {
    {"U2", "Pankow",              0xe52829,  2},
    {"S5", "Spandau",             0xf28623,  4},
    {"M10", "Hauptbahnhof",       0xd52432,  6},
    {"U8", "Wittenau",            0x2357a4,  8},
    {"S7", "Potsdam Hbf",         0x8f468c, 10},
    {"M4", "Hackescher Markt",    0xd52432, 12},
    {"U5", "Alexanderplatz",      0x925c36, 14},
    {"S3", "Ostkreuz",            0x155b9e, 16},
    {"TXL", "Sample destination", 0xa13131, 18},
    {"U6", "Alt-Tegel",           0x7c468a, 20},
};

typedef struct {
    lv_obj_t *row;
    lv_obj_t *badge;
    lv_obj_t *line;
    lv_obj_t *direction;
    lv_obj_t *station;
    lv_obj_t *time;
} row_widgets_t;

static lv_obj_t *s_screen;
static lv_obj_t *s_list;
static lv_obj_t *s_status;
static lv_obj_t *s_empty;
static lv_obj_t *s_return_screen;
static row_widgets_t s_rows[DEPARTURES_MAX];
static lv_timer_t *s_update_timer;
static bool s_live;
static departures_t s_departures;
static unsigned s_mock_minutes[MOCK_ROW_COUNT];
static unsigned s_mock_next;

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int x, int y,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void set_text_if_changed(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static uint32_t line_color(const char *line)
{
    if (line[0] == 'U') return 0x2357a4;
    if (line[0] == 'S') return 0x268352;
    return 0xd52432;
}

static void update_live_times(void)
{
    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < s_departures.count; ++i) {
        const departure_t *item = &s_departures.items[i];
        char text[32];
        if (item->cancelled) {
            snprintf(text, sizeof(text), "cancelled");
        } else if (item->when <= now) {
            snprintf(text, sizeof(text), "%sdue", item->planned_only ? "~" : "");
        } else {
            int64_t minutes = (item->when - now + 59) / 60;
            if (minutes > 9999) minutes = 9999;
            snprintf(text, sizeof(text), "%s%ld min",
                     item->planned_only ? "~" : "", (long)minutes);
        }
        set_text_if_changed(s_rows[i].time, text);
    }
}

void transport_screen_set_departures(const departures_update_t *update)
{
    if (!s_screen || !s_live || !update) return;
    s_departures = update->departures;
    if (s_departures.count > DEPARTURES_MAX) s_departures.count = DEPARTURES_MAX;
    switch (update->status) {
    case DEPARTURES_LIVE:
        set_text_if_changed(s_status, "2 stations | API OK  (~ = scheduled)");
        break;
    case DEPARTURES_DEMO:
        set_text_if_changed(s_status, "2 stations | DEMO - simulated data");
        break;
    case DEPARTURES_STALE:
        set_text_if_changed(s_status, "2 stations | STALE - retrying");
        break;
    case DEPARTURES_PARTIAL:
        set_text_if_changed(s_status, "2 stations | PARTIAL - retrying");
        break;
    case DEPARTURES_NO_CONFIG:
        set_text_if_changed(s_status, "Wi-Fi not configured - see README");
        break;
    case DEPARTURES_ERROR:
        set_text_if_changed(s_status, "Connection or setup failed - check serial log");
        break;
    default:
        set_text_if_changed(s_status, "Connecting to Wi-Fi / synchronizing time...");
        break;
    }
    if (s_departures.count == 0 &&
        (update->status == DEPARTURES_LIVE || update->status == DEPARTURES_DEMO)) {
        lv_obj_remove_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);
    }
    for (size_t i = 0; i < DEPARTURES_MAX; ++i) {
        row_widgets_t *row = &s_rows[i];
        if (i >= s_departures.count) {
            lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const departure_t *item = &s_departures.items[i];
        lv_obj_remove_flag(row->row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(row->badge, lv_color_hex(line_color(item->line)), 0);
        set_text_if_changed(row->line, item->line);
        lv_obj_center(row->line);
        set_text_if_changed(row->direction, item->direction);
        char station_text[80];
        snprintf(station_text, sizeof(station_text), "%s%s",
                 departures_station_name(item->station), item->stale ? " (stale)" : "");
        set_text_if_changed(row->station, station_text);
    }
    update_live_times();
}

void transport_screen_hide(void)
{
    if (!s_screen || !s_return_screen) return;
    if (s_update_timer) {
        lv_timer_delete(s_update_timer);
        s_update_timer = NULL;
    }
    lv_screen_load(s_return_screen);
    lv_obj_delete_async(s_screen);
    s_screen = NULL;
    s_list = NULL;
}

static void back_cb(lv_event_t *event)
{
    (void)event;
    transport_screen_hide();
}

static void update_cb(lv_timer_t *timer)
{
    (void)timer;
    if (lv_screen_active() != s_screen) return;
    if (s_live) {
        update_live_times();
    } else {
        unsigned i = s_mock_next++ % MOCK_ROW_COUNT;
        s_mock_minutes[i] = s_mock_minutes[i] > 1 ? s_mock_minutes[i] - 1 : 20;
        char text[16];
        snprintf(text, sizeof(text), "%u min", s_mock_minutes[i]);
        set_text_if_changed(s_rows[i].time, text);
    }
}

void transport_screen_set_updates_paused(bool paused)
{
    if (!s_update_timer) return;
    if (paused) {
        lv_timer_pause(s_update_timer);
    } else {
        lv_timer_reset(s_update_timer);
        lv_timer_resume(s_update_timer);
    }
}

static lv_obj_t *show(lv_obj_t *return_screen, bool live)
{
    if (s_screen) {
        lv_screen_load(s_screen);
        return s_list;
    }
    s_live = live;
    s_return_screen = return_screen;
    s_mock_next = 0;
    memset(&s_departures, 0, sizeof(s_departures));
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x101923), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    make_label(s_screen, "BERLIN  |  DEPARTURES", 24, 13,
               &lv_font_montserrat_24, 0xffffff);
    s_status = make_label(s_screen, live ? "Connecting..." : "MOCK DATA - simulated, not live",
                          25, 44, &lv_font_montserrat_12, 0xb4c1ce);

    if (return_screen) {
        lv_obj_t *back = lv_button_create(s_screen);
        lv_obj_set_pos(back, 660, 12);
        lv_obj_set_size(back, 116, 43);
        lv_obj_set_style_bg_color(back, lv_color_hex(0x334759), 0);
        lv_obj_set_style_radius(back, 0, 0);
        lv_obj_set_style_shadow_width(back, 0, 0);
        lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *back_text = lv_label_create(back);
        lv_label_set_text(back_text, "Widgets");
        lv_obj_center(back_text);
    }

    s_list = lv_obj_create(s_screen);
    lv_obj_set_pos(s_list, 0, 70);
    lv_obj_set_size(s_list, 800, 410);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x101923), 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    s_empty = make_label(s_list, "No departures currently available", 24, 25,
                         &lv_font_montserrat_18, 0xb4c1ce);
    lv_obj_add_flag(s_empty, LV_OBJ_FLAG_HIDDEN);

    unsigned row_count = live ? DEPARTURES_MAX : MOCK_ROW_COUNT;
    for (unsigned i = 0; i < row_count; ++i) {
        row_widgets_t *w = &s_rows[i];
        w->row = lv_obj_create(s_list);
        lv_obj_set_pos(w->row, 12, (int32_t)i * ROW_HEIGHT);
        lv_obj_set_size(w->row, 764, ROW_HEIGHT - 2);
        lv_obj_set_style_bg_color(w->row, lv_color_hex(0x1c2a36), 0);
        lv_obj_set_style_bg_opa(w->row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(w->row, 0, 0);
        lv_obj_set_style_radius(w->row, 0, 0);
        lv_obj_set_style_pad_all(w->row, 0, 0);
        lv_obj_clear_flag(w->row, LV_OBJ_FLAG_SCROLLABLE);

        w->badge = lv_obj_create(w->row);
        lv_obj_set_pos(w->badge, 9, 10);
        lv_obj_set_size(w->badge, 68, 40);
        lv_obj_set_style_bg_color(w->badge, lv_color_hex(live ? 0xd52432 : s_mock[i].color), 0);
        lv_obj_set_style_bg_opa(w->badge, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(w->badge, 0, 0);
        lv_obj_set_style_radius(w->badge, 0, 0);
        lv_obj_set_style_pad_all(w->badge, 0, 0);
        lv_obj_clear_flag(w->badge, LV_OBJ_FLAG_SCROLLABLE);
        w->line = make_label(w->badge, live ? "" : s_mock[i].line, 0, 0,
                             &lv_font_montserrat_18, 0xffffff);
        lv_obj_center(w->line);
        w->direction = make_label(w->row, live ? "" : s_mock[i].destination, 96, live ? 7 : 19,
                                  &lv_font_montserrat_18, 0xffffff);
        lv_obj_set_width(w->direction, 500);
        lv_label_set_long_mode(w->direction, LV_LABEL_LONG_DOT);
        if (live) {
            w->station = make_label(w->row, "", 96, 35, &lv_font_montserrat_12, 0xb4c1ce);
            lv_obj_set_width(w->station, 500);
            lv_label_set_long_mode(w->station, LV_LABEL_LONG_DOT);
        }
        char text[16] = "";
        if (!live) {
            s_mock_minutes[i] = s_mock[i].minutes;
            snprintf(text, sizeof(text), "%u min", s_mock_minutes[i]);
        }
        w->time = make_label(w->row, text, 628, 19, &lv_font_montserrat_18, 0xffffff);
        if (live) lv_obj_add_flag(w->row, LV_OBJ_FLAG_HIDDEN);
    }
    s_update_timer = lv_timer_create(update_cb, live ? 1000 : 5000, NULL);
    lv_screen_load(s_screen);
    return s_list;
}

lv_obj_t *transport_screen_show(lv_obj_t *return_screen)
{
    return show(return_screen, false);
}

lv_obj_t *transport_screen_show_live(void)
{
    return show(NULL, true);
}
