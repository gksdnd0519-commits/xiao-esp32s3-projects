// Seeed Studio XIAO ESP32-S3 (Sense) in AP mode: no router or internet needed.
// - The board makes its own Wi-Fi "XIAO-ESP32S3" (password 12345678).
// - Connect a phone to it and open http://192.168.4.1
// - Web page: touch sensor (D0 = GPIO1, T1) value, LED (GPIO21) on/off, camera stream (port 81).
// Build with PSRAM=opi.
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

const char* AP_SSID = "XIAO-ESP32S3";
const char* AP_PASS = "12345678";  // at least 8 characters

const int TOUCH_PIN = T1;  // D0
bool ledOn = false;
bool touched = false;
uint32_t baseline = 0;
uint32_t touchValue = 0;
bool cameraOk = false;
esp_err_t cameraErr = ESP_OK;

// XIAO ESP32-S3 Sense camera pins.
camera_config_t cameraConfig() {
  camera_config_t c = {};
  c.pin_pwdn = -1;
  c.pin_reset = -1;
  c.pin_xclk = 10;
  c.pin_sccb_sda = 40;
  c.pin_sccb_scl = 39;
  c.pin_d7 = 48;
  c.pin_d6 = 11;
  c.pin_d5 = 12;
  c.pin_d4 = 14;
  c.pin_d3 = 16;
  c.pin_d2 = 18;
  c.pin_d1 = 17;
  c.pin_d0 = 15;
  c.pin_vsync = 38;
  c.pin_href = 47;
  c.pin_pclk = 13;
  c.xclk_freq_hz = 20000000;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;
  return c;
}

