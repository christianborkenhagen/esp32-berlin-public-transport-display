#include "departures_service.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static bool s_pending = true, s_result_ready;
static app_service_result_t s_result;
bool departures_service_start(void) { s_pending = true; return true; }
bool departures_service_receive(departures_update_t *update)
{
    if (!s_pending || !update) return false;
    s_pending = false;
    memset(update, 0, sizeof(*update));
    app_settings_t settings; app_settings_get(&settings);
    update->status = DEPARTURES_DEMO;
    const char *lines[] = {"M10", "M4", "S7", "U5"};
    const char *directions[] = {"Hauptbahnhof", "Ostkreuz", "Alexanderplatz", "Potsdam Hbf"};
    int64_t now = (int64_t)time(NULL);
    if (settings.station_count) update->last_updated = now;
    for (size_t n = 0; n < 12; n++) for (size_t st = 0; st < settings.station_count; st++) {
        departure_t *d = &update->departures.items[update->departures.count++];
        snprintf(d->line, sizeof(d->line), "%s", lines[n % 4]);
        snprintf(d->direction, sizeof(d->direction), "%s", directions[n % 4]);
        d->station = (unsigned)st; d->when = now + (int64_t)(n * 2 + st + 1) * 60;
        d->planned_only = n == 1; d->cancelled = n == 3;
        update->station_valid[st] = true;
    }
    return true;
}
bool departures_service_scan(void)
{
    memset(&s_result, 0, sizeof(s_result)); s_result.type = APP_RESULT_SCAN; s_result.success = true;
    strcpy(s_result.items[0].name, "Demo Wi-Fi"); s_result.count = 1; s_result_ready = true; return true;
}
bool departures_service_search(const char *query)
{
    if (!query || !*query) return false;
    memset(&s_result, 0, sizeof(s_result)); s_result.type = APP_RESULT_SEARCH; s_result.success = true;
    if (!strcmp(query, "offline")) { s_result.success = false; s_result_ready = true; return true; }
    strcpy(s_result.items[0].id, "900100001"); strcpy(s_result.items[0].name, "Demo Alexanderplatz");
    strcpy(s_result.items[1].id, "900100002"); strcpy(s_result.items[1].name, "Demo Ostkreuz");
    s_result.count = 2; s_result_ready = true; return true;
}
void departures_service_settings_changed(void) { s_pending = true; }
bool departures_service_receive_result(app_service_result_t *result)
{
    if (!s_result_ready || !result) return false;
    s_result_ready = false; *result = s_result; return true;
}
