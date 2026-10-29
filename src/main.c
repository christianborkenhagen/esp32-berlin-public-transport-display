#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "lvgl.h"
#include "benchmark_screen.h"

#if !CONFIG_ESP_MAIN_TASK_AFFINITY_CPU0 && !CONFIG_ESP_MAIN_TASK_AFFINITY_CPU1
#error "The main task must be pinned so the LCD interrupts and switch task share a core"
#endif

#if defined(APP_LVGL_O3) && CONFIG_LV_DRAW_THREAD_STACK_SIZE >= 32768
#error "The -O3 build needs smaller LVGL worker stacks; see README.md"
#endif

#define LCD_H_RES               800
#define LCD_V_RES               480
/* Match Elecrow's V1.1 ESP-IDF RGB panel configuration. */
#define LCD_PIXEL_CLOCK_HZ      (16 * 1000 * 1000)
#define LCD_BOUNCE_BUFFER_LINES  20
#define LCD_FRAME_SYNC_TIMEOUT_MS 1000
#define LCD_DIAGNOSTICS_PERIOD_MS 5000

/* Elecrow's V1.1 RGB pin map from its board example. */
#define LCD_PCLK_GPIO           39
#define LCD_HSYNC_GPIO          40
#define LCD_VSYNC_GPIO          41
#define LCD_DE_GPIO             42

#define I2C_SDA_GPIO            15
#define I2C_SCL_GPIO            16
#define BACKLIGHT_I2C_ADDRESS   0x30
#define TOUCH_I2C_ADDRESS       0x5D
#define TOUCH_STATUS_REGISTER   0x814E
#define TOUCH_I2C_SPEED_HZ      400000
#define TOUCH_I2C_TIMEOUT_MS    10

#define LVGL_MAX_SLEEP_MS       10
#define LVGL_TOUCH_READ_PERIOD_MS 30
#define LVGL_TASK_STACK_SIZE    12288
#define LVGL_TASK_PRIORITY      5
#define LVGL_TASK_CORE          1
#define LVGL_EXTRA_POOL_SIZE    (48 * 1024)
#define LCD_SWITCH_TASK_STACK_SIZE 3072
#define LCD_SWITCH_TASK_PRIORITY 6

