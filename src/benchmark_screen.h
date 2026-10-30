#ifndef BENCHMARK_SCREEN_H
#define BENCHMARK_SCREEN_H

#include <stdint.h>

typedef uint32_t (*benchmark_frame_count_cb_t)(void);

/* Call once from the LVGL task after registering the display. */
void benchmark_screen_create(benchmark_frame_count_cb_t frame_count);

#ifdef APP_WINDOWS_SIMULATOR
/* Select the first scene before creating the desktop benchmark. */
void benchmark_screen_set_initial_scene(unsigned scene);
#endif

#endif
