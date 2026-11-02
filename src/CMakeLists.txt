idf_component_register(SRCS "main.c" "ui/transport_screen.c"
    "departures/update_time.c" "departures/departures_parser.c"
    "departures/departures_merge.c" "departures/departures_service.c"
    "settings/app_settings_common.c" "settings/app_settings_esp.c"
    "platform/lvgl_psram_pool.c"
    INCLUDE_DIRS "." "ui" "departures" "settings" "platform"
    REQUIRES esp_lcd esp_driver_i2c lvgl esp_wifi esp_event esp_netif esp_http_client json mbedtls nvs_flash nvs_sec_provider efuse)