const char PAGE[] = R"HTML(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO AP</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--text:#1d2129;--muted:#6b7280;--on:#f59e0b;--off:#d1d5db;--accent:#2563eb}
@media (prefers-color-scheme:dark){:root{--bg:#111318;--card:#1b1e25;--text:#e8eaed;--muted:#9aa0a6;--off:#3a3f4a}}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--text)}
main{max-width:480px;margin:0 auto;padding:24px 16px}h1{font-size:22px;margin:0 0 4px}
.sub{color:var(--muted);font-size:14px;margin-bottom:20px}
.card{background:var(--card);border-radius:16px;padding:20px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px}
.label{font-size:15px;color:var(--muted)}.state{font-size:28px;font-weight:700;margin-top:4px}
.dot{width:56px;height:56px;border-radius:50%;background:var(--off);transition:.15s}
.dot.on{background:var(--on);box-shadow:0 0 24px var(--on)}
.btns{display:flex;gap:10px;margin-top:16px}
button{flex:1;font-size:17px;padding:14px;border:0;border-radius:12px;cursor:pointer;background:var(--off);color:var(--text)}
button.primary{background:var(--accent);color:#fff}
.val{color:var(--muted);font-size:13px;margin-top:12px}
img{width:100%;border-radius:12px;background:#000;aspect-ratio:4/3;display:block}
</style></head><body><main>
<h1>XIAO ESP32-S3</h1><div class="sub" id="conn">연결 중…</div>
<div class="card"><div class="label" style="margin-bottom:10px">카메라</div><div id="camBox"></div></div>
<div class="card"><div class="row"><div><div class="label">LED</div><div class="state" id="ledText">-</div></div><div class="dot" id="ledDot"></div></div>
<div class="btns"><button class="primary" onclick="setLed(1)">켜기</button><button onclick="setLed(0)">끄기</button></div></div>
<div class="card"><div class="row"><div><div class="label">터치센서 (D0 핀)</div><div class="state" id="touchText">-</div></div><div class="dot" id="touchDot"></div></div>
<div class="val" id="touchVal"></div></div>
</main><script>
let camShown=false;
function show(s){
  ledText.textContent=s.led?'켜짐':'꺼짐';ledDot.classList.toggle('on',s.led);
  touchText.textContent=s.touched?'터치됨':'안 됨';touchDot.classList.toggle('on',s.touched);
  touchVal.textContent='측정값 '+s.value+' / 기준값 '+s.baseline;
  conn.textContent='연결됨 (인터넷 없이 보드에 직접 연결)';
  if(!camShown){camShown=true;
    camBox.innerHTML=s.camera?'<img src="http://'+location.hostname+':81/stream" alt="camera">'
      :'<div class="val">카메라를 찾지 못했어요 (오류 '+s.cameraErr+'). 카메라 보드를 끝까지 끼우고 다시 켜 주세요.</div>';}
}
async function poll(){try{show(await (await fetch('/status')).json())}catch(e){conn.textContent='연결 끊김'}}
async function setLed(v){try{show(await (await fetch('/led?on='+v)).json())}catch(e){}}
poll();setInterval(poll,300);
</script></body></html>)HTML";

void updateLedPin() {
  digitalWrite(LED_BUILTIN, (ledOn || touched) ? LOW : HIGH);  // active-low
}

esp_err_t sendStatus(httpd_req_t* req) {
  char json[192];
  snprintf(json, sizeof(json),
           "{\"led\":%s,\"touched\":%s,\"value\":%lu,\"baseline\":%lu,\"camera\":%s,\"cameraErr\":\"0x%x\"}",
           (ledOn || touched) ? "true" : "false", touched ? "true" : "false", touchValue, baseline,
           cameraOk ? "true" : "false", cameraErr);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t pageHandler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t ledHandler(httpd_req_t* req) {
  char query[32], value[4];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "on", value, sizeof(value)) == ESP_OK) {
    ledOn = value[0] == '1';
    updateLedPin();
  }
  return sendStatus(req);
}

esp_err_t streamHandler(httpd_req_t* req) {
  if (!cameraOk) return httpd_resp_send_404(req);
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  char part[64];
  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return ESP_FAIL;
    int n = snprintf(part, sizeof(part), "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    esp_err_t res = httpd_resp_send_chunk(req, part, n);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) return res;  // client closed the page
  }
}

void startServers() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t web = nullptr;
  if (httpd_start(&web, &cfg) == ESP_OK) {
    httpd_uri_t page = { .uri = "/", .method = HTTP_GET, .handler = pageHandler, .user_ctx = nullptr };
    httpd_uri_t status = { .uri = "/status", .method = HTTP_GET, .handler = sendStatus, .user_ctx = nullptr };
    httpd_uri_t led = { .uri = "/led", .method = HTTP_GET, .handler = ledHandler, .user_ctx = nullptr };
    httpd_register_uri_handler(web, &page);
    httpd_register_uri_handler(web, &status);
    httpd_register_uri_handler(web, &led);
  }

  // Stream on its own server/port so a long-lived stream doesn't block the page.
  cfg.server_port = 81;
  cfg.ctrl_port += 1;
  httpd_handle_t stream = nullptr;
  if (httpd_start(&stream, &cfg) == ESP_OK) {
    httpd_uri_t s = { .uri = "/stream", .method = HTTP_GET, .handler = streamHandler, .user_ctx = nullptr };
    httpd_register_uri_handler(stream, &s);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Calibrate untouched value; don't touch D0 during boot.
  // First readings after init are garbage, so discard them.
  for (int i = 0; i < 10; i++) {
    touchRead(TOUCH_PIN);
    delay(20);
  }
  for (int i = 0; i < 16; i++) {
    baseline += touchRead(TOUCH_PIN);
    delay(20);
  }
  baseline /= 16;

  camera_config_t cam = cameraConfig();
  cameraErr = esp_camera_init(&cam);
  cameraOk = cameraErr == ESP_OK;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP %s ready, open http://%s  camera: %s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str(), cameraOk ? "ok" : "failed");

  startServers();
}

void loop() {
  touchValue = touchRead(TOUCH_PIN);
  // Untouched noise is only ~±20, so a 5% rise is a reliable touch.
  touched = touchValue > baseline * 1.05;
  updateLedPin();
  delay(20);
}
