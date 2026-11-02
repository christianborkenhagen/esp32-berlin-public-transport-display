#include "app_settings.h"
static app_settings_t s_settings;
bool app_settings_init(void) { app_settings_defaults(&s_settings); return true; }
bool app_settings_available(void) { return true; }
void app_settings_get(app_settings_t *out) { if (out) *out = s_settings; }
bool app_settings_save(const app_settings_t *s)
{
    if (!app_settings_valid(s)) return false;
    s_settings = *s;
    return true;
}
