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
#include "freertos/task.h"
#include "lvgl.h"
#include "lv_demos.h"

#define LCD_H_RES               800
#define LCD_V_RES               480
/* Match Elecrow's V1.1 ESP-IDF RGB panel configuration. */
#define LCD_PIXEL_CLOCK_HZ      (16 * 1000 * 1000)
#define LCD_BOUNCE_BUFFER_LINES  10
#define LCD_DRAW_BUFFER_LINES    20

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

static const char *TAG = "panel_test";

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_touch_device;
static esp_lcd_panel_handle_t s_panel;
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
        .num_fbs = 1,
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

    void *frame_buffer = NULL;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(
        panel, 1, &frame_buffer));
    const size_t frame_buffer_size =
        LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    memset(frame_buffer, 0, frame_buffer_size);

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    return panel;
}

static uint32_t lvgl_tick_get_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area,
                          uint8_t *pixel_map)
{
    esp_err_t err = esp_lcd_panel_draw_bitmap(
        s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixel_map);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD flush failed: %s", esp_err_to_name(err));
    }
    /* Single-framebuffer mode copies this area synchronously into the framebuffer. */
    lv_display_flush_ready(display);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    lv_init();
    lv_tick_set_cb(lvgl_tick_get_ms);

    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_ERROR_CHECK(display == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

    const size_t draw_buffer_size =
        LCD_H_RES * LCD_DRAW_BUFFER_LINES * sizeof(uint16_t);
    void *draw_buffer_0 = heap_caps_malloc(
        draw_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    void *draw_buffer_1 = heap_caps_malloc(
        draw_buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK((draw_buffer_0 != NULL && draw_buffer_1 != NULL)
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    lv_display_set_buffers(display, draw_buffer_0, draw_buffer_1,
                           draw_buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, lvgl_flush_cb);

    if (s_touch_device != NULL) {
        lv_indev_t *touch = lv_indev_create();
        ESP_ERROR_CHECK(touch == NULL ? ESP_ERR_NO_MEM : ESP_OK);
        lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch, lvgl_touch_read_cb);
        lv_timer_t *touch_read_timer = lv_indev_get_read_timer(touch);
        if (touch_read_timer != NULL) {
            lv_timer_set_period(touch_read_timer, LVGL_TOUCH_READ_PERIOD_MS);
        }
    }

    lv_demo_widgets();
    ESP_LOGI(TAG, "LVGL widgets demo running on the 800x480 panel at %u MHz PCLK",
             (unsigned)(LCD_PIXEL_CLOCK_HZ / 1000000));
    ESP_LOGI(TAG, "LVGL partial draw buffers: %u bytes each",
             (unsigned)draw_buffer_size);
    ESP_LOGI(TAG, "PSRAM detected: %u bytes", (unsigned)esp_psram_get_size());

    while (true) {
        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms > LVGL_MAX_SLEEP_MS) {
            delay_ms = LVGL_MAX_SLEEP_MS;
        } else if (delay_ms == 0) {
            delay_ms = 1;
        }
        TickType_t delay_ticks = pdMS_TO_TICKS(delay_ms);
        if (delay_ticks == 0) {
            delay_ticks = 1;
        }
        vTaskDelay(delay_ticks);
    }
}

void app_main(void)
{
    init_i2c_and_backlight();
    s_panel = init_rgb_panel();
    BaseType_t task_created = xTaskCreatePinnedToCore(
        lvgl_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL,
        LVGL_TASK_PRIORITY, NULL, LVGL_TASK_CORE);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_FAIL);
}
