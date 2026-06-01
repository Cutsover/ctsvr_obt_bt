#include "obd_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bt_elm327.h"

static esp_err_t send_cmd(const char *cmd, char *buf, size_t buf_len)
{
    return bt_elm327_send_command(cmd, buf, buf_len, 3000);
}

esp_err_t obd_init_elm(void)
{
    char response[256];
    const char *cmds[] = {
        "ATZ",
        "ATE0",
        "ATL0",
        "ATS0",
        "ATH0",
        "ATAT1",
        "ATSP0",
        "0100",
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        esp_err_t err = send_cmd(cmds[i], response, sizeof(response));
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static int hexbyte(const char *s)
{
    char tmp[3] = {s[0], s[1], 0};
    return (int)strtol(tmp, NULL, 16);
}

static char *find41(char *response, const char *pid)
{
    for (char *p = response; *p; p++) {
        if (p[0] == '4' && p[1] == '1' && p[2] == pid[0] && p[3] == pid[1]) {
            return p + 4;
        }
    }

    return NULL;
}

esp_err_t obd_read_live(obd_live_data_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    char response[256];
    char *payload;
    esp_err_t err;

    err = send_cmd("010C", response, sizeof(response));
    payload = find41(response, "0C");
    if (err == ESP_OK && payload && strlen(payload) >= 4) {
        int a = hexbyte(payload);
        int b = hexbyte(payload + 2);
        out->rpm = ((a * 256) + b) / 4;
    }

    err = send_cmd("010D", response, sizeof(response));
    payload = find41(response, "0D");
    if (err == ESP_OK && payload && strlen(payload) >= 2) {
        out->speed_kmh = hexbyte(payload);
    }

    err = send_cmd("0105", response, sizeof(response));
    payload = find41(response, "05");
    if (err == ESP_OK && payload && strlen(payload) >= 2) {
        out->coolant_c = hexbyte(payload) - 40;
    }

    err = send_cmd("0111", response, sizeof(response));
    payload = find41(response, "11");
    if (err == ESP_OK && payload && strlen(payload) >= 2) {
        out->throttle_pct = (hexbyte(payload) * 100) / 255;
    }

    err = send_cmd("0142", response, sizeof(response));
    payload = find41(response, "42");
    if (err == ESP_OK && payload && strlen(payload) >= 4) {
        int a = hexbyte(payload);
        int b = hexbyte(payload + 2);
        out->ecu_voltage = ((a * 256) + b) / 1000.0f;
    }

    return ESP_OK;
}

esp_err_t obd_read_dtc(char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return send_cmd("03", out, out_len);
}

esp_err_t obd_clear_dtc(void)
{
    char response[128];
    return send_cmd("04", response, sizeof(response));
}
