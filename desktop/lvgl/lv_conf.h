#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#if defined(_WIN32)
#define LV_USE_OS LV_OS_WINDOWS
#define LV_USE_WINDOWS 1
#else
#define LV_USE_OS LV_OS_NONE
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#endif
#define LV_USE_XML 0
#define LV_USE_OBSERVER 1
#define LV_USE_OBJ_NAME 1
#define LV_MEM_SIZE (4U * 1024U * 1024U)
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#endif
