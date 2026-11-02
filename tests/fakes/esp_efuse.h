#pragma once
#include <stdbool.h>
#define EFUSE_BLK_KEY0 0
bool esp_efuse_get_key_dis_read(int block);
bool esp_efuse_get_key_dis_write(int block);
