#ifndef DEPARTURES_PARSER_H
#define DEPARTURES_PARSER_H

#include <stdbool.h>
#include "departures.h"

/* Invalid individual departures are skipped; an empty array is valid. */
bool departures_parse(const char *json, departures_station_t *out);

#endif
