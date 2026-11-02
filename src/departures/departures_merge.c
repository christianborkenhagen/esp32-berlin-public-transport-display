#include "departures_merge.h"

void departures_merge(const departures_station_t stations[DEPARTURES_STATION_COUNT],
                      const bool stale[DEPARTURES_STATION_COUNT], departures_t *out)
{
    out->count = 0;
    for (unsigned station = 0; station < DEPARTURES_STATION_COUNT; ++station) {
        size_t count = stations[station].count;
        if (count > DEPARTURES_PER_STATION_MAX) count = DEPARTURES_PER_STATION_MAX;
        for (size_t i = 0; i < count; ++i) {
            departure_t item = stations[station].items[i];
            item.station = station;
            item.stale = stale[station];
            size_t pos = out->count;
            while (pos > 0 && out->items[pos - 1].when > item.when) {
                out->items[pos] = out->items[pos - 1];
                --pos;
            }
            out->items[pos] = item;
            ++out->count;
        }
    }
}
