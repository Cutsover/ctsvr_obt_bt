#include "app_web.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bt_elm327.h"
#include "obd_service.h"
#include "app_storage.h"
static const char *TAG="APP_WEB";
static const char index_html[] =
"<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>CTSR OBD Reader</title><style>body{font-family:Arial;margin:0;background:#0f172a;color:#e5e7eb}.wrap{max-width:1100px;margin:auto;padding:18px}.card{background:#111827;border:1px solid #334155;border-radius:14px;padding:16px;margin:12px 0}button,input{border-radius:10px;border:1px solid #475569;background:#1f2937;color:#fff;padding:10px;margin:4px}button{cursor:pointer;background:#2563eb}table{width:100%;border-collapse:collapse}td,th{border-bottom:1px solid #334155;padding:8px;text-align:left}code{color:#93c5fd}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}.metric{font-size:26px;font-weight:bold}</style></head><body><div class='wrap'>"
"<h1>CTSR OBD Reader</h1><div class='card'><b>Статус:</b> <span id='status'>...</span><br><b>Лог:</b><pre id='log'></pre></div>"
"<div class='card'><h2>Classic Bluetooth / SPP</h2><button onclick='scanClassic()'>Сканировать Classic</button><button onclick='loadClassic()'>Обновить Classic</button><table><thead><tr><th>Имя</th><th>MAC</th><th>RSSI</th><th></th></tr></thead><tbody id='classic'><tr><td colspan=4>Нажмите Classic Scan</td></tr></tbody></table></div>"
"<div class='card'><h2>BLE Scan</h2><button onclick='scanBle()'>Сканировать BLE</button><button onclick='loadBle()'>Обновить BLE</button><table><thead><tr><th>Имя</th><th>MAC</th><th>RSSI</th><th>Тип</th></tr></thead><tbody id='ble'><tr><td colspan=4>Нажмите BLE Scan</td></tr></tbody></table><p>Подключение к BLE ELM пока диагностическое: сначала надо увидеть UUID адаптера. Подключение OBD-команд сейчас реализовано для Classic SPP.</p></div>"
"<div class='card'><h2>Ручное Classic SPP подключение</h2><input id='manualMac' placeholder='AA:BB:CC:DD:EE:FF'><button onclick='connectManual()'>Подключить по MAC</button><button onclick='initElm()'>Инициализировать ELM</button></div>"
"<div class='card'><h2>Live данные</h2><div class='grid'><div>RPM<div class='metric' id='rpm'>—</div></div><div>Скорость<div class='metric' id='speed'>—</div></div><div>ОЖ °C<div class='metric' id='coolant'>—</div></div><div>Дроссель %<div class='metric' id='thr'>—</div></div><div>ECU V<div class='metric' id='volt'>—</div></div></div><button onclick='pollLive()'>Прочитать сейчас</button><button onclick='autoPoll()'>Автоопрос</button></div>"
"<div class='card'><h2>DTC</h2><button onclick='readDtc()'>Читать ошибки</button><button onclick='clearDtc()'>Стереть ошибки</button><pre id='dtc'></pre></div>"
"<script>async function api(u,o){let r=await fetch(u,o||{});return await r.json()}function log(s){let l=document.getElementById('log');l.textContent=new Date().toLocaleTimeString()+' '+s+'\\n'+l.textContent}async function stat(){let s=await api('/api/status');status.textContent=JSON.stringify(s)}setInterval(stat,2000);stat();"
"async function scanClassic(){log('Classic scan...');classic.innerHTML='<tr><td colspan=4>Идёт Classic сканирование...</td></tr>';let r=await api('/api/bt/classic/scan',{method:'POST'});log('Classic: '+JSON.stringify(r));setTimeout(loadClassic,4000);setTimeout(loadClassic,10000);setTimeout(loadClassic,21000)}"
"async function scanBle(){log('BLE scan...');ble.innerHTML='<tr><td colspan=4>Идёт BLE сканирование...</td></tr>';let r=await api('/api/bt/ble/scan',{method:'POST'});log('BLE: '+JSON.stringify(r));setTimeout(loadBle,3000);setTimeout(loadBle,8000);setTimeout(loadBle,16000)}"
"async function loadClassic(){let d=await api('/api/bt/classic/devices');classic.innerHTML='';if(!d.devices.length){classic.innerHTML='<tr><td colspan=4>Classic устройства пока не найдены</td></tr>';return}d.devices.forEach(x=>{let tr=document.createElement('tr');let btn='<button onclick=\"connectMac(\\\''+x.mac+'\\\')\">Подключить SPP</button>';tr.innerHTML='<td>'+x.name+'</td><td><code>'+x.mac+'</code></td><td>'+x.rssi+'</td><td>'+btn+'</td>';classic.appendChild(tr)})}"
"async function loadBle(){let d=await api('/api/bt/ble/devices');ble.innerHTML='';if(!d.devices.length){ble.innerHTML='<tr><td colspan=4>BLE устройства пока не найдены</td></tr>';return}d.devices.forEach(x=>{let tr=document.createElement('tr');tr.innerHTML='<td>'+x.name+'</td><td><code>'+x.mac+'</code></td><td>'+x.rssi+'</td><td>BLE</td>';ble.appendChild(tr)})}"
"async function connectMac(m){manualMac.value=m;let r=await api('/api/bt/connect?mac='+encodeURIComponent(m),{method:'POST'});log('connect: '+JSON.stringify(r))}async function connectManual(){connectMac(manualMac.value)}async function initElm(){let r=await api('/api/obd/init',{method:'POST'});log('init: '+JSON.stringify(r))}"
"async function pollLive(){let d=await api('/api/obd/live');if(d.error){log('Live error: '+d.error);return}rpm.textContent=(d.rpm==null?'—':d.rpm);speed.textContent=(d.speed_kmh==null?'—':d.speed_kmh);coolant.textContent=(d.coolant_c==null?'—':d.coolant_c);thr.textContent=(d.throttle_pct==null?'—':d.throttle_pct);volt.textContent=(d.ecu_voltage?Number(d.ecu_voltage).toFixed(2):'—')}let ap=null;function autoPoll(){if(ap){clearInterval(ap);ap=null;log('auto off')}else{ap=setInterval(pollLive,2000);log('auto on')}}"
"async function readDtc(){let d=await api('/api/obd/dtc');dtc.textContent=JSON.stringify(d,null,2)}async function clearDtc(){let d=await api('/api/obd/clear_dtc',{method:'POST'});dtc.textContent=JSON.stringify(d,null,2)}</script></div></body></html>";

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}


