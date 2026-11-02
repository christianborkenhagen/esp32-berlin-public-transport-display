#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "departures_parser.h"
#include "departures_merge.h"

int main(int argc, char **argv)
{
    departures_station_t result = {0};
    const char *sample =
        "{\"departures\":["
        "{\"line\":{\"name\":\"M10\"},\"direction\":\"Stra\\u00dfmannstr.\","
        "\"when\":\"2026-11-02T09:34:00+01:00\",\"plannedWhen\":\"2026-11-02T09:33:00+01:00\"},"
        "{\"line\":{\"name\":\"U8\"},\"direction\":null,\"destination\":{\"name\":\"Witt\\u00e4u\"},"
        "\"when\":null,\"plannedWhen\":\"2026-11-02T08:34:00Z\",\"cancelled\":true},"
        "{\"line\":{\"name\":\"M10\"},\"direction\":\"X\",\"when\":\"not-a-date\"},"
        "{\"line\":{\"name\":\"S1\"},\"direction\":\"Y\",\"when\":\"2026-11-02T10:34:00.000+02:00\"}"
        "]}";
    assert(departures_parse(sample, &result));
    assert(result.count == 3);
    assert(strcmp(result.items[0].line, "M10") == 0);
    assert(strcmp(result.items[0].direction, "Strassmannstr.") == 0);
    assert(strcmp(result.items[1].direction, "Wittaeu") == 0);
    assert(!result.items[0].planned_only && !result.items[0].cancelled);
    assert(result.items[1].planned_only && result.items[1].cancelled);
    assert(result.items[0].when == 1793608440);
    assert(result.items[0].when == result.items[1].when);
    assert(result.items[0].when == result.items[2].when);

    assert(departures_parse("{\"departures\":[]}", &result));
    assert(result.count == 0);
    assert(!departures_parse("{\"departures\":[{}]}", &result));
    assert(!departures_parse("{\"error\":\"nope\"}", &result));
    assert(!departures_parse("{\"departures\":[]} garbage", &result));
    assert(!departures_parse("{", &result));
    assert(!departures_parse(NULL, &result));
    assert(!departures_parse("{}", NULL));

    const char *bad_date =
        "{\"departures\":[{\"line\":{\"name\":\"M10\"},\"direction\":\"X\","
        "\"when\":\"2026-02-29T09:34:00+01:00\"}]}";
    assert(!departures_parse(bad_date, &result));
    const char *bad_offset =
        "{\"departures\":[{\"line\":{\"name\":\"M10\"},\"direction\":\"X\","
        "\"when\":\"2026-02-28T09:34:00+24:00\"}]}";
    assert(!departures_parse(bad_offset, &result));
    char many[8192] = "{\"departures\":[";
    for (unsigned i = 0; i < DEPARTURES_PER_STATION_MAX + 2; ++i) {
        strcat(many, i ? "," : "");
        strcat(many, "{\"line\":{\"name\":\"M10\"},\"direction\":\"X\","
                     "\"when\":\"2026-11-02T09:34:00+01:00\"}");
    }
    strcat(many, "]}");
    assert(departures_parse(many, &result));
    assert(result.count == DEPARTURES_PER_STATION_MAX);
    departures_station_t stations[DEPARTURES_STATION_COUNT] = {0};
    departures_t merged = {0};
    bool stale[DEPARTURES_STATION_COUNT] = {false, true};
    stations[0].count = 2;
    stations[0].items[0].when = 20;
    stations[0].items[1].when = 10;
    stations[1].count = 2;
    stations[1].items[0].when = 20;
    stations[1].items[1].when = 5;
    departures_merge(stations, stale, &merged);
    assert(merged.count == 4);
    assert(merged.items[0].when == 5 && merged.items[0].station == 1 && merged.items[0].stale);
    assert(merged.items[1].when == 10 && merged.items[1].station == 0 && !merged.items[1].stale);
    assert(merged.items[2].when == 20 && merged.items[2].station == 0);
    assert(merged.items[3].when == 20 && merged.items[3].station == 1);
    assert(stations[0].items[0].when == 20);
    for (unsigned s = 0; s < DEPARTURES_STATION_COUNT; ++s) {
        stations[s].count = DEPARTURES_PER_STATION_MAX + 1;
        for (unsigned i = 0; i < DEPARTURES_PER_STATION_MAX; ++i) {
            stations[s].items[i].when = (int64_t)(i * 2 + s);
        }
    }
    departures_merge(stations, stale, &merged);
    assert(merged.count == DEPARTURES_MAX);
    for (unsigned i = 1; i < DEPARTURES_MAX; ++i)
        assert(merged.items[i - 1].when <= merged.items[i].when);
    for (unsigned s = 0; s < DEPARTURES_STATION_COUNT; ++s)
        stations[s].count = 0;
    departures_merge(stations, stale, &merged);
    assert(merged.count == 0);
    char deeply_nested[256] = "{\"a\":";
    for (int i = 0; i < 40; ++i) strcat(deeply_nested, "[");
    strcat(deeply_nested, "0");
    for (int i = 0; i < 40; ++i) strcat(deeply_nested, "]");
    strcat(deeply_nested, ",\"departures\":[]}");
    assert(!departures_parse(deeply_nested, &result));
    if (argc > 1) {
        FILE *file = fopen(argv[1], "rb");
        assert(file);
        char response[32769];
        size_t bytes = fread(response, 1, sizeof(response) - 1, file);
        assert(feof(file));
        fclose(file);
        response[bytes] = '\0';
        assert(departures_parse(response, &result));
        printf("live response: %u usable departures\n", (unsigned)result.count);
    }
    puts("departure parser tests passed");
    return 0;
}
