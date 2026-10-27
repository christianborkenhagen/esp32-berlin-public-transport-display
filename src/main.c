#include <stdint.h>
#include <stdlib.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_H_RES               800
#define LCD_V_RES               480
#define LCD_PIXEL_CLOCK_HZ      (20 * 1000 * 1000)

/* Elecrow's V1.1 RGB pin map from its board example. */
#define LCD_PCLK_GPIO           39
#define LCD_HSYNC_GPIO          40
#define LCD_VSYNC_GPIO          41
#define LCD_DE_GPIO             42

#define I2C_SDA_GPIO            15
#define I2C_SCL_GPIO            16
#define BACKLIGHT_I2C_ADDRESS   0x30
#define TOUCH_I2C_ADDRESS       0x5D

#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static const char *TAG = "panel_test";

static i2c_master_bus_handle_t s_i2c_bus;

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
    err = i2c_master_transmit(backlight_device, &maximum_brightness,
                              sizeof(maximum_brightness), 100);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Backlight set to maximum");
    } else {
        ESP_LOGW(TAG, "Could not set backlight: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(backlight_device));
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
                .pclk_active_neg = 0,
                .pclk_idle_high = 1,
            },
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 10 * LCD_H_RES,
        .dma_burst_size = 64,
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
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    return panel;
}

static void draw_color_bars(esp_lcd_panel_handle_t panel)
{
    static const uint16_t colors[8] = {
        RGB565(255, 255, 255),
        RGB565(255, 255, 0),
        RGB565(0, 255, 255),
        RGB565(0, 255, 0),
        RGB565(255, 0, 255),
        RGB565(255, 0, 0),
        RGB565(0, 0, 255),
        RGB565(0, 0, 0),
    };

    const size_t pixel_count = LCD_H_RES * LCD_V_RES;
    uint16_t *frame = heap_caps_malloc(
        pixel_count * sizeof(*frame), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(frame == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    for (int y = 0; y < LCD_V_RES; ++y) {
        for (int x = 0; x < LCD_H_RES; ++x) {
            const int bar = (x * 8) / LCD_H_RES;
            uint16_t color = colors[bar];

            /* Add a monochrome footer to make color/channel problems obvious. */
            if (y >= LCD_V_RES - 40) {
                color = ((x / 20) % 2 == 0)
                    ? RGB565(255, 255, 255)
                    : RGB565(0, 0, 0);
            }
            frame[y * LCD_H_RES + x] = color;
        }
    }

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
        panel, 0, 0, LCD_H_RES, LCD_V_RES, frame));
    heap_caps_free(frame);
    ESP_LOGI(TAG, "Color-bar test pattern sent to the 800x480 panel");
}

void app_main(void)
{
    ESP_LOGI(TAG, "CrowPanel Advance 5.0 V1.1 / DIS02050A test");
    ESP_LOGI(TAG, "PSRAM detected: %u bytes",
             (unsigned)esp_psram_get_size());

    init_i2c_and_backlight();
    esp_lcd_panel_handle_t panel = init_rgb_panel();
    draw_color_bars(panel);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
