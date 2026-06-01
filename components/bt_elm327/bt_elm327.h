#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#define BT_ELM_MAX_DEVICES 24
#define BT_ELM_NAME_LEN 64
#define BT_ELM_MAC_LEN 18
typedef enum { BT_ELM_STATE_IDLE=0, BT_ELM_STATE_CLASSIC_SCANNING, BT_ELM_STATE_BLE_SCANNING, BT_ELM_STATE_CONNECTING, BT_ELM_STATE_CONNECTED, BT_ELM_STATE_ERROR } bt_elm_state_t;
typedef struct { char mac[BT_ELM_MAC_LEN]; char name[BT_ELM_NAME_LEN]; int rssi; } bt_elm_device_t;
esp_err_t bt_elm327_init(void);
esp_err_t bt_elm327_start_classic_scan(void);
esp_err_t bt_elm327_start_ble_scan(void);
size_t bt_elm327_get_classic_devices(bt_elm_device_t *out, size_t max_count);
size_t bt_elm327_get_ble_devices(bt_elm_device_t *out, size_t max_count);
esp_err_t bt_elm327_connect_mac_string(const char *mac_string);
esp_err_t bt_elm327_disconnect(void);
bt_elm_state_t bt_elm327_get_state(void);
const char *bt_elm327_state_string(void);
bool bt_elm327_is_connected(void);
const char *bt_elm327_connected_mac(void);
esp_err_t bt_elm327_send_command(const char *cmd, char *response, size_t response_len, uint32_t timeout_ms);
