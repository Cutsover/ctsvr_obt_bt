#include "bt_elm327.h"

#include <stdio.h>
#include <string.h>

#include "app_storage.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "BT_ELM327";
static const char *DEVICE_NAME = "CTSR_ESP32_OBD";
static const char *DEFAULT_PIN = "1234";

static bt_elm_device_t s_classic[BT_ELM_MAX_DEVICES];
static bt_elm_device_t s_ble[BT_ELM_MAX_DEVICES];
static size_t s_classic_count = 0;
static size_t s_ble_count = 0;

static SemaphoreHandle_t s_lock = NULL;
static SemaphoreHandle_t s_cmd_done = NULL;

static bt_elm_state_t s_state = BT_ELM_STATE_IDLE;
static uint32_t s_spp_handle = 0;
static char s_connected_mac[BT_ELM_MAC_LEN] = {0};
static esp_bd_addr_t s_target_bda = {0};

static char s_rx_buf[1024];
static size_t s_rx_len = 0;

static void mac_to_string(const esp_bd_addr_t bda, char *out, size_t out_len)
{
    snprintf(out, out_len,
             "%02X:%02X:%02X:%02X:%02X:%02X",
             bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}

static bool string_to_mac(const char *str, esp_bd_addr_t out)
{
    unsigned int b[6];

    if (!str) {
        return false;
    }

    if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6 &&
        sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)b[i];
    }

    return true;
}

static void add_device(bt_elm_device_t *list,
                       size_t *count,
                       const esp_bd_addr_t bda,
                       const char *name,
                       int rssi)
{
    if (!s_lock || !list || !count) {
        return;
    }

    char mac[BT_ELM_MAC_LEN];
    mac_to_string(bda, mac, sizeof(mac));

    const char *safe_name = (name && name[0]) ? name : "Unknown device";

    xSemaphoreTake(s_lock, portMAX_DELAY);

    for (size_t i = 0; i < *count; i++) {
        if (strcmp(list[i].mac, mac) == 0) {
            list[i].rssi = rssi;
            strlcpy(list[i].name, safe_name, sizeof(list[i].name));
            xSemaphoreGive(s_lock);
            return;
        }
    }

    if (*count < BT_ELM_MAX_DEVICES) {
        strlcpy(list[*count].mac, mac, sizeof(list[*count].mac));
        strlcpy(list[*count].name, safe_name, sizeof(list[*count].name));
        list[*count].rssi = rssi;
        (*count)++;
    }

    xSemaphoreGive(s_lock);
}

static void parse_eir_name(uint8_t *eir, char *name, size_t name_len)
{
    if (!eir || !name || name_len == 0) {
        return;
    }

    uint8_t len = 0;
    uint8_t *data = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);

    if (!data) {
        data = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
    }

    if (data && len > 0) {
        size_t copy_len = len < name_len - 1 ? len : name_len - 1;
        memcpy(name, data, copy_len);
        name[copy_len] = '\0';
    }
}

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    ESP_LOGI(TAG, "Classic GAP event: %d", event);

    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        char name[BT_ELM_NAME_LEN] = {0};
        int rssi = 0;

        for (int i = 0; i < param->disc_res.num_prop; i++) {
            esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];

            if (prop->type == ESP_BT_GAP_DEV_PROP_EIR && prop->val) {
                parse_eir_name((uint8_t *)prop->val, name, sizeof(name));
            } else if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME && prop->val && prop->len > 0) {
                size_t copy_len = prop->len < sizeof(name) - 1 ? prop->len : sizeof(name) - 1;
                memcpy(name, prop->val, copy_len);
                name[copy_len] = '\0';
            } else if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI && prop->val) {
                rssi = *(int8_t *)prop->val;
            }
        }

        if (!name[0]) {
            strlcpy(name, "Unknown Classic device", sizeof(name));
        }

        ESP_LOGI(TAG,
                 "Classic found: %02X:%02X:%02X:%02X:%02X:%02X name='%s' rssi=%d props=%d",
                 param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
                 param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5],
                 name, rssi, param->disc_res.num_prop);

        add_device(s_classic, &s_classic_count, param->disc_res.bda, name, rssi);
        break;
    }

    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            s_state = BT_ELM_STATE_CLASSIC_SCANNING;
            ESP_LOGI(TAG, "Classic discovery started");
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            if (s_state == BT_ELM_STATE_CLASSIC_SCANNING) {
                s_state = BT_ELM_STATE_IDLE;
            }
            ESP_LOGI(TAG, "Classic discovery stopped, found devices=%u", (unsigned)s_classic_count);
        }
        break;

    case ESP_BT_GAP_PIN_REQ_EVT: {
        esp_bt_pin_code_t pin;
        memset(pin, 0, sizeof(pin));

        size_t pin_len = strlen(DEFAULT_PIN);
        if (pin_len > ESP_BT_PIN_CODE_LEN) {
            pin_len = ESP_BT_PIN_CODE_LEN;
        }

        memcpy(pin, DEFAULT_PIN, pin_len);
        ESP_LOGI(TAG, "PIN requested, sending %s", DEFAULT_PIN);
        esp_bt_gap_pin_reply(param->pin_req.bda, true, pin_len, pin);
        break;
    }

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        ESP_LOGI(TAG, "BT auth %s, device=%s",
                 param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS ? "OK" : "FAILED",
                 param->auth_cmpl.device_name);
        break;

    default:
        break;
    }
}

