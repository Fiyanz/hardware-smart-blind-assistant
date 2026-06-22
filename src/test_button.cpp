#include <Arduino.h>

constexpr uint8_t BTN_ACTION_PIN = 2;
constexpr uint8_t BTN_MODE_PIN = 3;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BTN_ACTION_PIN, INPUT_PULLDOWN);
  pinMode(BTN_MODE_PIN, INPUT_PULLDOWN);

  Serial.println("Button reader - reading raw input states");
}

void loop() {
  const bool actionRead = digitalRead(BTN_ACTION_PIN);
  const bool modeRead = digitalRead(BTN_MODE_PIN);

  Serial.print("ACTION=");
  Serial.print(actionRead ? "HIGH" : "LOW");
  Serial.print(" | MODE=");
  Serial.println(modeRead ? "HIGH" : "LOW");

  delay(500);
}
