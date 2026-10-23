// Seeed Studio XIAO ESP32-S3: touch D0 pin (GPIO1, T1) -> user LED (GPIO21) on, release -> off.
// On ESP32-S3, touchRead() value rises when touched. User LED is active-low.
const int TOUCH_PIN = T1;  // D0
uint32_t baseline = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);

  // Blink 3 times at boot so it's visible the sketch is running.
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(150);
  }
  Serial.println("boot");

  // Calibrate untouched value at startup; don't touch the pin during this time.
  for (int i = 0; i < 16; i++) {
    baseline += touchRead(TOUCH_PIN);
    delay(20);
  }
  baseline /= 16;
  Serial.printf("baseline=%lu\n", baseline);
}

void loop() {
  uint32_t value = touchRead(TOUCH_PIN);
  bool touched = value > baseline * 1.3;

  digitalWrite(LED_BUILTIN, touched ? LOW : HIGH);

  Serial.printf("touch=%lu baseline=%lu %s\n", value, baseline, touched ? "ON" : "OFF");
  delay(50);
}
