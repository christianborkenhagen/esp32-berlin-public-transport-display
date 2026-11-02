set(app_sources "main.c" "benchmark_screen.c" "benchmark_metrics.c")
set(app_requires esp_lcd esp_driver_i2c lvgl)
if(APP_TRANSPORT)
    # Keep an actual header in the dependency graph even before configuration,
    # so editing credentials triggers recompilation on subsequent builds.
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/wifi_config.h")
        file(WRITE "${CMAKE_CURRENT_LIST_DIR}/wifi_config.h"
             "#ifndef WIFI_CONFIG_H\n#define WIFI_CONFIG_H\n#define APP_WIFI_SSID \"\"\n#define APP_WIFI_PASSWORD \"\"\n#endif\n")
    endif()
    list(APPEND app_sources "transport_screen.c" "departures_parser.c" "departures_merge.c" "departures_service.c")
    list(APPEND app_requires esp_wifi esp_event esp_netif esp_http_client json mbedtls nvs_flash)
endif()
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." REQUIRES ${app_requires})
if(APP_TRANSPORT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_TRANSPORT=1)
endif()
if(APP_LVGL_O3)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_LVGL_O3=1)
endif()
