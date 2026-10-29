#include "benchmark_screen.h"

#include <math.h>
#include <stdio.h>

#include "benchmark_metrics.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "lvgl.h"
#include "sdkconfig.h"

#if !CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS || \
    !CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER || CONFIG_FREERTOS_SMP
#error "The benchmark needs ESP timer-based FreeRTOS runtime stats on both cores"
#endif

#define SCENE_DURATION_MS 12000
#define SAMPLE_PERIOD_MS 1000
#define SCENE_HEIGHT 404
#define SCENE_COUNT 8
#define STATIC_SCENE_INDEX 5
#define PIXEL_SCENE_INDEX 6
#define FULL_REDRAW_SCENE_INDEX 7
#define REFRESH_BASELINE_MS 33
#define REFRESH_TEST_MS 25

static const char *TAG = "benchmark";
static const char *const s_scene_names[SCENE_COUNT] = {
    "Speedometer", "Moving blocks", "Animated arcs", "Translucent layers",
    "Scrolling list", "Static idle", "Static 1 pixel", "Static full redraw",
};
static const uint32_t s_colors[] = {
    0x27c5b1, 0x4d92ff, 0xfca85d, 0xf47487, 0xab8efa, 0xf3d35e,
};

static lv_obj_t *s_scene;
static lv_obj_t *s_title;
static lv_obj_t *s_metrics_label;
static lv_obj_t *s_speed_label;
static lv_obj_t *s_pixel;
static lv_point_precise_t s_tick_points[9][2];
static benchmark_frame_count_cb_t s_frame_count;
static benchmark_metrics_state_t s_metrics;
static unsigned s_scene_index;
static uint32_t s_refresh_period_ms = REFRESH_BASELINE_MS;
static lv_timer_t *s_display_refresh_timer;
static lv_timer_t *s_animation_timer;
static lv_timer_t *s_redraw_timer;
static bool s_pixel_on;

static void set_refresh_period(uint32_t period_ms)
{
    /* Both timers are serviced by the LVGL task; touch stays at 30 ms. */
    lv_timer_set_period(s_display_refresh_timer, period_ms);
    lv_timer_set_period(s_animation_timer, period_ms);
    if (s_redraw_timer) lv_timer_set_period(s_redraw_timer, period_ms);
    s_refresh_period_ms = period_ms;
    ESP_LOGI(TAG, "Refresh/animation period: %" LV_PRIu32 " ms", period_ms);
}

static void animate(lv_obj_t *obj, lv_anim_exec_xcb_t callback, int32_t from,
                    int32_t to, uint32_t duration)
{
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, obj);
    lv_anim_set_exec_cb(&animation, callback);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_reverse_duration(&animation, duration);
    lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&animation);
}

static void set_x(void *obj, int32_t x)
{
    lv_obj_set_x(obj, x);
}

static void set_arc_value(void *obj, int32_t value)
{
    lv_arc_set_value(obj, value);
}

static void set_speed(void *obj, int32_t value)
{
    lv_arc_set_value(obj, value);
    lv_label_set_text_fmt(s_speed_label, "%" LV_PRId32, value);
}

static void set_scroll_y(void *obj, int32_t y)
{
    lv_obj_scroll_to_y(obj, y, LV_ANIM_OFF);
}

