// Seeed Studio XIAO ESP32-S3 <-> Supabase (works from outside the LAN).
// - Reads `led` from table device_state (id = 'xiao') and drives the user LED (GPIO21).
// - Writes touch sensor (D0 = GPIO1, T1) state and the board's LAN IP back to the same row.
// - Streams the Sense camera on the local network: http://<board IP>/ (MJPEG on port 81).
// Build with PSRAM=opi.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "esp_http_server.h"

const char* WIFI_SSID = "YOUR_WIFI_SSID";  // ESP32-S3 supports 2.4 GHz only
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char* SUPABASE_URL = "https://ulvsjlyfmadkiqauwpik.supabase.co";
const char* SUPABASE_KEY = "sb_publishable_NmGyXMXaqLYfJMfHGSffWg_3kAxmurA";
const char* DEVICE_ID = "xiao";

const int TOUCH_PIN = T1;  // D0
WiFiClientSecure tls;
bool ledOn = false;
bool touched = false;
bool lastSentTouched = false;
uint32_t baseline = 0;
uint32_t touchValue = 0;
unsigned long lastPoll = 0;
unsigned long lastPush = 0;

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

bool cameraOk = false;
esp_err_t cameraErr = ESP_OK;

const char CAM_PAGE[] = R"HTML(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO 카메라</title>
<style>
body{margin:0;background:#111318;color:#e8eaed;font-family:system-ui,sans-serif;text-align:center}
main{max-width:720px;margin:0 auto;padding:16px}
img{width:100%;border-radius:12px;background:#000;aspect-ratio:4/3}
p{color:#9aa0a6;font-size:14px}
</style></head><body><main>
<h1>XIAO 카메라</h1>
<img id="cam" alt="camera stream">
<p>같은 와이파이에서만 보여요.</p>
</main><script>
cam.src = location.protocol + '//' + location.hostname + ':81/stream';
</script></body></html>)HTML";

esp_err_t pageHandler(httpd_req_t* req) {
  if (!cameraOk) {
    char msg[96];
    snprintf(msg, sizeof(msg), "camera init failed: 0x%x (psram %s)", cameraErr, psramFound() ? "ok" : "missing");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, CAM_PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t streamHandler(httpd_req_t* req) {
  static const char* BOUNDARY = "frame";
  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char part[64];
  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return ESP_FAIL;
    int n = snprintf(part, sizeof(part), "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                     BOUNDARY, fb->len);
    esp_err_t res = httpd_resp_send_chunk(req, part, n);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) return res;  // client closed the page
  }
}

void startCameraServer() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t page = nullptr;
  httpd_uri_t pageUri = { .uri = "/", .method = HTTP_GET, .handler = pageHandler, .user_ctx = nullptr };
  if (httpd_start(&page, &cfg) == ESP_OK) httpd_register_uri_handler(page, &pageUri);

  // Stream on its own server/port so a long-lived stream doesn't block the page.
  cfg.server_port = 81;
  cfg.ctrl_port += 1;
  httpd_handle_t stream = nullptr;
  httpd_uri_t streamUri = { .uri = "/stream", .method = HTTP_GET, .handler = streamHandler, .user_ctx = nullptr };
  if (httpd_start(&stream, &cfg) == ESP_OK) httpd_register_uri_handler(stream, &streamUri);
}

void updateLedPin() {
  digitalWrite(LED_BUILTIN, (ledOn || touched) ? LOW : HIGH);  // active-low
}

String rowUrl() {
  return String(SUPABASE_URL) + "/rest/v1/device_state?id=eq." + DEVICE_ID;
}

void addHeaders(HTTPClient& http) {
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Content-Type", "application/json");
}

void pollLed() {
  HTTPClient http;
  http.begin(tls, rowUrl() + "&select=led");
  addHeaders(http);
  if (http.GET() == 200) {
    // Response looks like [{"led":true}]
    ledOn = http.getString().indexOf("true") >= 0;
  }
  http.end();
}

void pushTouch() {
  HTTPClient http;
  http.begin(tls, rowUrl());
  addHeaders(http);
  http.addHeader("Prefer", "return=minimal");
  char body[160];
  snprintf(body, sizeof(body), "{\"touched\":%s,\"touch_value\":%lu,\"ip\":\"%s\",\"camera\":%s,\"updated_at\":\"now\"}",
           touched ? "true" : "false", touchValue, WiFi.localIP().toString().c_str(), cameraOk ? "true" : "false");
  if (http.PATCH(body) / 100 == 2) lastSentTouched = touched;
  http.end();
  lastPush = millis();
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

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Blink while connecting.
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(250);
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  camera_config_t cam = cameraConfig();
  cameraErr = esp_camera_init(&cam);
  cameraOk = cameraErr == ESP_OK;
  startCameraServer();
  Serial.printf("camera: %s\n", cameraOk ? "ok" : "failed");

  tls.setInsecure();  // skip cert check to keep the sketch simple
  touchValue = touchRead(TOUCH_PIN);
  pushTouch();
}

void loop() {
  touchValue = touchRead(TOUCH_PIN);
  // Untouched noise is only ~±20, so a 5% rise is a reliable touch.
  touched = touchValue > baseline * 1.05;
  updateLedPin();

  // Send touch changes right away (plus a 5 s heartbeat); poll the LED command every 500 ms.
  if (touched != lastSentTouched || millis() - lastPush > 5000) pushTouch();
  if (millis() - lastPoll > 500) {
    lastPoll = millis();
    pollLed();
    updateLedPin();
  }
  delay(10);
}
