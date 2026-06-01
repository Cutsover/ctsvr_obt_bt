#include "app_wifi.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"

#define WIFI_SSID "CTSR_Prod"
#define WIFI_PASS "elephant"

static const char *TAG = "APP_WIFI";
static EventGroupHandle_t s_wifi_events;

#define WIFI_AP_STARTED_BIT BIT0
#define RETURN_ON_ERROR(expr) do { \
        esp_err_t err__ = (expr); \
        if (err__ != ESP_OK) { \
            ESP_LOGE(TAG, "%s failed: %s", #expr, esp_err_to_name(err__)); \
            return err__; \
        } \
    } while (0)

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START && s_wifi_events) {
        xEventGroupSetBits(s_wifi_events, WIFI_AP_STARTED_BIT);
    }
}

esp_err_t app_wifi_start_ap(void)
{
    RETURN_ON_ERROR(esp_netif_init());
    RETURN_ON_ERROR(esp_event_loop_create_default());

    s_wifi_events = xEventGroupCreate();
    if (!s_wifi_events) {
        return ESP_ERR_NO_MEM;
    }

    esp_netif_t *ap = esp_netif_create_default_wifi_ap();

    esp_netif_ip_info_t ip;
    IP4_ADDR(&ip.ip, 10, 10, 10, 1);
    IP4_ADDR(&ip.gw, 10, 10, 10, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

    RETURN_ON_ERROR(esp_netif_dhcps_stop(ap));
    RETURN_ON_ERROR(esp_netif_set_ip_info(ap, &ip));
    RETURN_ON_ERROR(esp_netif_dhcps_start(ap));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    RETURN_ON_ERROR(esp_wifi_init(&cfg));
    RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT,
                                               WIFI_EVENT_AP_START,
                                               wifi_event_handler,
                                               NULL));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.ap.ssid, WIFI_SSID, sizeof(wc.ap.ssid));
    strlcpy((char *)wc.ap.password, WIFI_PASS, sizeof(wc.ap.password));

    wc.ap.ssid_len = strlen(WIFI_SSID);
    wc.ap.channel = 6;
    wc.ap.max_connection = 4;
    wc.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP));
    RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wc));
    RETURN_ON_ERROR(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
                                           WIFI_AP_STARTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(5000));
    if ((bits & WIFI_AP_STARTED_BIT) == 0) {
        ESP_LOGE(TAG, "Wi-Fi AP start confirmation timed out");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG,
             "Wi-Fi AP started: SSID=%s, password=%s, URL=http://10.10.10.1",
             WIFI_SSID,
             WIFI_PASS);

    return ESP_OK;
}