static lv_obj_t *colored_rect(lv_obj_t *parent, int x, int y, int width,
                              int height, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static lv_obj_t *text_label(lv_obj_t *parent, const char *text, int x, int y,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_label_set_text(label, text);
    return label;
}

static void create_speedometer(void)
{
    const int center_x = 202;
    const int center_y = 202;
    lv_obj_t *arc = lv_arc_create(s_scene);
    lv_obj_set_pos(arc, 24, 24);
    lv_obj_set_size(arc, 356, 356);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 160);
    lv_obj_set_style_arc_width(arc, 38, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x293a49), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 38, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xffbf21), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    for (unsigned i = 0; i < 9; i++) {
        /* LVGL's angles increase clockwise in screen coordinates. */
        float angle = (135.0f + 270.0f * (float)i / 8.0f) * 0.01745329252f;
        float c = cosf(angle);
        float s = sinf(angle);
        s_tick_points[i][0].x = center_x + (int32_t)(c * 147);
        s_tick_points[i][0].y = center_y + (int32_t)(s * 147);
        s_tick_points[i][1].x = center_x + (int32_t)(c * 163);
        s_tick_points[i][1].y = center_y + (int32_t)(s * 163);
        lv_obj_t *line = lv_line_create(s_scene);
        lv_line_set_points(line, s_tick_points[i], 2);
        lv_obj_set_style_line_color(line, lv_color_hex(0x314a5b), 0);
        lv_obj_set_style_line_width(line, 2, 0);
        char value[4];
        snprintf(value, sizeof(value), "%u", i * 20);
        lv_obj_t *tick = text_label(s_scene, value,
                                    center_x + (int)(c * 120) - 20,
                                    center_y + (int)(s * 120) - 10,
                                    &lv_font_montserrat_12, 0xaec8d4);
        lv_obj_set_width(tick, 40);
        lv_obj_set_style_text_align(tick, LV_TEXT_ALIGN_CENTER, 0);
    }

    s_speed_label = text_label(s_scene, "0", center_x - 60, center_y - 30,
                               &lv_font_montserrat_24, 0x70ddff);
    lv_obj_set_width(s_speed_label, 120);
    lv_obj_set_style_text_align(s_speed_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_transform_scale_x(s_speed_label, 512, 0);
    lv_obj_set_style_transform_scale_y(s_speed_label, 512, 0);
    text_label(s_scene, "mph", center_x - 18, center_y + 35,
               &lv_font_montserrat_18, 0x88b9d7);

    text_label(s_scene, "SPEEDOMETER", 455, 84,
               &lv_font_montserrat_24, 0xf5f8fc);
    text_label(s_scene, "Animated gauge", 455, 125,
               &lv_font_montserrat_18, 0xaec8d4);
    text_label(s_scene, "LIVE RENDER TEST", 455, 218,
               &lv_font_montserrat_18, 0x75e1ce);
    text_label(s_scene, "More scenes follow automatically", 455, 251,
               &lv_font_montserrat_12, 0xaec8d4);
    animate(arc, set_speed, 0, 160, 4500);
}

static void create_blocks(void)
{
    for (unsigned i = 0; i < 12; i++) {
        int y = 18 + (int)(i / 3) * 94;
        lv_obj_t *block = colored_rect(s_scene, 0, y, 82, 64,
                                       s_colors[i % 6]);
        lv_obj_set_style_radius(block, 10, 0);
        int start = (int)(i % 3) * 110;
        animate(block, set_x, start, 710 - start, 1000 + i * 87);
    }
}

static void create_arcs(void)
{
    for (unsigned i = 0; i < 12; i++) {
        lv_obj_t *arc = lv_arc_create(s_scene);
        lv_obj_set_pos(arc, 35 + (int)(i % 6) * 126,
                       40 + (int)(i / 6) * 165);
        lv_obj_set_size(arc, 96, 96);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_range(arc, 0, 100);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(s_colors[i % 6]),
                                   LV_PART_INDICATOR);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        animate(arc, set_arc_value, 5, 100, 900 + i * 85);
    }
}

static void create_layers(void)
{
    for (unsigned i = 0; i < 8; i++) {
        int y = 20 + (int)(i % 3) * 105;
        int start = 30 + (int)(i % 4) * 70;
        lv_obj_t *layer = colored_rect(s_scene, start, y, 210, 150,
                                       s_colors[i % 6]);
        lv_obj_set_style_radius(layer, 38, 0);
        lv_obj_set_style_bg_opa(layer, LV_OPA_60, 0);
        animate(layer, set_x, start, 560 - start, 1300 + i * 140);
    }
}

