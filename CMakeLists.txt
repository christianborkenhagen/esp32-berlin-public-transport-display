cmake_minimum_required(VERSION 3.16)

# Fresh -O3 builds need smaller draw-worker stacks because the larger IRAM
# image leaves less internal RAM. Existing sdkconfig files take precedence.
if((APP_LVGL_O3 AND APP_TICK_1000) OR (APP_TRANSPORT AND (APP_LVGL_O3 OR APP_TICK_1000)))
    message(FATAL_ERROR "The transport, -O3, and 1000 Hz variants are separate builds")
elseif(APP_TRANSPORT)
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_SOURCE_DIR}/sdkconfig.transport.defaults")
elseif(APP_LVGL_O3)
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_SOURCE_DIR}/sdkconfig.o3.defaults")
elseif(APP_TICK_1000)
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_SOURCE_DIR}/sdkconfig.tick_1000.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(crowpanel_advance_5_v1_1_test)

if(APP_TICK_1000 AND NOT CONFIG_FREERTOS_HZ EQUAL 1000)
    message(FATAL_ERROR "The 1000 Hz benchmark requires CONFIG_FREERTOS_HZ=1000 in its sdkconfig")
endif()

# Software rendering is performance-sensitive; keep the rest of the project
# at its configured optimization level while optimizing the LVGL component.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(APP_TRANSPORT)
    # Bound the parser's recursion for untrusted network responses.
    idf_component_get_property(json_lib json COMPONENT_LIB)
    target_compile_definitions(${json_lib} PRIVATE CJSON_NESTING_LIMIT=32)
    # Avoid a Windows Xtensa GCC internal compiler error in the RGB driver at
    # -Og with PLATFORMIO defined, without changing the project's optimization.
    idf_component_get_property(lcd_lib esp_lcd COMPONENT_LIB)
    target_compile_options(${lcd_lib} PRIVATE -fno-ira-share-spill-slots)
endif()
if(APP_LVGL_O3)
    target_compile_options(${lvgl_lib} PRIVATE -O3)
else()
    target_compile_options(${lvgl_lib} PRIVATE -O2)
endif()
