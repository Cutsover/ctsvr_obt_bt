#pragma once

#include <stddef.h>

#include "esp_err.h"

#define APP_STORAGE_MAC_LEN 18

esp_err_t app_storage_init(void);
esp_err_t app_storage_save_elm(const char *mac, const char *name);
esp_err_t app_storage_load_elm(char *mac, size_t mac_len, char *name, size_t name_len);
esp_err_t app_storage_clear_elm(void);