static void create_scrolling_list(void)
{
    lv_obj_t *list = lv_obj_create(s_scene);
    lv_obj_set_size(list, 800, SCENE_HEIGHT);
    lv_obj_set_pos(list, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x111d2b), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);

    for (unsigned i = 0; i < 16; i++) {
        lv_obj_t *row = colored_rect(list, 15, (int)i * 66, 770, 60,
                                     0x233549);
        lv_obj_t *badge = colored_rect(row, 10, 10, 54, 40, s_colors[i % 6]);
        lv_obj_set_style_radius(badge, 6, 0);
        lv_obj_t *label = lv_label_create(row);
        lv_obj_set_style_text_color(label, lv_color_hex(0xf2f6fa), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_set_pos(label, 85, 18);
        lv_label_set_text_fmt(label, "Animated list item %02u", i + 1);
    }
    lv_obj_update_layout(list);
    animate(list, set_scroll_y, 0, lv_obj_get_scroll_bottom(list), 3500);
}

static void create_static_scene(unsigned index)
{
    text_label(s_scene, "STATIC DISPLAY", 45, 45,
               &lv_font_montserrat_24, 0xf5f8fc);
    const char *description = index == STATIC_SCENE_INDEX
                                  ? "No periodic redraws"
                              : index == PIXEL_SCENE_INDEX
                                  ? "One pixel changes each timer period"
                                  : "Full screen redrawn each timer period";
    text_label(s_scene, description, 45, 90,
               &lv_font_montserrat_18, 0xaec8d4);
    text_label(s_scene, "The panel keeps scanning in all three tests.",
               45, 130, &lv_font_montserrat_16, 0x75e1ce);

    /* Only this 1x1 object is modified in the partial-redraw scene. */
    if (index == PIXEL_SCENE_INDEX) {
        s_pixel_on = false;
        s_pixel = colored_rect(s_scene, 400, 180, 1, 1, 0x000000);
    }

    for (unsigned i = 0; i < 6; i++) {
        lv_obj_t *block = colored_rect(s_scene, 45 + (int)i * 115, 210,
                                       85, 85, s_colors[i]);
        lv_obj_set_style_radius(block, 10, 0);
    }
    text_label(s_scene, "Live FPS and CPU estimates: serial console",
               45, 340, &lv_font_montserrat_16, 0xaec8d4);
}

static void load_scene(unsigned index)
{
    lv_obj_clean(s_scene);
    lv_label_set_text_fmt(s_title, "DISPLAY BENCHMARK  |  %s",
                          s_scene_names[index]);
    if (index >= STATIC_SCENE_INDEX) {
        lv_label_set_text_fmt(s_metrics_label,
                              "Static comparison   |   %" LV_PRIu32
                              " ms   |   Live metrics in serial",
                              s_refresh_period_ms);
    } else {
        lv_label_set_text(s_metrics_label, "Measuring...");
    }
    switch (index) {
    case 0: create_speedometer(); break;
    case 1: create_blocks(); break;
    case 2: create_arcs(); break;
    case 3: create_layers(); break;
    case 4: create_scrolling_list(); break;
    case STATIC_SCENE_INDEX:
    case PIXEL_SCENE_INDEX:
    case FULL_REDRAW_SCENE_INDEX: create_static_scene(index); break;
    }
    if (index == PIXEL_SCENE_INDEX || index == FULL_REDRAW_SCENE_INDEX) {
        lv_timer_reset(s_redraw_timer);
        lv_timer_resume(s_redraw_timer);
    }
    ESP_LOGI(TAG, "Scene %u/%u: %s", index + 1, SCENE_COUNT,
             s_scene_names[index]);
}

static void redraw_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_scene_index == PIXEL_SCENE_INDEX) {
        s_pixel_on = !s_pixel_on;
        lv_obj_set_style_bg_color(s_pixel,
                                  lv_color_hex(s_pixel_on ? 0xffffff : 0x000000), 0);
    } else if (s_scene_index == FULL_REDRAW_SCENE_INDEX) {
        /* Force a full redraw without changing the otherwise static content. */
        lv_obj_invalidate(lv_screen_active());
    }
}

