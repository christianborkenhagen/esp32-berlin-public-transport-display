cmake_minimum_required(VERSION 3.20)
project(transport_lvgl_pc LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

# Reuse the ESP-IDF managed component if it has already been downloaded.
set(LVGL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../managed_components/lvgl__lvgl"
    CACHE PATH "LVGL 9.4.0 source directory")
if(EXISTS "${LVGL_SOURCE_DIR}/lvgl.h")
    file(READ "${LVGL_SOURCE_DIR}/lv_version.h" LVGL_VERSION_HEADER)
    foreach(VERSION_DEFINE "MAJOR 9" "MINOR 4" "PATCH 0")
        if(NOT LVGL_VERSION_HEADER MATCHES "#define LVGL_VERSION_${VERSION_DEFINE}([ \r\n]|$)")
            message(FATAL_ERROR "The PC simulator needs LVGL 9.4.0: ${LVGL_SOURCE_DIR}")
        endif()
    endforeach()
    add_subdirectory("${LVGL_SOURCE_DIR}" lvgl)
else()
    include(FetchContent)
    FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG v9.4.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(lvgl)
endif()

add_executable(lvgl_pc main.c ../../src/transport_screen.c ../../src/benchmark_screen.c)
target_include_directories(lvgl_pc PRIVATE ../../src)
target_compile_definitions(lvgl_pc PRIVATE APP_WINDOWS_SIMULATOR=1)
target_link_libraries(lvgl_pc PRIVATE lvgl user32 gdi32 shell32 shcore ole32)
