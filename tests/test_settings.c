#include "app_settings.h"
#include "departures_service.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    app_settings_t settings, out;
    assert(app_settings_init());
    app_settings_get(&settings);
    assert(settings.station_count == 2);
    assert(!strcmp(settings.stations[0].id, "900120515"));
    assert(!strcmp(settings.stations[1].id, "900120016"));
    assert(app_settings_valid(&settings));
    strcpy(settings.ssid, "Demo Wi-Fi");
    strcpy(settings.password, "demo-password");
    assert(app_settings_save(&settings));
    memset(&out, 0, sizeof(out));
    app_settings_get(&out);
    assert(!strcmp(out.ssid, settings.ssid));
    assert(!strcmp(out.password, settings.password));
    out.station_count = APP_MAX_STATIONS + 1;
    assert(!app_settings_save(&out));
    app_settings_get(&out);
    assert(out.station_count == 2);
    out.stations[1] = out.stations[0];
    assert(!app_settings_save(&out));
    app_settings_get(&out);
    memset(out.ssid, 'x', sizeof(out.ssid));
    assert(!app_settings_save(&out));
    assert(departures_service_start());
    departures_update_t *update = calloc(1, sizeof(*update));
    assert(update && departures_service_receive(update));
    assert(update->status == DEPARTURES_DEMO && update->departures.count == 24);
    for (size_t i = 1; i < update->departures.count; i++)
        assert(update->departures.items[i-1].when <= update->departures.items[i].when);
    app_service_result_t result;
    assert(!departures_service_receive_result(&result));
    assert(departures_service_scan());
    assert(departures_service_receive_result(&result));
    assert(result.success && result.type == APP_RESULT_SCAN && result.count == 1);
    assert(!departures_service_search(""));
    assert(departures_service_search("Berlin"));
    assert(departures_service_receive_result(&result));
    assert(result.success && result.type == APP_RESULT_SEARCH && result.count == 2);
    free(update);
    puts("Settings and mock service tests passed");
    return 0;
}