static const char *TAG = "panel_test";

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_touch_device;
static esp_lcd_panel_handle_t s_panel;
static void *s_frame_buffers[2];
static SemaphoreHandle_t s_frame_done;
static SemaphoreHandle_t s_switch_done;
static QueueHandle_t s_switch_queue;
static portMUX_TYPE s_frame_stats_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_vsync_count;
static uint32_t s_bounce_frame_count;
static uint32_t s_lvgl_frame_count;
static uint64_t s_lvgl_sync_wait_us;
static uint64_t s_lvgl_prepare_us;
static uint64_t s_lvgl_render_us;
static uint64_t s_lvgl_refresh_us;
static uint32_t s_lvgl_refresh_max_us;
static uint64_t s_lvgl_drawn_pixels;
static uint64_t s_lvgl_drawn_regions;
static uint32_t s_lvgl_largest_region_pixels;
static lv_area_t s_lvgl_largest_region;
/* All cadence counters below are accessed only by the LVGL task. */
static uint64_t s_last_present_us;
static uint64_t s_present_interval_sum_us;
static uint32_t s_present_interval_count;
static uint32_t s_present_interval_min_us;
static uint32_t s_present_interval_max_us;
static uint64_t s_last_present_refresh_end_us;
static uint64_t s_refresh_gap_sum_us;
static uint32_t s_refresh_gap_count;
static uint32_t s_refresh_gap_max_us;
static lv_timer_t *s_touch_timer;
static uint32_t s_touch_period_ms = LVGL_TOUCH_READ_PERIOD_MS;
static int64_t s_touch_last_read_us;
static uint64_t s_touch_interval_sum_us;
static uint32_t s_touch_interval_count;
static uint32_t s_touch_interval_max_us;
static bool s_touch_pressed;
static lv_point_t s_last_touch;
static void init_i2c_and_backlight(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus init failed: %s; continuing without I2C checks",
                 esp_err_to_name(err));
        return;
    }

    err = i2c_master_probe(s_i2c_bus, TOUCH_I2C_ADDRESS, 100);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Touch controller responds at I2C address 0x%02X",
                 TOUCH_I2C_ADDRESS);

        const i2c_device_config_t touch_device_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = TOUCH_I2C_ADDRESS,
            .scl_speed_hz = TOUCH_I2C_SPEED_HZ,
        };
        err = i2c_master_bus_add_device(
            s_i2c_bus, &touch_device_config, &s_touch_device);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "GT911 touch input enabled");
        } else {
            ESP_LOGW(TAG, "Could not add GT911 I2C device: %s",
                     esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "No response at touch address 0x%02X (%s)",
                 TOUCH_I2C_ADDRESS, esp_err_to_name(err));
    }

    err = i2c_master_probe(s_i2c_bus, BACKLIGHT_I2C_ADDRESS, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Backlight controller not found at 0x%02X (%s)",
                 BACKLIGHT_I2C_ADDRESS, esp_err_to_name(err));
        return;
    }

    const i2c_device_config_t backlight_device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BACKLIGHT_I2C_ADDRESS,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t backlight_device;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(
        s_i2c_bus, &backlight_device_config, &backlight_device));

    /*
     * On V1.1, the STC8H1K28 backlight controller accepts a brightness value
     * from 0x05 (off) through 0x10 (maximum).
     */
    const uint8_t maximum_brightness = 0x10;
    err = i2c_master_transmit(backlight_device, &maximum_brightness, sizeof(maximum_brightness), 100);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Screen turned on");
    } else {
        ESP_LOGW(TAG, "Could not turn on screen: %s", esp_err_to_name(err));
    }

    const uint8_t desired_brightness = 0x06;
    err = i2c_master_transmit(backlight_device, &desired_brightness, sizeof(desired_brightness), 100);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Brightness set");
    } else {
        ESP_LOGW(TAG, "Could not set brightness: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(backlight_device));
}

static esp_err_t gt911_read_register(uint16_t reg, uint8_t *data, size_t size)
{
    const uint8_t register_address[] = {
        (uint8_t)(reg >> 8),
        (uint8_t)reg,
    };
    return i2c_master_transmit_receive(s_touch_device, register_address,
                                       sizeof(register_address), data, size,
                                       TOUCH_I2C_TIMEOUT_MS);
}

static void gt911_clear_status(void)
{
    const uint8_t clear_status[] = {
        (uint8_t)(TOUCH_STATUS_REGISTER >> 8),
        (uint8_t)TOUCH_STATUS_REGISTER,
        0,
    };
    (void)i2c_master_transmit(s_touch_device, clear_status,
                              sizeof(clear_status), TOUCH_I2C_TIMEOUT_MS);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    int64_t now_us = esp_timer_get_time();
    if (s_touch_last_read_us != 0) {
        uint32_t interval_us = (uint32_t)(now_us - s_touch_last_read_us);
        s_touch_interval_sum_us += interval_us;
        s_touch_interval_count++;
        if (interval_us > s_touch_interval_max_us) {
            s_touch_interval_max_us = interval_us;
        }
    }
    s_touch_last_read_us = now_us;

    uint8_t status;
    esp_err_t err = gt911_read_register(
        TOUCH_STATUS_REGISTER, &status, sizeof(status));
    if (err != ESP_OK) {
        s_touch_pressed = false;
    } else if ((status & 0x80) != 0) {
        const uint8_t touch_count = status & 0x0F;
        if (touch_count == 0) {
            s_touch_pressed = false;
        } else if (touch_count <= 5) {
            uint8_t point[8];
            if (gt911_read_register(TOUCH_STATUS_REGISTER + 1,
                                    point, sizeof(point)) == ESP_OK) {
                s_last_touch.x = (int32_t)(((uint16_t)point[2] << 8) | point[1]);
                s_last_touch.y = (int32_t)(((uint16_t)point[4] << 8) | point[3]);
                if (s_last_touch.x >= LCD_H_RES) {
                    s_last_touch.x = LCD_H_RES - 1;
                }
                if (s_last_touch.y >= LCD_V_RES) {
                    s_last_touch.y = LCD_V_RES - 1;
                }
                s_touch_pressed = true;
            } else {
                s_touch_pressed = false;
            }
        } else {
            s_touch_pressed = false;
        }
        gt911_clear_status();
    }

    data->state = s_touch_pressed ? LV_INDEV_STATE_PRESSED
                                  : LV_INDEV_STATE_RELEASED;
    data->point = s_last_touch;
}

static esp_lcd_panel_handle_t init_rgb_panel(void)
{
    const esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags = {
                .hsync_idle_low = 0,
                .vsync_idle_low = 0,
                .de_idle_high = 0,
                .pclk_active_neg = 1,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = LCD_BOUNCE_BUFFER_LINES * LCD_H_RES,
        .dma_burst_size = 64,
        .sram_trans_align = 4,
        .hsync_gpio_num = LCD_HSYNC_GPIO,
        .vsync_gpio_num = LCD_VSYNC_GPIO,
        .de_gpio_num = LCD_DE_GPIO,
        .pclk_gpio_num = LCD_PCLK_GPIO,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            21, 47, 48, 45, 38, 9, 10, 11,
            12, 13, 14, 7, 17, 18, 3, 46,
        },
        .flags.fb_in_psram = 1,
    };

    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel));

    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(
        panel, 2, &s_frame_buffers[0], &s_frame_buffers[1]));
    const size_t frame_buffer_size =
        LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    memset(s_frame_buffers[0], 0, frame_buffer_size);
    memset(s_frame_buffers[1], 0, frame_buffer_size);

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    return panel;
}

