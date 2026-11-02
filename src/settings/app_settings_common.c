#include "app_settings.h"
#include <string.h>

void app_settings_defaults(app_settings_t *out)
{
    memset(out, 0, sizeof(*out));
    out->station_count = 2;
    strcpy(out->stations[0].id, "900120515");
    strcpy(out->stations[0].name, "Strassmannstr.");
    strcpy(out->stations[1].id, "900120016");
    strcpy(out->stations[1].name, "Landsberger Allee/Petersburger Str.");
}

bool app_settings_valid(const app_settings_t *s)
{
    if (!s || s->station_count > APP_MAX_STATIONS ||
        !memchr(s->ssid, 0, sizeof(s->ssid)) ||
        !memchr(s->password, 0, sizeof(s->password))) return false;
    for (size_t i = 0; i < s->station_count; i++) {
        if (!s->stations[i].id[0] || !s->stations[i].name[0] ||
            !memchr(s->stations[i].id, 0, sizeof(s->stations[i].id)) ||
            !memchr(s->stations[i].name, 0, sizeof(s->stations[i].name))) return false;
        for (size_t j = 0; j < i; j++)
            if (strcmp(s->stations[i].id, s->stations[j].id) == 0) return false;
    }
    return true;
}
