#ifndef DEPARTURES_H
#define DEPARTURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEPARTURES_STATION_COUNT 4
#define DEPARTURES_PER_STATION_MAX 32
#define DEPARTURES_MAX (DEPARTURES_STATION_COUNT * DEPARTURES_PER_STATION_MAX)

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
    int64_t last_updated; /* Unix time of the most recent successful station fetch. */
    bool station_valid[DEPARTURES_STATION_COUNT];
    bool station_stale[DEPARTURES_STATION_COUNT];
} departures_update_t;

#endif
