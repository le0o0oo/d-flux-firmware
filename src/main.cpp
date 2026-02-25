#include <Arduino.h>
#include <Preferences.h>
#include <esp32-hal-ledc.h>

#include "bluetooth.h"
#include "commands.h"
#include "config.h"
#include "sensor.h"

Preferences preferences;
int lastFlashTime = 0;

void setup() {
  Serial.begin(115200);
  preferences.begin("d-flux", false);

  Serial.println("--- SETTINGS ---");
  for (auto &settingKey : settingsKeys) {
    Serial.println(
        String(settingKey.key) + " = " +
        String(preferences.getFloat(settingKey.key, settingKey.defaultValue)));
  }
  Serial.println("----------------");

  pinMode(LED_PIN, OUTPUT);
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);

  setupSensor();
  setupBluetooth();
}

void loop() {
  if (!deviceConnected &&
      millis() - lastFlashTime > NOT_CONNECTED_FLASH_INTERVAL) {
    lastFlashTime = millis();
    digitalWrite(LED_PIN, HIGH);
  } else if (millis() - lastFlashTime > NOT_CONNECTED_FLASH_INTERVAL / 2) {
    digitalWrite(LED_PIN, LOW);
  }

  readSensorData();
  delay(20);
}
