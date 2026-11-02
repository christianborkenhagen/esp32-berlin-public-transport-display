#include "departures_parser.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"

/* Convert the API's ISO 8601 offset to Unix time without relying on TZ/DST. */
static bool parse_when(const char *text, int64_t *result)
{
    int year, month, day, hour, minute, second, offset_hour = 0, offset_min = 0;
    int used = 0;
    if (!text || sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d%n",
                        &year, &month, &day, &hour, &minute, &second, &used) != 6) {
        return false;
    }
    if (year < 1970 || year > 9999 || month < 1 || month > 12 ||
        hour > 23 || minute > 59 || second > 59 ||
        hour < 0 || minute < 0 || second < 0) return false;
    const int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    int max_day = days_per_month[month - 1] + (month == 2 && leap);
    if (day < 1 || day > max_day) return false;

    const char *p = text + used;
    if (*p == '.') {
        ++p;
        if (*p < '0' || *p > '9') return false;
        while (*p >= '0' && *p <= '9') ++p;
    }
    int sign = 0;
    if (*p == 'Z' && p[1] == '\0') {
        sign = 0;
    } else if ((*p == '+' || *p == '-') && strlen(p) == 6 &&
               p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9' &&
               p[3] == ':' && p[4] >= '0' && p[4] <= '9' &&
               p[5] >= '0' && p[5] <= '9') {
        sign = *p == '+' ? 1 : -1;
        offset_hour = (p[1] - '0') * 10 + p[2] - '0';
        offset_min = (p[4] - '0') * 10 + p[5] - '0';
        if (offset_hour > 23 || offset_min > 59) return false;
    } else {
        return false;
    }
    /* Days since 1970-01-01 in the proleptic Gregorian calendar. */
    int y = year - (month <= 2);
    int era = y / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned shifted_month = (unsigned)(month + (month > 2 ? -3 : 9));
    unsigned doy = (153 * shifted_month + 2) / 5 + (unsigned)day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = (int64_t)era * 146097 + doe - 719468;
    *result = days * 86400 + hour * 3600 + minute * 60 + second -
              sign * (offset_hour * 3600 + offset_min * 60);
    return true;
}

/*
 * The built-in Montserrat fonts only contain ASCII and a few symbols.
 * Transliterate common German characters and replace unsupported UTF-8.
 */
static void copy_display_text(char *dst, size_t size, const char *src)
{
    size_t n = 0;
    while (*src && n + 1 < size) {
        unsigned char c = (unsigned char)*src++;
        const char *replacement = NULL;
        if (c == 0xc3 && *src) {
            unsigned char next = (unsigned char)*src;
            switch (next) {
            case 0x84: replacement = "Ae"; break;
            case 0x96: replacement = "Oe"; break;
            case 0x9c: replacement = "Ue"; break;
            case 0xa4: replacement = "ae"; break;
            case 0xb6: replacement = "oe"; break;
            case 0xbc: replacement = "ue"; break;
            case 0x9f: replacement = "ss"; break;
            default: break;
            }
        }
        if (c < 0x80) {
            dst[n++] = (char)c;
        } else {
            while (((unsigned char)*src & 0xc0) == 0x80) ++src;
            if (!replacement) replacement = "?";
            while (*replacement && n + 1 < size) dst[n++] = *replacement++;
        }
    }
    dst[n] = '\0';
}

bool departures_parse(const char *json, departures_station_t *out)
{
    if (!json || !out) return false;
    cJSON *root = cJSON_ParseWithOpts(json, NULL, true);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, "departures");
    if (!cJSON_IsArray(array)) {
        cJSON_Delete(root);
        return false;
    }
    departures_station_t parsed = {0};
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (parsed.count == DEPARTURES_PER_STATION_MAX) break;
        if (!cJSON_IsObject(item)) continue;
        cJSON *line = cJSON_GetObjectItemCaseSensitive(item, "line");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(line, "name");
        cJSON *direction = cJSON_GetObjectItemCaseSensitive(item, "direction");
        if (!cJSON_IsString(direction) || !direction->valuestring[0]) {
            cJSON *destination = cJSON_GetObjectItemCaseSensitive(item, "destination");
            direction = cJSON_GetObjectItemCaseSensitive(destination, "name");
        }
        cJSON *when = cJSON_GetObjectItemCaseSensitive(item, "when");
        bool planned_only = !cJSON_IsString(when);
        if (planned_only) when = cJSON_GetObjectItemCaseSensitive(item, "plannedWhen");
        int64_t timestamp;
        if (!cJSON_IsString(name) || !name->valuestring[0] ||
            !cJSON_IsString(direction) || !direction->valuestring[0] ||
            !cJSON_IsString(when) || !parse_when(when->valuestring, &timestamp)) continue;
        departure_t *departure = &parsed.items[parsed.count++];
        copy_display_text(departure->line, sizeof(departure->line), name->valuestring);
        copy_display_text(departure->direction, sizeof(departure->direction),
                          direction->valuestring);
        departure->when = timestamp;
        departure->planned_only = planned_only;
        departure->cancelled = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "cancelled"));
    }
    /* Do not report a malformed nonempty payload as "no departures". */
    if (cJSON_GetArraySize(array) > 0 && parsed.count == 0) {
        cJSON_Delete(root);
        return false;
    }
    *out = parsed;
    cJSON_Delete(root);
    return true;
}