static uint32_t lvgl_tick_get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool lcd_frame_done_cb(esp_lcd_panel_handle_t panel,
                              const esp_lcd_rgb_panel_event_data_t *event_data,
                              void *user_ctx)
{
    (void)panel;
    (void)event_data;

    portENTER_CRITICAL_ISR(&s_frame_stats_lock);
    s_bounce_frame_count++;
    portEXIT_CRITICAL_ISR(&s_frame_stats_lock);

    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &task_woken);
    return task_woken == pdTRUE;
}

static bool lcd_vsync_cb(esp_lcd_panel_handle_t panel,
                         const esp_lcd_rgb_panel_event_data_t *event_data,
                         void *user_ctx)
{
    (void)panel;
    (void)event_data;
    (void)user_ctx;

    portENTER_CRITICAL_ISR(&s_frame_stats_lock);
    s_vsync_count++;
    portEXIT_CRITICAL_ISR(&s_frame_stats_lock);
    return false;
}

static void lcd_switch_task(void *arg)
{
    (void)arg;

    while (true) {
        uint8_t *frame_buffer;
        ESP_ERROR_CHECK(xQueueReceive(s_switch_queue, &frame_buffer, portMAX_DELAY) == pdTRUE
                            ? ESP_OK
                            : ESP_FAIL);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
            s_panel, 0, 0, LCD_H_RES, LCD_V_RES, frame_buffer));

        /*
         * This task runs on the core that installed the LCD interrupts. An
         * interrupt that latched the old framebuffer must therefore finish
         * before the task can clear its notification. The next completion
         * after the switch has latched the requested framebuffer.
         */
        (void)xSemaphoreTake(s_frame_done, 0);
        ESP_ERROR_CHECK(xSemaphoreTake(s_frame_done,
                                      pdMS_TO_TICKS(LCD_FRAME_SYNC_TIMEOUT_MS)) == pdTRUE
                            ? ESP_OK
                            : ESP_ERR_TIMEOUT);
        ESP_ERROR_CHECK(xSemaphoreGive(s_switch_done) == pdTRUE ? ESP_OK : ESP_FAIL);
    }
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area,
                          uint8_t *pixel_map)
{
    /* These are LVGL's joined, clipped render regions in direct mode. */
    uint32_t pixels = (uint32_t)lv_area_get_width(area) *
                      (uint32_t)lv_area_get_height(area);
    s_lvgl_drawn_pixels += pixels;
    s_lvgl_drawn_regions++;
    if (pixels > s_lvgl_largest_region_pixels) {
        s_lvgl_largest_region_pixels = pixels;
        s_lvgl_largest_region = *area;
    }

    if (lv_display_flush_is_last(display)) {
        const int64_t wait_start_us = esp_timer_get_time();
        /* Direct mode passes a complete driver-owned framebuffer. */
        ESP_ERROR_CHECK(xQueueSend(s_switch_queue, &pixel_map,
                                   pdMS_TO_TICKS(LCD_FRAME_SYNC_TIMEOUT_MS)) == pdTRUE
                            ? ESP_OK
                            : ESP_ERR_TIMEOUT);
        ESP_ERROR_CHECK(xSemaphoreTake(s_switch_done,
                                      pdMS_TO_TICKS(2 * LCD_FRAME_SYNC_TIMEOUT_MS)) == pdTRUE
                            ? ESP_OK
                            : ESP_ERR_TIMEOUT);
        uint64_t present_us = (uint64_t)esp_timer_get_time();
        s_lvgl_sync_wait_us += present_us - (uint64_t)wait_start_us;
        if (s_last_present_us != 0) {
            uint32_t interval_us = (uint32_t)(present_us - s_last_present_us);
            s_present_interval_sum_us += interval_us;
            s_present_interval_count++;
            if (s_present_interval_min_us == 0 || interval_us < s_present_interval_min_us) {
                s_present_interval_min_us = interval_us;
            }
            if (interval_us > s_present_interval_max_us) {
                s_present_interval_max_us = interval_us;
            }
        }
        s_last_present_us = present_us;
        s_lvgl_frame_count++;
        static bool first_frame_logged;
        if (!first_frame_logged) {
            ESP_LOGI(TAG, "First framebuffer switch synchronized");
            first_frame_logged = true;
        }
    }
    lv_display_flush_ready(display);
}

