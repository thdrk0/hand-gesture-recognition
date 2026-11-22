#include "preview.hpp"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <inttypes.h>

namespace {
constexpr const char *tag = "camera_preview";
constexpr std::size_t capacity = 128 * 1024;
SemaphoreHandle_t lock = nullptr;
std::uint8_t *latest = nullptr;
std::uint8_t *sending = nullptr;
std::size_t latest_size = 0;
std::uint64_t frame_ms = 0;
struct State {
    char label[48] = "waiting";
    float confidence = 0;
    unsigned hands = 0;
    int digit = 0;
    int stable = 0;
    std::uint64_t cycle_ms = 0;
};
State state;
bool ready = false;

constexpr const char page[] = R"HTML(<!doctype html>
<html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Camera ESP32</title><style>
body{margin:24px auto;padding:0 16px;max-width:760px;font:17px system-ui;background:#121821;color:#edf4ff}
img{width:100%;max-width:640px;image-rendering:auto;border-radius:12px;background:#000;min-height:200px}
pre{white-space:pre-wrap;background:#202b38;padding:16px;border-radius:10px}button{padding:10px;font:inherit}
</style><h1>Camera ESP32</h1><p>Image utilisee pour la reconnaissance, 320 x 240 pixels.</p>
<img id="camera" alt="En attente de la camera"><p id="connection">Connexion...</p>
<pre id="status">Chargement des diagnostics...</pre><button id="pause">Pause</button>
<p>Une seule main, paume face a la camera. Maintenir le geste environ une seconde.
Le label indique aussi les gestes rejetes (like, no_gesture...).
L'image et les diagnostics sont actualises separement.</p>
<script>
const img=document.getElementById('camera'), status=document.getElementById('status'), connection=document.getElementById('connection');
let paused=false;
document.getElementById('pause').onclick=function(){paused=!paused;this.textContent=paused?'Reprendre':'Pause';};
async function refresh(){
 try{
  if(!paused){
   const image=await fetch('/frame.jpg?t='+Date.now(),{cache:'no-store',signal:AbortSignal.timeout(5000)});
   if(!image.ok)throw Error('Image indisponible');
   const url=URL.createObjectURL(await image.blob());
   await new Promise((resolve,reject)=>{img.onload=resolve;img.onerror=reject;img.src=url;}).finally(()=>URL.revokeObjectURL(url));
   const r=await fetch('/status',{cache:'no-store',signal:AbortSignal.timeout(5000)});
   if(!r.ok)throw Error('Diagnostics indisponibles');
   const s=await r.json();
   connection.textContent=s.frame_age_ms>2000?'Attention : image ancienne ('+s.frame_age_ms+' ms)':'Camera active';
   status.textContent='Mains : '+s.hands+'\nClasse : '+s.label+'\nConfiance : '+Math.round(s.confidence*100)+' %\nChiffre accepte : '+s.digit+'\nChiffre stable : '+s.stable+'\nCycle : '+s.cycle_ms+' ms';
  }
 }catch(e){connection.textContent='En attente de la camera ou du reseau...';}
 setTimeout(refresh,350);
}
refresh();
</script></html>)HTML";

esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

esp_err_t frame_handler(httpd_req_t *req)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const auto size = latest_size;
    if (size) std::memcpy(sending, latest, size);
    xSemaphoreGive(lock);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (!size) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "No camera frame yet", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_type(req, "image/jpeg");
    // One HTTP server task owns sending. No network I/O while holding the lock.
    return httpd_resp_send(req, reinterpret_cast<const char *>(sending), size);
}

esp_err_t status_handler(httpd_req_t *req)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    const auto snapshot = state;
    const auto captured = frame_ms;
    xSemaphoreGive(lock);
    char json[320];
    const auto age = static_cast<std::uint64_t>(esp_timer_get_time() / 1000) - captured;
    const int size = std::snprintf(json, sizeof(json),
        "{\"label\":\"%s\",\"confidence\":%.3f,\"hands\":%u,\"digit\":%d,\"stable\":%d,"
        "\"cycle_ms\":%" PRIu64 ",\"frame_age_ms\":%" PRIu64 "}",
        snapshot.label, snapshot.confidence, snapshot.hands, snapshot.digit, snapshot.stable,
        snapshot.cycle_ms, age);
    if (size < 0 || size >= static_cast<int>(sizeof(json))) return ESP_FAIL;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, size);
}
} // namespace

esp_err_t preview_start()
{
    lock = xSemaphoreCreateMutex();
    latest = static_cast<std::uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    sending = static_cast<std::uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!lock || !latest || !sending) {
        heap_caps_free(latest);
        heap_caps_free(sending);
        latest = sending = nullptr;
        if (lock) vSemaphoreDelete(lock);
        lock = nullptr;
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(nvs_flash_init(), tag, "NVS initialization failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), tag, "Network initialization failed");
    const auto loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) return loop;
    if (!esp_netif_create_default_wifi_ap()) return ESP_FAIL;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), tag, "Wi-Fi initialization failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), tag, "Wi-Fi storage failed");
    wifi_config_t config{};
    std::strcpy(reinterpret_cast<char *>(config.ap.ssid), "HandGesture-ESP32");
    std::strcpy(reinterpret_cast<char *>(config.ap.password), "gesture123");
    config.ap.ssid_len = std::strlen("HandGesture-ESP32");
    config.ap.channel = 1;
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), tag, "AP mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), tag, "AP configuration failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), tag, "Wi-Fi start failed");
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.stack_size = 6144;
    http.max_open_sockets = 4;
    http.lru_purge_enable = true;
    http.send_wait_timeout = 2;
    httpd_handle_t server = nullptr;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &http), tag, "HTTP server failed");
    const char *uris[] = {"/", "/frame.jpg", "/status"};
    esp_err_t (*handlers[])(httpd_req_t *) = {index_handler, frame_handler, status_handler};
    for (unsigned i = 0; i < 3; ++i) {
        httpd_uri_t uri{};
        uri.uri = uris[i];
        uri.method = HTTP_GET;
        uri.handler = handlers[i];
        const auto error = httpd_register_uri_handler(server, &uri);
        if (error != ESP_OK) { httpd_stop(server); return error; }
    }
    ready = true;
    return ESP_OK;
}

void preview_frame(const std::uint8_t *jpeg, std::size_t size)
{
    if (!ready || !jpeg || !size || size > capacity) return;
    if (xSemaphoreTake(lock, pdMS_TO_TICKS(5)) != pdTRUE) return;
    std::memcpy(latest, jpeg, size);
    latest_size = size;
    frame_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
    xSemaphoreGive(lock);
}

void preview_result(const char *label, float confidence, unsigned hands,
                    int digit, int stable, std::uint64_t cycle_ms)
{
    if (!ready) return;
    xSemaphoreTake(lock, portMAX_DELAY);
    // Model labels are controlled strings; sanitize before JSON serialization.
    std::size_t i = 0;
    for (; label && label[i] && i < sizeof(state.label) - 1; ++i) {
        const char c = label[i];
        state.label[i] = ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') ? c : '_';
    }
    state.label[i] = '\0';
    state.confidence = std::isfinite(confidence) ? confidence : 0;
    state.hands = hands;
    state.digit = digit;
    state.stable = stable;
    state.cycle_ms = cycle_ms;
    xSemaphoreGive(lock);
}
