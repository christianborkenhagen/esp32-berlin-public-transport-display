#ifndef DEPARTURES_MERGE_H
#define DEPARTURES_MERGE_H

#include "departures.h"

/* Merge station snapshots in chronological order; preserve equal-time order. */
void departures_merge(const departures_station_t stations[DEPARTURES_STATION_COUNT],
                      const bool stale[DEPARTURES_STATION_COUNT], departures_t *out);

#endif