static void url_decode_in_place(char *s)
{
    char *src = s;
    char *dst = s;

    while (*src) {
        if (src[0] == '%' && src[1] && src[2]) {
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);

            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
                continue;
            }
        }

        *dst++ = (*src == '+') ? ' ' : *src;
        src++;
    }

    *dst = '\0';
}

static void scan_classic_task(void *arg)
{
    (void)arg;
    esp_err_t err = bt_elm327_start_classic_scan();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "classic scan failed: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

static void scan_ble_task(void *arg)
{
    (void)arg;
    esp_err_t err = bt_elm327_start_ble_scan();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ble scan failed: %s", esp_err_to_name(err));
    }

    vTaskDelete(NULL);
}

static esp_err_t status_get(httpd_req_t *req)
{
    char saved[18] = "";
    char name[64] = "";
    (void)app_storage_load_elm(saved, sizeof(saved), name, sizeof(name));

    char json[256];
    snprintf(json, sizeof(json),
             "{\"bt_state\":\"%s\",\"connected_mac\":\"%s\",\"saved_mac\":\"%s\"}",
             bt_elm327_state_string(),
             bt_elm327_connected_mac(),
             saved);

    return send_json(req, json);
}

static esp_err_t scan_classic_post(httpd_req_t *req)
{
    BaseType_t ok = xTaskCreate(scan_classic_task, "classic_scan", 4096, NULL, 5, NULL);

    if (ok != pdPASS) {
        return send_json(req, "{\"ok\":false,\"error\":\"xTaskCreate failed\"}");
    }

    return send_json(req, "{\"ok\":true,\"message\":\"classic scan task created\"}");
}

static esp_err_t scan_ble_post(httpd_req_t *req)
{
    BaseType_t ok = xTaskCreate(scan_ble_task, "ble_scan", 4096, NULL, 5, NULL);

    if (ok != pdPASS) {
        return send_json(req, "{\"ok\":false,\"error\":\"xTaskCreate failed\"}");
    }

    return send_json(req, "{\"ok\":true,\"message\":\"ble scan task created\"}");
}

