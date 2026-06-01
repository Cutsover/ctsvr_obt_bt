#include "esp_err.h"
#include "esp_log.h"

#include "app_storage.h"
#include "app_web.h"
#include "app_wifi.h"
#include "bt_elm327.h"

static const char *TAG = "CTSR_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting CTSR OBD Reader Dual Scan");

    ESP_ERROR_CHECK(app_storage_init());
    ESP_ERROR_CHECK(app_wifi_start_ap());

    esp_err_t bt_ret = bt_elm327_init();
    if (bt_ret != ESP_OK) {
        ESP_LOGE(TAG,
                 "Bluetooth init failed: %s. Web UI will continue.",
                 esp_err_to_name(bt_ret));
    }

    ESP_ERROR_CHECK(app_web_start());

    char mac[18] = "";
    char name[64] = "";

    if (app_storage_load_elm(mac, sizeof(mac), name, sizeof(name)) == ESP_OK && mac[0]) {
        ESP_LOGI(TAG, "Saved ELM adapter: %s %s", mac, name);
    } else {
        ESP_LOGI(TAG, "No saved ELM327 adapter. Open http://10.10.10.1");
    }
}
