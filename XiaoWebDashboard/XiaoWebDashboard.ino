// Seeed Studio XIAO ESP32-S3 web dashboard.
// - Turn the user LED (GPIO21) on/off from a web page.
// - Show the touch sensor (D0 = GPIO1, T1) state live; touching also lights the LED.
// Open http://xiao.local or the IP printed on Serial.
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";  // ESP32-S3 supports 2.4 GHz only
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const int TOUCH_PIN = T1;  // D0
WebServer server(80);
bool ledOn = false;
uint32_t baseline = 0;
uint32_t touchValue = 0;
uint32_t touchPeak = 0;
bool touched = false;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO 대시보드</title>
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
</style></head><body><main>
<h1>XIAO ESP32-S3</h1><div class="sub" id="conn">연결 중…</div>
<div class="card"><div class="row"><div><div class="label">LED</div><div class="state" id="ledText">-</div></div><div class="dot" id="ledDot"></div></div>
<div class="btns"><button class="primary" onclick="setLed(1)">켜기</button><button onclick="setLed(0)">끄기</button></div></div>
<div class="card"><div class="row"><div><div class="label">터치센서 (D0 핀)</div><div class="state" id="touchText">-</div></div><div class="dot" id="touchDot"></div></div>
<div class="val" id="touchVal"></div></div>
</main><script>
function show(s){
  ledText.textContent=s.led?'켜짐':'꺼짐';ledDot.classList.toggle('on',s.led);
  touchText.textContent=s.touched?'터치됨':'안 됨';touchDot.classList.toggle('on',s.touched);
  touchVal.textContent='측정값 '+s.value+' / 기준값 '+s.baseline;
  conn.textContent='연결됨';
}
async function poll(){try{show(await (await fetch('/status')).json())}catch(e){conn.textContent='연결 끊김'}}
async function setLed(v){try{show(await (await fetch('/led?on='+v)).json())}catch(e){}}
poll();setInterval(poll,300);
</script></body></html>)HTML";

void updateLedPin() {
  digitalWrite(LED_BUILTIN, (ledOn || touched) ? LOW : HIGH);  // active-low
}

void setLed(bool on) {
  ledOn = on;
  updateLedPin();
}

void sendStatus() {
  char json[128];
  snprintf(json, sizeof(json), "{\"led\":%s,\"touched\":%s,\"value\":%lu,\"baseline\":%lu,\"peak\":%lu}",
           (ledOn || touched) ? "true" : "false", touched ? "true" : "false", touchValue, baseline, touchPeak);
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  setLed(false);

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
  setLed(false);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  MDNS.begin("xiao");

  server.on("/", []() { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/status", sendStatus);
  server.on("/led", []() {
    if (server.hasArg("on")) setLed(server.arg("on") == "1");
    sendStatus();
  });
  server.begin();
}

void loop() {
  server.handleClient();
  touchValue = touchRead(TOUCH_PIN);
  if (touchValue > touchPeak) touchPeak = touchValue;
  // Untouched noise is only ~±20, so a 5% rise is a reliable touch.
  touched = touchValue > baseline * 1.05;
  updateLedPin();
  delay(2);
}
