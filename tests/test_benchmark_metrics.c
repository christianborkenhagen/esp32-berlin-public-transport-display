#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "benchmark_metrics.h"

int main(void)
{
    benchmark_metrics_state_t state = {0};
    benchmark_metrics_result_t result = {0};

    assert(!benchmark_metrics_sample(&state, 1000000, 1000, 2000, 10, &result));
    assert(benchmark_metrics_sample(&state, 2000000, 251000, 752000, 20, &result));
    assert(result.fps_tenths == 100);
    assert(result.cpu_percent == 50);
    assert(result.cpu_core_percent[0] == 75);
    assert(result.cpu_core_percent[1] == 25);

    /* 32-bit counters and the frame counter can wrap between samples. */
    state.time_us = 3000000;
    state.idle_us[0] = UINT32_MAX - 99;
    state.idle_us[1] = UINT32_MAX - 199;
    state.frames = UINT32_MAX - 4;
    assert(benchmark_metrics_sample(&state, 4000000, 499900, 499800, 5, &result));
    assert(result.fps_tenths == 100);
    assert(result.cpu_percent == 50);
    assert(result.cpu_core_percent[0] == 50);
    assert(result.cpu_core_percent[1] == 50);

    /* Counter jitter must never produce negative or over-100% CPU. */
    assert(benchmark_metrics_sample(&state, 5000000, 2499900, 2499800, 5, &result));
    assert(result.cpu_percent == 0);
    assert(result.cpu_core_percent[0] == 0);
    assert(result.cpu_core_percent[1] == 0);
    assert(result.fps_tenths == 0);
    assert(!benchmark_metrics_sample(&state, 5000000, 0, 0, 0, &result));
    assert(!benchmark_metrics_sample(&state, (uint64_t)5000000 + UINT32_MAX + 1,
                                     0, 0, 0, &result));

    /* One saturated core and one idle core still average to 50%. */
    state = (benchmark_metrics_state_t){0};
    assert(!benchmark_metrics_sample(&state, 1000000, 0, 0, 0, &result));
    assert(benchmark_metrics_sample(&state, 2000000, 0, 1000000, 1, &result));
    assert(result.cpu_percent == 50);
    assert(result.cpu_core_percent[0] == 100);
    assert(result.cpu_core_percent[1] == 0);

    puts("benchmark metrics: OK");
    return 0;
}