static void lvgl_display_event_cb(lv_event_t *event)
{
    /* LVGL display events and flush callbacks run on the LVGL task. */
    static uint64_t refresh_start_us;
    static uint64_t render_start_us;
    static uint64_t sync_wait_start_us;
    static uint32_t frame_count_at_start;
    uint64_t now_us = (uint64_t)esp_timer_get_time();

    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        refresh_start_us = now_us;
        sync_wait_start_us = s_lvgl_sync_wait_us;
        frame_count_at_start = s_lvgl_frame_count;
        break;
    case LV_EVENT_RENDER_START:
        render_start_us = now_us;
        break;
    case LV_EVENT_REFR_READY:
        if (s_lvgl_frame_count != frame_count_at_start) {
            uint64_t refresh_us = now_us - refresh_start_us;
            uint64_t prepare_us = render_start_us - refresh_start_us;
            uint64_t sync_wait_us = s_lvgl_sync_wait_us - sync_wait_start_us;
            if (s_last_present_refresh_end_us != 0) {
                uint32_t gap_us = (uint32_t)(refresh_start_us - s_last_present_refresh_end_us);
                s_refresh_gap_sum_us += gap_us;
                s_refresh_gap_count++;
                if (gap_us > s_refresh_gap_max_us) {
                    s_refresh_gap_max_us = gap_us;
                }
            }
            s_last_present_refresh_end_us = now_us;
            s_lvgl_prepare_us += prepare_us;
            /* Render phase includes the synchronous flush; report it separately. */
            s_lvgl_render_us += refresh_us - prepare_us - sync_wait_us;
            s_lvgl_refresh_us += refresh_us;
            if (refresh_us > s_lvgl_refresh_max_us) {
                s_lvgl_refresh_max_us = (uint32_t)refresh_us;
            }
        }
        break;
    default:
        break;
    }
}