static void ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGI(TAG, "BLE GAP event: %d", event);

    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(15);
        break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            s_state = BT_ELM_STATE_BLE_SCANNING;
            ESP_LOGI(TAG, "BLE scan started");
        } else {
            s_state = BT_ELM_STATE_ERROR;
            ESP_LOGE(TAG, "BLE scan start failed status=%d", param->scan_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
        if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            char name[BT_ELM_NAME_LEN] = {0};
            uint8_t adv_len = 0;
            uint8_t *adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                         ESP_BLE_AD_TYPE_NAME_CMPL,
                                                         &adv_len);

            if (!adv_name) {
                adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                    ESP_BLE_AD_TYPE_NAME_SHORT,
                                                    &adv_len);
            }

            if (adv_name && adv_len > 0) {
                size_t copy_len = adv_len < sizeof(name) - 1 ? adv_len : sizeof(name) - 1;
                memcpy(name, adv_name, copy_len);
                name[copy_len] = '\0';
            }

            if (!name[0]) {
                strlcpy(name, "Unknown BLE device", sizeof(name));
            }

            ESP_LOGI(TAG,
                     "BLE found: %02X:%02X:%02X:%02X:%02X:%02X name='%s' rssi=%d",
                     param->scan_rst.bda[0], param->scan_rst.bda[1], param->scan_rst.bda[2],
                     param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5],
                     name, param->scan_rst.rssi);

            add_device(s_ble, &s_ble_count, param->scan_rst.bda, name, param->scan_rst.rssi);
        } else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
            if (s_state == BT_ELM_STATE_BLE_SCANNING) {
                s_state = BT_ELM_STATE_IDLE;
            }
            ESP_LOGI(TAG, "BLE scan stopped, found devices=%u", (unsigned)s_ble_count);
        }
        break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (s_state == BT_ELM_STATE_BLE_SCANNING) {
            s_state = BT_ELM_STATE_IDLE;
        }
        ESP_LOGI(TAG, "BLE scan stop complete, found devices=%u", (unsigned)s_ble_count);
        break;

    default:
        break;
    }
}

static void spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(TAG, "SPP initialized");
        esp_bt_gap_set_device_name(DEVICE_NAME);
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        break;

    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(TAG, "SPP discovery complete: status=%d scn_num=%d",
                 param->disc_comp.status,
                 param->disc_comp.scn_num);

        if (param->disc_comp.status == ESP_SPP_SUCCESS && param->disc_comp.scn_num > 0) {
            esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE,
                            ESP_SPP_ROLE_MASTER,
                            param->disc_comp.scn[0],
                            s_target_bda);
        } else {
            s_state = BT_ELM_STATE_ERROR;
        }
        break;

    case ESP_SPP_OPEN_EVT:
        s_spp_handle = param->open.handle;
        s_state = BT_ELM_STATE_CONNECTED;
        mac_to_string(param->open.rem_bda, s_connected_mac, sizeof(s_connected_mac));
        app_storage_save_elm(s_connected_mac, "ELM327");
        ESP_LOGI(TAG, "SPP connected to %s handle=%lu",
                 s_connected_mac,
                 (unsigned long)s_spp_handle);
        break;

    case ESP_SPP_CLOSE_EVT:
        ESP_LOGW(TAG, "SPP closed");
        s_spp_handle = 0;
        s_connected_mac[0] = '\0';
        s_state = BT_ELM_STATE_IDLE;
        break;

    case ESP_SPP_DATA_IND_EVT:
        if (param->data_ind.len > 0) {
            size_t room = sizeof(s_rx_buf) - 1 - s_rx_len;
            size_t copy_len = param->data_ind.len < room ? param->data_ind.len : room;

            memcpy(&s_rx_buf[s_rx_len], param->data_ind.data, copy_len);
            s_rx_len += copy_len;
            s_rx_buf[s_rx_len] = '\0';

            if (strchr(s_rx_buf, '>') && s_cmd_done) {
                xSemaphoreGive(s_cmd_done);
            }
        }
        break;

    default:
        break;
    }
}

esp_err_t bt_elm327_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_cmd_done = xSemaphoreCreateBinary();

    if (!s_lock || !s_cmd_done) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_enable(BTDM) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_bt_gap_register_callback(gap_cb);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_ble_gap_register_callback(ble_gap_cb);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_spp_register_callback(spp_cb);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_spp_cfg_t spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };

    ret = esp_spp_enhanced_init(&spp_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_spp_enhanced_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Bluetooth BTDM stack started: Classic SPP + BLE scan");
    return ESP_OK;
}

