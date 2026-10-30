#include <stdio.h>
#include <stdint.h>

#include "transport_screen.h"

#define DEPARTURE_COUNT 10
#define ROW_HEIGHT 62

typedef struct {
    const char *line;
    const char *destination;
    uint32_t line_color;
    uint8_t minutes;
} departure_t;

/* Deliberately fictional; the screen does not connect to a transport API. */
static const departure_t s_departures[DEPARTURE_COUNT] = {
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

static lv_obj_t *s_screen;
static lv_obj_t *s_list;
static lv_obj_t *s_return_screen;
static lv_obj_t *s_times[DEPARTURE_COUNT];
static uint8_t s_minutes[DEPARTURE_COUNT];
static unsigned s_next_update;
static lv_timer_t *s_update_timer;

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

void transport_screen_hide(void)
{
    if (s_screen == NULL) return;
    if (s_update_timer != NULL) {
        lv_timer_delete(s_update_timer);
        s_update_timer = NULL;
    }
    lv_screen_load(s_return_screen);
    /* Safe even when invoked by a button event on the screen being removed. */
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

    /* One fictional value changes per tick, so only that label is invalidated. */
    unsigned i = s_next_update++ % DEPARTURE_COUNT;
    s_minutes[i] = s_minutes[i] > 1 ? s_minutes[i] - 1 : 20;
    char text[16];
    snprintf(text, sizeof(text), "%u min", (unsigned)s_minutes[i]);
    lv_label_set_text(s_times[i], text);
}

void transport_screen_set_updates_paused(bool paused)
{
    if (s_update_timer == NULL) return;
    if (paused) {
        lv_timer_pause(s_update_timer);
    } else {
        lv_timer_reset(s_update_timer);
        lv_timer_resume(s_update_timer);
    }
}

lv_obj_t *transport_screen_show(lv_obj_t *return_screen)
{
    if (s_screen != NULL) {
        lv_screen_load(s_screen);
        return s_list;
    }

    s_return_screen = return_screen;
    s_next_update = 0;
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x101923), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    make_label(s_screen, "BERLIN  |  DEPARTURES", 24, 13,
               &lv_font_montserrat_24, 0xffffff);
    make_label(s_screen, "MOCK DATA - simulated, not live", 25, 44,
               &lv_font_montserrat_12, 0xb4c1ce);

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

    s_list = lv_obj_create(s_screen);
    lv_obj_set_pos(s_list, 0, 70);
    lv_obj_set_size(s_list, 800, 410);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x101923), 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_radius(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);

    for (unsigned i = 0; i < DEPARTURE_COUNT; ++i) {
        const departure_t *departure = &s_departures[i];
        lv_obj_t *row = lv_obj_create(s_list);
        lv_obj_set_pos(row, 12, (int32_t)i * ROW_HEIGHT);
        lv_obj_set_size(row, 764, ROW_HEIGHT - 2);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1c2a36), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *badge = lv_obj_create(row);
        lv_obj_set_pos(badge, 9, 10);
        lv_obj_set_size(badge, 68, 40);
        lv_obj_set_style_bg_color(badge, lv_color_hex(departure->line_color), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 0, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *line = make_label(badge, departure->line, 0, 0,
                                    &lv_font_montserrat_18, 0xffffff);
        lv_obj_center(line);

        make_label(row, departure->destination, 96, 19,
                   &lv_font_montserrat_18, 0xffffff);
        char text[16];
        s_minutes[i] = departure->minutes;
        snprintf(text, sizeof(text), "%u min", (unsigned)s_minutes[i]);
        s_times[i] = make_label(row, text, 648, 19,
                                &lv_font_montserrat_18, 0xffffff);
    }
    s_update_timer = lv_timer_create(update_cb, 5000, NULL);
    lv_screen_load(s_screen);
    return s_list;
}
