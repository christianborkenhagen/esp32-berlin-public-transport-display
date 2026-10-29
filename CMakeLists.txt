cmake_minimum_required(VERSION 3.16)

# Fresh -O3 builds need smaller draw-worker stacks because the larger IRAM
# image leaves less internal RAM. Existing sdkconfig files take precedence.
if(APP_LVGL_O3)
    set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults;${CMAKE_SOURCE_DIR}/sdkconfig.o3.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(crowpanel_advance_5_v1_1_test)

# Software rendering is performance-sensitive; keep the rest of the project
# at its configured optimization level while optimizing the LVGL component.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
if(APP_LVGL_O3)
    target_compile_options(${lvgl_lib} PRIVATE -O3)
else()
    target_compile_options(${lvgl_lib} PRIVATE -O2)
endif()