static uint32_t benchmark_present_count(void)
{
    /* Both the flush callback and benchmark timer run on the LVGL task. */
    return s_lvgl_frame_count;
}

static void lvgl_task(void *arg)
{
    (void)arg;

    lv_init();
    lv_tick_set_cb(lvgl_tick_get_ms);

    /*
     * The two drawing workers can need more temporary layer memory than
     * LVGL's 64 KiB internal pool provides, especially when changing tabs.
     * Add a separate PSRAM pool before any display objects or draw jobs exist.
     * LVGL owns the allocation for the lifetime of the application.
     */
    void *extra_pool = heap_caps_malloc(LVGL_EXTRA_POOL_SIZE,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (extra_pool == NULL) {
        ESP_LOGW(TAG, "Could not allocate LVGL extra PSRAM pool");
    } else if (lv_mem_add_pool(extra_pool, LVGL_EXTRA_POOL_SIZE) == NULL) {
        heap_caps_free(extra_pool);
        ESP_LOGW(TAG, "Could not add LVGL extra PSRAM pool");
    } else {
        ESP_LOGI(TAG, "Added %u-byte LVGL PSRAM memory pool",
                 (unsigned)LVGL_EXTRA_POOL_SIZE);
    }

    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_ERROR_CHECK(display == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    const size_t frame_buffer_size =
        LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    /* The panel initially scans buffer 0, so LVGL must start with buffer 1. */
    lv_display_set_buffers(display, s_frame_buffers[1], s_frame_buffers[0],
                           frame_buffer_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_add_event_cb(display, lvgl_display_event_cb, LV_EVENT_ALL, NULL);

    if (s_touch_device != NULL) {
        lv_indev_t *touch = lv_indev_create();
        ESP_ERROR_CHECK(touch == NULL ? ESP_ERR_NO_MEM : ESP_OK);
        lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch, lvgl_touch_read_cb);
        s_touch_timer = lv_indev_get_read_timer(touch);
        if (s_touch_timer != NULL) {
            lv_timer_set_period(s_touch_timer, s_touch_period_ms);
        }
    }

    benchmark_screen_create(benchmark_present_count);
#ifdef APP_LVGL_O3
    ESP_LOGI(TAG, "LVGL compiler optimization: -O3");
#else
    ESP_LOGI(TAG, "LVGL compiler optimization: -O2");
#endif
    ESP_LOGI(TAG, "Animation benchmark running on the 800x480 panel at %u MHz PCLK",
             (unsigned)(LCD_PIXEL_CLOCK_HZ / 1000000));
    ESP_LOGI(TAG, "LVGL direct mode: two %u-byte PSRAM framebuffers",
             (unsigned)frame_buffer_size);
    ESP_LOGI(TAG, "Timing: FreeRTOS tick %u Hz, LVGL configured default %u ms; benchmark alternates 33/25 ms",
             (unsigned)CONFIG_FREERTOS_HZ, (unsigned)CONFIG_LV_DEF_REFR_PERIOD);
    ESP_LOGI(TAG, "PSRAM detected: %u bytes", (unsigned)esp_psram_get_size());

    uint64_t last_diagnostics_ms = esp_timer_get_time() / 1000;
    portENTER_CRITICAL(&s_frame_stats_lock);
    uint32_t last_vsync = s_vsync_count;
    uint32_t last_bounce_frame = s_bounce_frame_count;
    portEXIT_CRITICAL(&s_frame_stats_lock);
    uint32_t last_lvgl_frame = 0;
    uint64_t last_sync_wait_us = 0;
    uint64_t last_prepare_us = 0;
    uint64_t last_render_us = 0;
    uint64_t last_refresh_us = 0;
    uint64_t last_drawn_pixels = 0;
    uint64_t last_drawn_regions = 0;
    uint64_t delay_requested_sum_us = 0;
    uint64_t delay_actual_sum_us = 0;
    uint32_t delay_count = 0;
    uint32_t delay_actual_max_us = 0;
    while (true) {
        uint32_t delay_ms = lv_timer_handler();
        uint64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - last_diagnostics_ms >= LCD_DIAGNOSTICS_PERIOD_MS) {
            portENTER_CRITICAL(&s_frame_stats_lock);
            uint32_t vsync = s_vsync_count;
            uint32_t bounce_frame = s_bounce_frame_count;
            portEXIT_CRITICAL(&s_frame_stats_lock);

            uint32_t updates = s_lvgl_frame_count - last_lvgl_frame;
            uint64_t sync_wait_us = s_lvgl_sync_wait_us - last_sync_wait_us;
            uint64_t prepare_us = s_lvgl_prepare_us - last_prepare_us;
            uint64_t render_us = s_lvgl_render_us - last_render_us;
            uint64_t refresh_us = s_lvgl_refresh_us - last_refresh_us;
            uint64_t drawn_pixels = s_lvgl_drawn_pixels - last_drawn_pixels;
            uint64_t drawn_regions = s_lvgl_drawn_regions - last_drawn_regions;
            ESP_LOGI(TAG, "Display %u ms: VSYNC %u, bounce frames %u, LVGL updates %u, avg sync wait %u ms",
                     (unsigned)(now_ms - last_diagnostics_ms),
                     (unsigned)(vsync - last_vsync),
                     (unsigned)(bounce_frame - last_bounce_frame),
                     (unsigned)updates,
                     (unsigned)(updates ? sync_wait_us / updates / 1000 : 0));
            if (updates) {
                ESP_LOGI(TAG, "LVGL per update: avg prepare %u ms, draw/other %u ms, sync %u ms, total %u ms; max total %u ms",
                         (unsigned)(prepare_us / updates / 1000),
                         (unsigned)(render_us / updates / 1000),
                         (unsigned)(sync_wait_us / updates / 1000),
                         (unsigned)(refresh_us / updates / 1000),
                         (unsigned)(s_lvgl_refresh_max_us / 1000));
                ESP_LOGI(TAG, "LVGL areas per update: avg %u pixels, %u regions; largest (%ld,%ld)-(%ld,%ld), %u pixels",
                         (unsigned)(drawn_pixels / updates),
                         (unsigned)(drawn_regions / updates),
                         (long)s_lvgl_largest_region.x1, (long)s_lvgl_largest_region.y1,
                         (long)s_lvgl_largest_region.x2, (long)s_lvgl_largest_region.y2,
                         (unsigned)s_lvgl_largest_region_pixels);
            }
            if (s_present_interval_count) {
                ESP_LOGI(TAG, "Present interval: avg %u ms, min %u ms, max %u ms (%u intervals)",
                         (unsigned)(s_present_interval_sum_us / s_present_interval_count / 1000),
                         (unsigned)(s_present_interval_min_us / 1000),
                         (unsigned)(s_present_interval_max_us / 1000),
                         (unsigned)s_present_interval_count);
            }
            if (s_refresh_gap_count) {
                ESP_LOGI(TAG, "Between presented refreshes: avg %u ms, max %u ms (%u gaps)",
                         (unsigned)(s_refresh_gap_sum_us / s_refresh_gap_count / 1000),
                         (unsigned)(s_refresh_gap_max_us / 1000),
                         (unsigned)s_refresh_gap_count);
            }
            if (delay_count) {
                ESP_LOGI(TAG, "LVGL task delay: requested avg %u ms, actual avg %u ms, max %u ms (%u delays)",
                         (unsigned)(delay_requested_sum_us / delay_count / 1000),
                         (unsigned)(delay_actual_sum_us / delay_count / 1000),
                         (unsigned)(delay_actual_max_us / 1000),
                         (unsigned)delay_count);
            }
            if (s_touch_interval_count) {
                ESP_LOGI(TAG, "Touch poll: configured %u ms, observed avg %u ms, max %u ms",
                         (unsigned)s_touch_period_ms,
                         (unsigned)(s_touch_interval_sum_us / s_touch_interval_count / 1000),
                         (unsigned)(s_touch_interval_max_us / 1000));
            }
            ESP_LOGI(TAG, "Internal heap: free %u bytes, largest block %u bytes",
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            last_diagnostics_ms = now_ms;
            last_vsync = vsync;
            last_bounce_frame = bounce_frame;
            last_lvgl_frame = s_lvgl_frame_count;
            last_sync_wait_us = s_lvgl_sync_wait_us;
            last_prepare_us = s_lvgl_prepare_us;
            last_render_us = s_lvgl_render_us;
            last_refresh_us = s_lvgl_refresh_us;
            last_drawn_pixels = s_lvgl_drawn_pixels;
            last_drawn_regions = s_lvgl_drawn_regions;
            s_lvgl_refresh_max_us = 0;
            s_lvgl_largest_region_pixels = 0;
            s_present_interval_sum_us = 0;
            s_present_interval_count = 0;
            s_present_interval_min_us = 0;
            s_present_interval_max_us = 0;
            s_refresh_gap_sum_us = 0;
            s_refresh_gap_count = 0;
            s_refresh_gap_max_us = 0;
            delay_requested_sum_us = 0;
            delay_actual_sum_us = 0;
            delay_count = 0;
            delay_actual_max_us = 0;
            s_touch_interval_sum_us = 0;
            s_touch_interval_count = 0;
            s_touch_interval_max_us = 0;
        }
        if (delay_ms > LVGL_MAX_SLEEP_MS) {
            delay_ms = LVGL_MAX_SLEEP_MS;
        } else if (delay_ms == 0) {
            delay_ms = 1;
        }
        TickType_t delay_ticks = pdMS_TO_TICKS(delay_ms);
        if (delay_ticks == 0) {
            delay_ticks = 1;
        }
        uint64_t delay_start_us = (uint64_t)esp_timer_get_time();
        vTaskDelay(delay_ticks);
        uint32_t actual_delay_us = (uint32_t)((uint64_t)esp_timer_get_time() - delay_start_us);
        delay_requested_sum_us += (uint64_t)delay_ticks * 1000000 / CONFIG_FREERTOS_HZ;
        delay_actual_sum_us += actual_delay_us;
        delay_count++;
        if (actual_delay_us > delay_actual_max_us) {
            delay_actual_max_us = actual_delay_us;
        }
    }
}

void app_main(void)
{
    init_i2c_and_backlight();
    s_panel = init_rgb_panel();
    s_frame_done = xSemaphoreCreateBinary();
    s_switch_done = xSemaphoreCreateBinary();
    s_switch_queue = xQueueCreate(1, sizeof(uint8_t *));
    ESP_ERROR_CHECK(s_frame_done && s_switch_done && s_switch_queue
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    const esp_lcd_rgb_panel_event_callbacks_t panel_callbacks = {
        .on_vsync = lcd_vsync_cb,
        .on_frame_buf_complete = lcd_frame_done_cb,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(
        s_panel, &panel_callbacks, s_frame_done));
    /* The RGB and GDMA interrupts were allocated on this pinned core. */
    BaseType_t switch_task_created = xTaskCreatePinnedToCore(
        lcd_switch_task, "lcd_switch", LCD_SWITCH_TASK_STACK_SIZE, NULL,
        LCD_SWITCH_TASK_PRIORITY, NULL, xPortGetCoreID());
    ESP_ERROR_CHECK(switch_task_created == pdPASS ? ESP_OK : ESP_FAIL);

    BaseType_t task_created = xTaskCreatePinnedToCore(
        lvgl_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL,
        LVGL_TASK_PRIORITY, NULL, LVGL_TASK_CORE);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_FAIL);
}