static esp_err_t devices_json(httpd_req_t *req, bool ble)
{
    bt_elm_device_t devices[BT_ELM_MAX_DEVICES];
    size_t count;

    if (ble) {
        count = bt_elm327_get_ble_devices(devices, BT_ELM_MAX_DEVICES);
    } else {
        count = bt_elm327_get_classic_devices(devices, BT_ELM_MAX_DEVICES);
    }

    char buf[4096];
    size_t off = 0;

    off += snprintf(buf + off, sizeof(buf) - off, "{\"devices\":[");

    for (size_t i = 0; i < count && off < sizeof(buf) - 160; i++) {
        off += snprintf(buf + off,
                        sizeof(buf) - off,
                        "%s{\"name\":\"%s\",\"mac\":\"%s\",\"rssi\":%d}",
                        i ? "," : "",
                        devices[i].name,
                        devices[i].mac,
                        devices[i].rssi);
    }

    snprintf(buf + off, sizeof(buf) - off, "]}");
    return send_json(req, buf);
}

static esp_err_t classic_devices_get(httpd_req_t *req)
{
    return devices_json(req, false);
}

static esp_err_t ble_devices_get(httpd_req_t *req)
{
    return devices_json(req, true);
}

static esp_err_t connect_post(httpd_req_t *req)
{
    char query[128];
    char mac[32];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "mac", mac, sizeof(mac)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":\"mac query required\"}");
    }

    url_decode_in_place(mac);

    esp_err_t err = bt_elm327_connect_mac_string(mac);
    char json[128];
    snprintf(json, sizeof(json),
             "{\"ok\":%s,\"error\":\"%s\"}",
             err == ESP_OK ? "true" : "false",
             esp_err_to_name(err));

    return send_json(req, json);
}

static esp_err_t obd_init_post(httpd_req_t *req)
{
    esp_err_t err = obd_init_elm();
    char json[96];
    snprintf(json, sizeof(json),
             "{\"ok\":%s,\"error\":\"%s\"}",
             err == ESP_OK ? "true" : "false",
             esp_err_to_name(err));

    return send_json(req, json);
}

static esp_err_t live_get(httpd_req_t *req)
{
    obd_live_data_t data;
    esp_err_t err = obd_read_live(&data);
    char json[256];

    if (err != ESP_OK) {
        snprintf(json, sizeof(json), "{\"error\":\"%s\"}", esp_err_to_name(err));
    } else {
        snprintf(json, sizeof(json),
                 "{\"rpm\":%d,\"speed_kmh\":%d,\"coolant_c\":%d,\"throttle_pct\":%d,\"ecu_voltage\":%.3f}",
                 data.rpm,
                 data.speed_kmh,
                 data.coolant_c,
                 data.throttle_pct,
                 data.ecu_voltage);
    }

    return send_json(req, json);
}

static esp_err_t dtc_get(httpd_req_t *req)
{
    char response[512] = "";
    esp_err_t err = obd_read_dtc(response, sizeof(response));
    char json[768];

    snprintf(json, sizeof(json),
             "{\"ok\":%s,\"raw\":\"%s\",\"error\":\"%s\"}",
             err == ESP_OK ? "true" : "false",
             response,
             esp_err_to_name(err));

    return send_json(req, json);
}

static esp_err_t clear_dtc_post(httpd_req_t *req)
{
    esp_err_t err = obd_clear_dtc();
    char json[96];

    snprintf(json, sizeof(json),
             "{\"ok\":%s,\"error\":\"%s\"}",
             err == ESP_OK ? "true" : "false",
             esp_err_to_name(err));

    return send_json(req, json);
}

#define MAKE_URI(path_value, http_method, handler_func) { .uri = path_value, .method = http_method, .handler = handler_func, .user_ctx = NULL }

esp_err_t app_web_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 16;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        return err;
    }

    httpd_uri_t handlers[] = {
        MAKE_URI("/", HTTP_GET, root_get),
        MAKE_URI("/api/status", HTTP_GET, status_get),
        MAKE_URI("/api/bt/classic/scan", HTTP_POST, scan_classic_post),
        MAKE_URI("/api/bt/ble/scan", HTTP_POST, scan_ble_post),
        MAKE_URI("/api/bt/classic/devices", HTTP_GET, classic_devices_get),
        MAKE_URI("/api/bt/ble/devices", HTTP_GET, ble_devices_get),
        MAKE_URI("/api/bt/connect", HTTP_POST, connect_post),
        MAKE_URI("/api/obd/init", HTTP_POST, obd_init_post),
        MAKE_URI("/api/obd/live", HTTP_GET, live_get),
        MAKE_URI("/api/obd/dtc", HTTP_GET, dtc_get),
        MAKE_URI("/api/obd/clear_dtc", HTTP_POST, clear_dtc_post),
    };

    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &handlers[i]));
    }

    ESP_LOGI(TAG, "Web server started: http://10.10.10.1");
    return ESP_OK;
}
