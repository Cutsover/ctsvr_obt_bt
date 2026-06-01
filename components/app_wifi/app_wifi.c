#include "app_wifi.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

#define WIFI_SSID "CTSR_Prod"
#define WIFI_PASS "elephant"

static const char *TAG = "APP_WIFI";

esp_err_t app_wifi_start_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap = esp_netif_create_default_wifi_ap();

    esp_netif_ip_info_t ip;
    IP4_ADDR(&ip.ip, 10, 10, 10, 1);
    IP4_ADDR(&ip.gw, 10, 10, 10, 1);
    IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap, &ip));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wc = {0};
    strlcpy((char *)wc.ap.ssid, WIFI_SSID, sizeof(wc.ap.ssid));
    strlcpy((char *)wc.ap.password, WIFI_PASS, sizeof(wc.ap.password));

    wc.ap.ssid_len = strlen(WIFI_SSID);
    wc.ap.channel = 6;
    wc.ap.max_connection = 4;
    wc.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,
             "Wi-Fi AP started: SSID=%s, password=%s, URL=http://10.10.10.1",
             WIFI_SSID,
             WIFI_PASS);

    return ESP_OK;
}
