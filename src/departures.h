#ifndef DEPARTURES_H
#define DEPARTURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEPARTURES_STATION_COUNT 2
#define DEPARTURES_PER_STATION_MAX 8
#define DEPARTURES_MAX (DEPARTURES_STATION_COUNT * DEPARTURES_PER_STATION_MAX)

static inline const char *departures_station_id(unsigned station)
{
    return station == 0 ? "900120515" : "900120016";
}

static inline const char *departures_station_name(unsigned station)
{
    return station == 0 ? "Strassmannstr." : "Landsberger Allee/Petersburger Str.";
}

typedef struct {
    char line[16];
    char direction[96];
    int64_t when;
    bool cancelled;
    bool planned_only;
    unsigned station;
    bool stale;
} departure_t;

typedef struct {
    departure_t items[DEPARTURES_PER_STATION_MAX];
    size_t count;
} departures_station_t;

typedef struct {
    departure_t items[DEPARTURES_MAX];
    size_t count;
} departures_t;

typedef enum {
    DEPARTURES_CONNECTING,
    DEPARTURES_LIVE,
    DEPARTURES_DEMO,
    DEPARTURES_STALE,
    DEPARTURES_PARTIAL,
    DEPARTURES_ERROR,
    DEPARTURES_NO_CONFIG
} departures_status_t;

typedef struct {
    departures_t departures;
    departures_status_t status;
} departures_update_t;

#endif
