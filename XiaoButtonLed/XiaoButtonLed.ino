// Seeed Studio XIAO ESP32-S3: hold BOOT button (GPIO0) -> user LED (GPIO21) on, release -> off.
// User LED is active-low; BOOT button reads LOW when pressed.
const int BUTTON_PIN = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}
