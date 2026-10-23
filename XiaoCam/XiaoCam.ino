// Seeed Studio XIAO ESP32-S3 Sense: camera streaming only.
// Joins the home Wi-Fi; open http://<board IP>/ on the same network (MJPEG stream on port 81).
// Build with PSRAM=opi.
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

const char* WIFI_SSID = "YOUR_WIFI_SSID";  // ESP32-S3 supports 2.4 GHz only
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

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
<title>XIAO 카메라</title>
<style>
body{margin:0;background:#111318;color:#e8eaed;font-family:system-ui,sans-serif;text-align:center}
main{max-width:720px;margin:0 auto;padding:16px}
img{width:100%;border-radius:12px;background:#000;aspect-ratio:4/3}
</style></head><body><main>
<h1>XIAO 카메라</h1>
<img id="cam" alt="camera stream">
</main><script>
cam.src = 'http://' + location.hostname + ':81/stream';
</script></body></html>)HTML";

esp_err_t pageHandler(httpd_req_t* req) {
  if (!cameraOk) {
    char msg[64];
    snprintf(msg, sizeof(msg), "camera init failed: 0x%x", cameraErr);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
  }
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

esp_err_t captureHandler(httpd_req_t* req) {
  camera_fb_t* fb = cameraOk ? esp_camera_fb_get() : nullptr;
  if (!fb) return httpd_resp_send_500(req);
  httpd_resp_set_type(req, "image/jpeg");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
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
    httpd_uri_t capture = { .uri = "/capture", .method = HTTP_GET, .handler = captureHandler, .user_ctx = nullptr };
    httpd_register_uri_handler(web, &page);
    httpd_register_uri_handler(web, &capture);
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

  camera_config_t cam = cameraConfig();
  cameraErr = esp_camera_init(&cam);
  cameraOk = cameraErr == ESP_OK;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  Serial.printf("open http://%s  camera: %s\n", WiFi.localIP().toString().c_str(), cameraOk ? "ok" : "failed");

  startServers();
}

void loop() {
  delay(1000);
}