esp_err_t bt_elm327_start_classic_scan(void)
{
    ESP_LOGI(TAG, "Classic scan requested, current state=%s", bt_elm327_state_string());

    if (!s_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state == BT_ELM_STATE_CLASSIC_SCANNING || s_state == BT_ELM_STATE_BLE_SCANNING) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state == BT_ELM_STATE_CONNECTING || s_state == BT_ELM_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_classic_count = 0;
    memset(s_classic, 0, sizeof(s_classic));
    xSemaphoreGive(s_lock);

    s_state = BT_ELM_STATE_CLASSIC_SCANNING;

    esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 20, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_gap_start_discovery GENERAL failed: %s", esp_err_to_name(err));
        s_state = BT_ELM_STATE_ERROR;
        return err;
    }

    ESP_LOGI(TAG, "Classic GENERAL discovery requested");
    return ESP_OK;
}

esp_err_t bt_elm327_start_ble_scan(void)
{
    ESP_LOGI(TAG, "BLE scan requested, current state=%s", bt_elm327_state_string());

    if (!s_lock) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state == BT_ELM_STATE_CLASSIC_SCANNING || s_state == BT_ELM_STATE_BLE_SCANNING) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state == BT_ELM_STATE_CONNECTING || s_state == BT_ELM_STATE_CONNECTED) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_ble_count = 0;
    memset(s_ble, 0, sizeof(s_ble));
    xSemaphoreGive(s_lock);

    s_state = BT_ELM_STATE_BLE_SCANNING;

    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
    };

    esp_err_t err = esp_ble_gap_set_scan_params(&scan_params);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_set_scan_params failed: %s", esp_err_to_name(err));
        s_state = BT_ELM_STATE_ERROR;
        return err;
    }

    ESP_LOGI(TAG, "BLE scan params requested");
    return ESP_OK;
}

size_t bt_elm327_get_classic_devices(bt_elm_device_t *out, size_t max_count)
{
    if (!out || max_count == 0 || !s_lock) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = s_classic_count < max_count ? s_classic_count : max_count;
    memcpy(out, s_classic, n * sizeof(bt_elm_device_t));
    xSemaphoreGive(s_lock);

    return n;
}

size_t bt_elm327_get_ble_devices(bt_elm_device_t *out, size_t max_count)
{
    if (!out || max_count == 0 || !s_lock) {
        return 0;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = s_ble_count < max_count ? s_ble_count : max_count;
    memcpy(out, s_ble, n * sizeof(bt_elm_device_t));
    xSemaphoreGive(s_lock);

    return n;
}

esp_err_t bt_elm327_connect_mac_string(const char *mac)
{
    esp_bd_addr_t bda;

    if (!string_to_mac(mac, bda)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_state == BT_ELM_STATE_CLASSIC_SCANNING) {
        esp_bt_gap_cancel_discovery();
    }

    if (s_state == BT_ELM_STATE_BLE_SCANNING) {
        esp_ble_gap_stop_scanning();
    }

    memcpy(s_target_bda, bda, ESP_BD_ADDR_LEN);
    s_state = BT_ELM_STATE_CONNECTING;

    ESP_LOGI(TAG, "Starting Classic SPP discovery for %s", mac);
    return esp_spp_start_discovery(bda);
}

esp_err_t bt_elm327_disconnect(void)
{
    if (s_spp_handle) {
        return esp_spp_disconnect(s_spp_handle);
    }

    return ESP_OK;
}

bt_elm_state_t bt_elm327_get_state(void)
{
    return s_state;
}

const char *bt_elm327_state_string(void)
{
    switch (s_state) {
    case BT_ELM_STATE_IDLE:
        return "idle";
    case BT_ELM_STATE_CLASSIC_SCANNING:
        return "classic_scanning";
    case BT_ELM_STATE_BLE_SCANNING:
        return "ble_scanning";
    case BT_ELM_STATE_CONNECTING:
        return "connecting";
    case BT_ELM_STATE_CONNECTED:
        return "connected";
    case BT_ELM_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

bool bt_elm327_is_connected(void)
{
    return s_state == BT_ELM_STATE_CONNECTED && s_spp_handle != 0;
}

const char *bt_elm327_connected_mac(void)
{
    return s_connected_mac;
}

esp_err_t bt_elm327_send_command(const char *cmd,
                                 char *response,
                                 size_t response_len,
                                 uint32_t timeout_ms)
{
    if (!cmd || !response || response_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!bt_elm327_is_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    while (xSemaphoreTake(s_cmd_done, 0) == pdTRUE) {
    }

    s_rx_len = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));

    char line[96];
    snprintf(line, sizeof(line), "%s\r", cmd);

    esp_err_t err = esp_spp_write(s_spp_handle, strlen(line), (uint8_t *)line);
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_cmd_done, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        strlcpy(response, s_rx_buf, response_len);
        return ESP_ERR_TIMEOUT;
    }

    strlcpy(response, s_rx_buf, response_len);
    return ESP_OK;
}
