#include "benchmark_metrics.h"

#include <limits.h>

bool benchmark_metrics_sample(benchmark_metrics_state_t *state,
                              uint64_t time_us, uint32_t idle0_us,
                              uint32_t idle1_us, uint32_t frames,
                              benchmark_metrics_result_t *result)
{
    uint64_t elapsed = time_us - state->time_us;
    bool valid = state->initialized && time_us > state->time_us &&
                 elapsed <= UINT32_MAX;

    if (valid) {
        uint64_t idle0 = (uint32_t)(idle0_us - state->idle_us[0]);
        uint64_t idle1 = (uint32_t)(idle1_us - state->idle_us[1]);
        /* Sampling and context switches are not simultaneous on both cores. */
        if (idle0 > elapsed) idle0 = elapsed;
        if (idle1 > elapsed) idle1 = elapsed;

        uint64_t capacity = elapsed * 2;
        uint64_t busy = capacity - idle0 - idle1;
        result->cpu_percent = (uint32_t)((busy * 100 + capacity / 2) / capacity);
        result->cpu_core_percent[0] =
            (uint32_t)(((elapsed - idle0) * 100 + elapsed / 2) / elapsed);
        result->cpu_core_percent[1] =
            (uint32_t)(((elapsed - idle1) * 100 + elapsed / 2) / elapsed);
        uint32_t frame_delta = frames - state->frames;
        result->fps_tenths =
            (uint32_t)(((uint64_t)frame_delta * 10000000 + elapsed / 2) / elapsed);
    }

    state->time_us = time_us;
    state->idle_us[0] = idle0_us;
    state->idle_us[1] = idle1_us;
    state->frames = frames;
    state->initialized = true;
    return valid;
}
