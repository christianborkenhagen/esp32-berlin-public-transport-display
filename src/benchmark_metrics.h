#ifndef BENCHMARK_METRICS_H
#define BENCHMARK_METRICS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t time_us;
    uint32_t idle_us[2];
    uint32_t frames;
    bool initialized;
} benchmark_metrics_state_t;

typedef struct {
    uint32_t fps_tenths;
    uint32_t cpu_percent;
    uint32_t cpu_core_percent[2];
} benchmark_metrics_result_t;

/*
 * Idle counters are 32-bit FreeRTOS runtime counters in microseconds.
 * A sample interval must be shorter than one counter wrap.
 */
bool benchmark_metrics_sample(benchmark_metrics_state_t *state,
                              uint64_t time_us, uint32_t idle0_us,
                              uint32_t idle1_us, uint32_t frames,
                              benchmark_metrics_result_t *result);

#endif
