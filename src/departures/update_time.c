#define _POSIX_C_SOURCE 200809L
#include "update_time.h"
#include <stdio.h>
#include <time.h>

static bool utc_time(time_t value, struct tm *result)
{
#ifdef _WIN32
    return gmtime_s(result, &value) == 0;
#else
    return gmtime_r(&value, result) != NULL;
#endif
}

bool update_time_format(int64_t timestamp, char *out, size_t capacity)
{
    if (!out || !capacity || timestamp <= 0) return false;
    time_t value = (time_t)timestamp;
    if ((int64_t)value != timestamp) return false;
    struct tm utc;
    if (!utc_time(value, &utc)) return false;

    /* EU summer time runs from 01:00 UTC on March's last Sunday until
     * 01:00 UTC on October's last Sunday. Both months have 31 days. */
    int month = utc.tm_mon + 1;
    int last_sunday = 31 - (utc.tm_wday + 31 - utc.tm_mday) % 7;
    bool summer = month > 3 && month < 10;
    if (month == 3)
        summer = utc.tm_mday > last_sunday || (utc.tm_mday == last_sunday && utc.tm_hour >= 1);
    else if (month == 10)
        summer = utc.tm_mday < last_sunday || (utc.tm_mday == last_sunday && utc.tm_hour < 1);

    int64_t local_timestamp = timestamp + (summer ? 7200 : 3600);
    time_t local_value = (time_t)local_timestamp;
    struct tm local;
    if ((int64_t)local_value != local_timestamp || !utc_time(local_value, &local)) return false;
    int written = snprintf(out, capacity, "%02d.%02d.%04d %02d:%02d",
                           local.tm_mday, local.tm_mon + 1, local.tm_year + 1900,
                           local.tm_hour, local.tm_min);
    return written > 0 && (size_t)written < capacity;
}
