// Seeed Studio XIAO ESP32-S3 over BLE: phone reads the touch sensor and controls the LED.
// - Advertises as "XIAO-BLE".
// - Touch characteristic (read/notify): "<value>,<0|1>"  e.g. "17012,0"
// - LED characteristic (read/write): write "1" = on, "0" = off
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define TOUCH_UUID   "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define LED_UUID     "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

const int TOUCH_PIN = T1;  // D0
BLECharacteristic* touchChar;
BLECharacteristic* ledChar;
bool connected = false;
bool ledOn = false;
bool touched = false;
uint32_t baseline = 0;
uint32_t touchValue = 0;
unsigned long lastNotify = 0;

void updateLedPin() {
  digitalWrite(LED_BUILTIN, (ledOn || touched) ? LOW : HIGH);  // active-low
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { connected = true; }
  void onDisconnect(BLEServer*) override {
    connected = false;
    BLEDevice::startAdvertising();  // let the phone reconnect
  }
};

class LedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String v = c->getValue();
    ledOn = v.length() > 0 && v[0] == '1';
    updateLedPin();
  }
};

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

  BLEDevice::init("XIAO-BLE");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService* service = server->createService(SERVICE_UUID);

  touchChar = service->createCharacteristic(TOUCH_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  touchChar->addDescriptor(new BLE2902());

  ledChar = service->createCharacteristic(LED_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  ledChar->setCallbacks(new LedCallbacks());
  ledChar->setValue("0");

  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising as XIAO-BLE");
}

void loop() {
  touchValue = touchRead(TOUCH_PIN);
  // Untouched noise is only ~±20, so a 5% rise is a reliable touch.
  bool nowTouched = touchValue > baseline * 1.05;
  bool changed = nowTouched != touched;
  touched = nowTouched;
  updateLedPin();

  // Notify on touch change right away, otherwise every 300 ms.
  if (connected && (changed || millis() - lastNotify > 300)) {
    String v = String(touchValue) + "," + (touched ? "1" : "0");
    touchChar->setValue(v.c_str());
    touchChar->notify();
    lastNotify = millis();
  }
  delay(20);
}
