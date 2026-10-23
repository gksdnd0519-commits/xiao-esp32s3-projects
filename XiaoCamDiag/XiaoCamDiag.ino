// XIAO ESP32-S3 Sense camera diagnostic.
// Drives XCLK, scans the camera I2C (SCCB) bus and reads sensor IDs.
// Result is served as text on http://<board IP>/ over the home Wi-Fi.
#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const int XCLK_PIN = 10;
const int SDA_PIN = 40;
const int SCL_PIN = 39;

WebServer server(80);
String report;

int read16(uint8_t addr, uint16_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return -1;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

int read8(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return -1;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

void runScan() {
  report = "";
  report += "psram: " + String(psramFound() ? "ok" : "missing") + "\n";
  report += "SDA(40) idle level: " + String(digitalRead(SDA_PIN)) + ", SCL(39) idle level: " + String(digitalRead(SCL_PIN)) + "\n";

  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      report += "i2c device at 0x" + String(a, HEX) + "\n";
      found++;
    }
  }
  report += "devices found: " + String(found) + "\n";

  // OV5640 / OV3660 at 0x3C: chip ID in 0x300A/0x300B (OV5640 = 0x5640, OV3660 = 0x3660).
  int h = read16(0x3C, 0x300A), l = read16(0x3C, 0x300B);
  report += "0x3C id: " + String(h, HEX) + " " + String(l, HEX) + "\n";

  // OV2640 at 0x30: select sensor bank, PID in 0x0A/0x0B (0x26 0x42).
  Wire.beginTransmission(0x30);
  Wire.write(0xFF);
  Wire.write(0x01);
  Wire.endTransmission();
  report += "0x30 id: " + String(read8(0x30, 0x0A), HEX) + " " + String(read8(0x30, 0x0B), HEX) + "\n";
}

void setup() {
  Serial.begin(115200);
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(10);

  // Camera sensors only answer on I2C while XCLK is running.
  ledcAttach(XCLK_PIN, 20000000, 1);
  ledcWrite(XCLK_PIN, 1);
  delay(50);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  runScan();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  server.on("/", []() {
    runScan();
    server.send(200, "text/plain", report);
  });
  server.begin();
}

void loop() {
  server.handleClient();
}
