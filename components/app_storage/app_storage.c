#include "app_storage.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "APP_STORAGE";
static const char *NS = "ctsr";

esp_err_t app_storage_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Storage initialized");
    }

    return ret;
}

esp_err_t app_storage_save_elm(const char *mac, const char *name)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS, NVS_READWRITE, &h);

    if (ret != ESP_OK) {
        return ret;
    }

    if (mac) {
        ret = nvs_set_str(h, "elm_mac", mac);
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
    }

    if (name) {
        ret = nvs_set_str(h, "elm_name", name);
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
    }

    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t app_storage_load_elm(char *mac, size_t mac_len, char *name, size_t name_len)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS, NVS_READONLY, &h);

    if (ret != ESP_OK) {
        return ret;
    }

    if (mac && mac_len > 0) {
        ret = nvs_get_str(h, "elm_mac", mac, &mac_len);
        if (ret != ESP_OK) {
            nvs_close(h);
            return ret;
        }
    }

    if (name && name_len > 0) {
        esp_err_t name_ret = nvs_get_str(h, "elm_name", name, &name_len);
        if (name_ret != ESP_OK) {
            name[0] = '\0';
        }
    }

    nvs_close(h);
    return ESP_OK;
}

esp_err_t app_storage_clear_elm(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS, NVS_READWRITE, &h);

    if (ret != ESP_OK) {
        return ret;
    }

    (void)nvs_erase_key(h, "elm_mac");
    (void)nvs_erase_key(h, "elm_name");

    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}
