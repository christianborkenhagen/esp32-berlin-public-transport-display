#ifndef UPDATE_TIME_H
#define UPDATE_TIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Format a Unix timestamp in Berlin local time, including daylight saving. */
bool update_time_format(int64_t timestamp, char *out, size_t capacity);

#endif
