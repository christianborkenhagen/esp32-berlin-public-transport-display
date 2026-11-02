cmake_minimum_required(VERSION 3.16)
set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_SOURCE_DIR}/sdkconfig.transport.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(berlin_transport_display)
# Avoid a Windows Xtensa GCC issue in the RGB driver at -Og.
idf_component_get_property(lcd_lib esp_lcd COMPONENT_LIB)
target_compile_options(${lcd_lib} PRIVATE -fno-ira-share-spill-slots)
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -O2)
# Keep LVGL's primary pool in PSRAM, leaving internal RAM for Wi-Fi and DMA.
target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_SOURCE_DIR}/src/platform")
target_compile_definitions(${lvgl_lib} PRIVATE
    "LV_MEM_POOL_INCLUDE=\"lvgl_psram_pool.h\""
    "LV_MEM_POOL_ALLOC=app_lvgl_get_pool")
idf_component_get_property(json_lib json COMPONENT_LIB)
target_compile_definitions(${json_lib} PRIVATE CJSON_NESTING_LIMIT=32)
