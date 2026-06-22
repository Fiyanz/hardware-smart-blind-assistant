#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

constexpr uint8_t BTN_ACTION_PIN = 2;
constexpr uint8_t BTN_MODE_PIN = 3;
constexpr uint8_t LED_PIN = 8;

constexpr bool LED_ON = LOW;
constexpr bool LED_OFF = HIGH;

constexpr const char* BLE_DEVICE_NAME = "SightAssist-ESP32";
constexpr const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
constexpr const char* TRIGGER_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

constexpr uint8_t CMD_ACTION = 0x01;
constexpr uint8_t CMD_NEXT_MODE = 0x02;
constexpr uint8_t CMD_STOP_ALL = 0x03;

constexpr unsigned long DEBOUNCE_MS = 250;
constexpr unsigned long LOOP_DELAY_MS = 10;
constexpr unsigned long LED_FEEDBACK_MS = 50;
constexpr unsigned long RECONNECT_DELAY_MS = 500;

struct ButtonState {
  uint8_t pin;
  bool raw;
  bool stablePressed;
  unsigned long lastRawChangeMs;

  ButtonState(uint8_t buttonPin)
    : pin(buttonPin), raw(false), stablePressed(false), lastRawChangeMs(0) {}
};

struct ButtonEvent {
  bool pressed;
  bool released;
};

BLEServer* pServer = nullptr;
BLECharacteristic* pTriggerCharacteristic = nullptr;
BLEAdvertising* pAdvertising = nullptr;

ButtonState actionButton(BTN_ACTION_PIN);
ButtonState modeButton(BTN_MODE_PIN);

bool deviceConnected = false;
bool stopAllSentWhilePressed = false;
bool ledFeedbackActive = false;
bool reconnectRequested = false;
unsigned long lastCommandSentAt = 0;
unsigned long ledFeedbackStartedAt = 0;
unsigned long reconnectRequestedAt = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    deviceConnected = true;
    reconnectRequested = false;
    digitalWrite(LED_PIN, LED_ON);
    ::Serial.println(">> Perangkat terhubung!");
  }

  void onDisconnect(BLEServer* server) {
    deviceConnected = false;
    reconnectRequested = true;
    reconnectRequestedAt = millis();
    ledFeedbackActive = false;
    digitalWrite(LED_PIN, LED_OFF);
    ::Serial.println(">> Perangkat terputus!");
  }
};

ButtonEvent updateButton(ButtonState& button) {
  const bool raw = digitalRead(button.pin) == LOW;
  const unsigned long now = millis();

  if (raw != button.raw) {
    button.raw = raw;
    button.lastRawChangeMs = now;
  }

  if (now - button.lastRawChangeMs >= DEBOUNCE_MS && button.stablePressed != button.raw) {
    const bool previousState = button.stablePressed;
    button.stablePressed = button.raw;
    return {button.raw, previousState};
  }

  return {false, false};
}

void startLedFeedback() {
  ledFeedbackActive = true;
  ledFeedbackStartedAt = millis();
  digitalWrite(LED_PIN, LED_OFF);
}

void updateLedFeedback() {
  if (!ledFeedbackActive) {
    return;
  }

  if (millis() - ledFeedbackStartedAt >= LED_FEEDBACK_MS) {
    ledFeedbackActive = false;
    digitalWrite(LED_PIN, deviceConnected ? LED_ON : LED_OFF);
  }
}

void sendCommand(uint8_t command) {
  const unsigned long now = millis();
  if (now - lastCommandSentAt < DEBOUNCE_MS) {
    return;
  }

  lastCommandSentAt = now;

  if (pTriggerCharacteristic == nullptr) {
    Serial.println("!! Karakteristik BLE belum siap");
    return;
  }

  if (!deviceConnected) {
    Serial.println("!! Tidak ada perangkat terhubung");
    return;
  }

  pTriggerCharacteristic->setValue(&command, 1);
  pTriggerCharacteristic->notify();

  Serial.print(">> send ");
  Serial.print(command, HEX);
  if (command == CMD_ACTION) {
    Serial.println(" (ACTION)");
  } else if (command == CMD_NEXT_MODE) {
    Serial.println(" (NEXT_MODE)");
  } else if (command == CMD_STOP_ALL) {
    Serial.println(" (STOP_ALL)");
  } else {
    Serial.println();
  }

  startLedFeedback();
}

void handleButtons() {
  const ButtonEvent actionEvent = updateButton(actionButton);
  const ButtonEvent modeEvent = updateButton(modeButton);

  if (actionButton.stablePressed && modeButton.stablePressed) {
    if (!stopAllSentWhilePressed) {
      Serial.println("[BTN] ACTION && MODE -> STOP_ALL");
      sendCommand(CMD_STOP_ALL);
      stopAllSentWhilePressed = true;
    }
    return;
  }

  if (!actionButton.stablePressed || !modeButton.stablePressed) {
    stopAllSentWhilePressed = false;
  }

  if (actionEvent.pressed) {
    Serial.print("[BTN] ACTION pin=");
    Serial.print(BTN_ACTION_PIN);
    Serial.print(" -> send 0x");
    Serial.println(CMD_ACTION, HEX);
    sendCommand(CMD_ACTION);
  }

  if (modeEvent.pressed) {
    Serial.print("[BTN] MODE pin=");
    Serial.print(BTN_MODE_PIN);
    Serial.print(" -> send 0x");
    Serial.println(CMD_NEXT_MODE, HEX);
    sendCommand(CMD_NEXT_MODE);
  }
}

void handleBleReconnect() {
  if (!deviceConnected && reconnectRequested && millis() - reconnectRequestedAt >= RECONNECT_DELAY_MS) {
    reconnectRequested = false;
    if (pAdvertising != nullptr) {
      pAdvertising->start();
      Serial.println(">> Advertising ulang...");
    }
  }
}

void setupBle() {
  BLEDevice::init(BLE_DEVICE_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTriggerCharacteristic = pService->createCharacteristic(
    TRIGGER_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTriggerCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  pAdvertising->start();
}

void setupButtons() {
  pinMode(BTN_ACTION_PIN, INPUT_PULLUP);
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  actionButton.raw = digitalRead(BTN_ACTION_PIN) == LOW;
  modeButton.raw = digitalRead(BTN_MODE_PIN) == LOW;
  actionButton.stablePressed = actionButton.raw;
  modeButton.stablePressed = modeButton.raw;

  digitalWrite(LED_PIN, LED_OFF);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== SightAssist ESP32-C3 BLE Controller ===");

  setupButtons();
  setupBle();

  Serial.println(">> BLE advertising dimulai...");
  Serial.println(">> Menunggu koneksi dari SightAssist app...");
}

void loop() {
  static unsigned long lastDebug = 0;
  const unsigned long now = millis();

  if (now - lastDebug >= 1000) {
    lastDebug = now;
    Serial.print("[PIN] ACTION=");
    Serial.print(digitalRead(BTN_ACTION_PIN));
    Serial.print(" MODE=");
    Serial.print(digitalRead(BTN_MODE_PIN));
    Serial.print(" | stable_STOP_ALL=");
    Serial.println(stopAllSentWhilePressed);
  }

  updateLedFeedback();
  handleButtons();
  handleBleReconnect();
  delay(LOOP_DELAY_MS);
}
