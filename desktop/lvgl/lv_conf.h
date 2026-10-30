#ifndef LV_CONF_H
#define LV_CONF_H

/* The Win32 backend has its own framebuffer and needs the Windows OS adapter. */
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_WINDOWS
#define LV_USE_WINDOWS 1
#define LV_MEM_SIZE (2U * 1024U * 1024U)
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1

#endif