static void scene_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_timer_pause(s_redraw_timer);
    s_pixel = NULL;
    s_scene_index = (s_scene_index + 1) % SCENE_COUNT;
    if (s_scene_index == 0) {
        set_refresh_period(s_refresh_period_ms == REFRESH_BASELINE_MS
                               ? REFRESH_TEST_MS
                               : REFRESH_BASELINE_MS);
    }
    load_scene(s_scene_index);
}

static void metrics_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    benchmark_metrics_result_t result;
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint32_t idle0 = (uint32_t)ulTaskGetIdleRunTimeCounterForCore(0);
    uint32_t idle1 = (uint32_t)ulTaskGetIdleRunTimeCounterForCore(1);
    if (!benchmark_metrics_sample(&s_metrics, now_us, idle0, idle1,
                                  s_frame_count(), &result)) {
        return;
    }
    if (s_scene_index < STATIC_SCENE_INDEX) {
        lv_label_set_text_fmt(s_metrics_label, "%" LV_PRIu32 ".%" LV_PRIu32
                              " FPS   |   CPU ~%" LV_PRIu32 "%% (2 cores)   |   %"
                              LV_PRIu32 " ms",
                              result.fps_tenths / 10, result.fps_tenths % 10,
                              result.cpu_percent, s_refresh_period_ms);
    }
    ESP_LOGI(TAG, "%s: %" LV_PRIu32 ".%" LV_PRIu32
             " FPS, CPU ~%" LV_PRIu32 "%% (2 cores; core 0 ~%" LV_PRIu32
             "%%, core 1 ~%" LV_PRIu32 "%%), period %" LV_PRIu32 " ms",
             s_scene_names[s_scene_index], result.fps_tenths / 10,
             result.fps_tenths % 10, result.cpu_percent,
             result.cpu_core_percent[0], result.cpu_core_percent[1],
             s_refresh_period_ms);
}

void benchmark_screen_create(benchmark_frame_count_cb_t frame_count)
{
    s_frame_count = frame_count;
    s_display_refresh_timer = lv_display_get_refr_timer(lv_display_get_default());
    s_animation_timer = lv_anim_get_timer();
    ESP_ERROR_CHECK(s_display_refresh_timer && s_animation_timer
                        ? ESP_OK
                        : ESP_ERR_INVALID_STATE);
    s_redraw_timer = lv_timer_create(redraw_timer_cb, REFRESH_BASELINE_MS, NULL);
    ESP_ERROR_CHECK(s_redraw_timer ? ESP_OK : ESP_ERR_NO_MEM);
    lv_timer_pause(s_redraw_timer);
    set_refresh_period(REFRESH_BASELINE_MS);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x111d2b), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = colored_rect(screen, 0, 0, 800, 76, 0x17283a);
    s_title = lv_label_create(header);
    lv_obj_set_pos(s_title, 18, 6);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0xf5f8fc), 0);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_24, 0);

    s_metrics_label = lv_label_create(header);
    lv_obj_set_pos(s_metrics_label, 18, 42);
    lv_obj_set_style_text_color(s_metrics_label, lv_color_hex(0x75e1ce), 0);
    lv_obj_set_style_text_font(s_metrics_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(s_metrics_label, "Measuring...");

    s_scene = lv_obj_create(screen);
    lv_obj_remove_style_all(s_scene);
    lv_obj_set_pos(s_scene, 0, 76);
    lv_obj_set_size(s_scene, 800, SCENE_HEIGHT);
    lv_obj_clear_flag(s_scene, LV_OBJ_FLAG_SCROLLABLE);
    load_scene(0);
    metrics_timer_cb(NULL); /* Establish the first sampling baseline. */
    lv_timer_create(metrics_timer_cb, SAMPLE_PERIOD_MS, NULL);
    lv_timer_create(scene_timer_cb, SCENE_DURATION_MS, NULL);
}
