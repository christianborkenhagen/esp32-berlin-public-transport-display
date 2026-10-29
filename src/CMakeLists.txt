idf_component_register(
    SRCS "main.c" "benchmark_screen.c" "benchmark_metrics.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_driver_i2c lvgl
)
if(APP_LVGL_O3)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_LVGL_O3=1)
endif()
