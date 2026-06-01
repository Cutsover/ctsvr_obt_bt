#include "esp_err.h"
#include "esp_log.h"

#include "app_storage.h"
#include "app_state.h"
#include "app_web.h"
#include "app_wifi.h"
#include "bt_elm327.h"

static const char *TAG = "CTSR_MAIN";

void app_main(void)
{
    ESP_ERROR_CHECK(app_state_init());
    app_state_set(APP_STATE_BOOT);

    ESP_LOGI(TAG, "Starting CTSR OBD Reader Dual Scan");

    app_state_set(APP_STATE_STORAGE_INIT);
    esp_err_t err = app_storage_init();
    if (err != ESP_OK) {
        app_state_fail(APP_STATE_ERROR, err);
        return;
    }
    app_state_mark_storage_ready(true);
    app_state_set(APP_STATE_STORAGE_READY);

    app_state_set(APP_STATE_WIFI_INIT);
    err = app_wifi_start_ap();
    if (err != ESP_OK) {
        app_state_fail(APP_STATE_ERROR, err);
        return;
    }
    app_state_mark_wifi_ready(true);
    app_state_set(APP_STATE_WIFI_READY);

    app_state_set(APP_STATE_BT_INIT);
    esp_err_t bt_ret = bt_elm327_init();
    if (bt_ret != ESP_OK) {
        app_state_mark_bt_ready(false);
        app_state_fail(APP_STATE_BT_FAILED, bt_ret);
        ESP_LOGE(TAG,
                 "Bluetooth init failed: %s. Web UI will continue.",
                 esp_err_to_name(bt_ret));
    } else {
        app_state_mark_bt_ready(true);
        app_state_set(APP_STATE_BT_READY);
    }

    app_state_set(APP_STATE_WEB_INIT);
    err = app_web_start();
    if (err != ESP_OK) {
        app_state_fail(APP_STATE_ERROR, err);
        return;
    }
    app_state_mark_web_ready(true);
    app_state_set(APP_STATE_WEB_READY);
    if (bt_ret == ESP_OK) {
        app_state_set(APP_STATE_READY);
    } else {
        app_state_fail(APP_STATE_BT_FAILED, bt_ret);
    }

    char mac[18] = "";
    char name[64] = "";

    if (app_storage_load_elm(mac, sizeof(mac), name, sizeof(name)) == ESP_OK && mac[0]) {
        ESP_LOGI(TAG, "Saved ELM adapter: %s %s", mac, name);
    } else {
        ESP_LOGI(TAG, "No saved ELM327 adapter. Open http://10.10.10.1");
    }
}
