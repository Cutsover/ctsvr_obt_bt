#pragma once

#include <stdbool.h>

#include "esp_err.h"

#define APP_STATE_LED_GPIO 2

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_STORAGE_INIT,
    APP_STATE_STORAGE_READY,
    APP_STATE_WIFI_INIT,
    APP_STATE_WIFI_READY,
    APP_STATE_BT_INIT,
    APP_STATE_BT_READY,
    APP_STATE_BT_FAILED,
    APP_STATE_WEB_INIT,
    APP_STATE_WEB_READY,
    APP_STATE_READY,
    APP_STATE_OBD_INIT,
    APP_STATE_OBD_READY,
    APP_STATE_ERROR,
} app_state_t;

typedef struct {
    app_state_t state;
    esp_err_t last_error;
    bool storage_ready;
    bool wifi_ready;
    bool bt_ready;
    bool web_ready;
    bool obd_ready;
} app_state_snapshot_t;

esp_err_t app_state_init(void);
void app_state_set(app_state_t state);
void app_state_fail(app_state_t state, esp_err_t err);
void app_state_mark_storage_ready(bool ready);
void app_state_mark_wifi_ready(bool ready);
void app_state_mark_bt_ready(bool ready);
void app_state_mark_web_ready(bool ready);
void app_state_mark_obd_ready(bool ready);
app_state_snapshot_t app_state_get_snapshot(void);
app_state_t app_state_get(void);
const char *app_state_string(app_state_t state);
