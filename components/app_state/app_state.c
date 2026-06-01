#include "app_state.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "APP_STATE";

static SemaphoreHandle_t s_lock;
static app_state_snapshot_t s_snapshot = {
    .state = APP_STATE_BOOT,
    .last_error = ESP_OK,
};

static void led_set(bool on)
{
    gpio_set_level(APP_STATE_LED_GPIO, on ? 1 : 0);
}

static void led_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void led_task(void *arg)
{
    (void)arg;

    while (true) {
        app_state_t state = app_state_get();

        switch (state) {
        case APP_STATE_ERROR:
        case APP_STATE_BT_FAILED:
            led_set(true);
            led_delay_ms(120);
            led_set(false);
            led_delay_ms(120);
            break;

        case APP_STATE_OBD_READY:
            led_set(true);
            led_delay_ms(500);
            break;

        case APP_STATE_READY:
        case APP_STATE_WEB_READY:
            led_set(true);
            led_delay_ms(80);
            led_set(false);
            led_delay_ms(1920);
            break;

        case APP_STATE_BOOT:
        case APP_STATE_STORAGE_INIT:
        case APP_STATE_WIFI_INIT:
        case APP_STATE_BT_INIT:
        case APP_STATE_WEB_INIT:
        case APP_STATE_OBD_INIT:
            led_set(true);
            led_delay_ms(250);
            led_set(false);
            led_delay_ms(250);
            break;

        default:
            led_set(true);
            led_delay_ms(80);
            led_set(false);
            led_delay_ms(920);
            break;
        }
    }
}

esp_err_t app_state_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << APP_STATE_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    led_set(false);

    BaseType_t task_ok = xTaskCreate(led_task, "state_led", 2048, NULL, 2, NULL);
    if (task_ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "State machine initialized, LED GPIO=%d", APP_STATE_LED_GPIO);
    return ESP_OK;
}

void app_state_set(app_state_t state)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }

    s_snapshot.state = state;
    if (state != APP_STATE_ERROR && state != APP_STATE_BT_FAILED) {
        s_snapshot.last_error = ESP_OK;
    }

    if (s_lock) {
        xSemaphoreGive(s_lock);
    }

    ESP_LOGI(TAG, "State: %s", app_state_string(state));
}

void app_state_fail(app_state_t state, esp_err_t err)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }

    s_snapshot.state = state;
    s_snapshot.last_error = err;

    if (s_lock) {
        xSemaphoreGive(s_lock);
    }

    ESP_LOGE(TAG, "State failed: %s, error=%s", app_state_string(state), esp_err_to_name(err));
}

static void set_flag(bool *flag, bool ready)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }

    *flag = ready;

    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

void app_state_mark_storage_ready(bool ready)
{
    set_flag(&s_snapshot.storage_ready, ready);
}

void app_state_mark_wifi_ready(bool ready)
{
    set_flag(&s_snapshot.wifi_ready, ready);
}

void app_state_mark_bt_ready(bool ready)
{
    set_flag(&s_snapshot.bt_ready, ready);
}

void app_state_mark_web_ready(bool ready)
{
    set_flag(&s_snapshot.web_ready, ready);
}

void app_state_mark_obd_ready(bool ready)
{
    set_flag(&s_snapshot.obd_ready, ready);
}

app_state_snapshot_t app_state_get_snapshot(void)
{
    app_state_snapshot_t snapshot;

    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }

    snapshot = s_snapshot;

    if (s_lock) {
        xSemaphoreGive(s_lock);
    }

    return snapshot;
}

app_state_t app_state_get(void)
{
    return app_state_get_snapshot().state;
}

const char *app_state_string(app_state_t state)
{
    switch (state) {
    case APP_STATE_BOOT:
        return "boot";
    case APP_STATE_STORAGE_INIT:
        return "storage_init";
    case APP_STATE_STORAGE_READY:
        return "storage_ready";
    case APP_STATE_WIFI_INIT:
        return "wifi_init";
    case APP_STATE_WIFI_READY:
        return "wifi_ready";
    case APP_STATE_BT_INIT:
        return "bt_init";
    case APP_STATE_BT_READY:
        return "bt_ready";
    case APP_STATE_BT_FAILED:
        return "bt_failed";
    case APP_STATE_WEB_INIT:
        return "web_init";
    case APP_STATE_WEB_READY:
        return "web_ready";
    case APP_STATE_READY:
        return "ready";
    case APP_STATE_OBD_INIT:
        return "obd_init";
    case APP_STATE_OBD_READY:
        return "obd_ready";
    case APP_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}
