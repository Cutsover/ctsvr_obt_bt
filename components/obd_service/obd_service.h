#pragma once
#include "esp_err.h"
typedef struct { int rpm; int speed_kmh; int coolant_c; int throttle_pct; float ecu_voltage; } obd_live_data_t;
esp_err_t obd_init_elm(void);
esp_err_t obd_read_live(obd_live_data_t *out);
esp_err_t obd_read_dtc(char *out, size_t out_len);
esp_err_t obd_clear_dtc(void);
