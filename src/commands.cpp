#include "commands.h"
#include "bluetooth.h"
#include "config.h"
#include "sensor.h"
#include "servo.h"

void sendCommand(String cmd, String payload) {
  if (!deviceConnected)
    return;

  pTxCharacteristic->setValue((cmd + ' ' + payload + '\n').c_str());
  pTxCharacteristic->notify();
  if (DEBUG)
    Serial.println("-> TX: " + cmd + " " + payload);
}

void settingsParser(String payload, int mode) {
  if (mode == SETTING_MODE_READ) {
    String response = "";
    for (auto &settingKey : settingsKeys) {
      float val = preferences.getFloat(settingKey.key, settingKey.defaultValue);
      response += String(settingKey.key) + "=" + String(val) + ";";
    }
    sendCommand("SETTINGS", response);

  } else if (mode == SETTING_MODE_WRITE) {
    bool updated = false;

    for (const auto &item : settingsKeys) {
      String searchToken = String(item.key) + "=";

      int keyIndex = payload.indexOf(searchToken);

      if (keyIndex != -1) {
        int valueStart = keyIndex + searchToken.length();

        int valueEnd = payload.indexOf(';', valueStart);

        if (valueEnd == -1) {
          valueEnd = payload.length();
        }

        String valueStr = payload.substring(valueStart, valueEnd);
        float newValue = valueStr.toFloat();

        preferences.putFloat(item.key, newValue);
        updated = true;

        if (DEBUG) {
          Serial.print("Updated [");
          Serial.print(item.key);
          Serial.print("] to: ");
          Serial.println(newValue);
        }
      }
    }

    if (updated) {
      settingsParser("", SETTING_MODE_READ);
    }
  }
}

void commandHandler(String cmd, String payload) {
  if (cmd == "START_ACQUISITION") {
    startSpin();
    acquire = true;
  } else if (cmd == "STOP_ACQUISITION") {
    stopSpin();
    acquire = false;
  } else if (cmd == "WHOIS") {
    sendCommand("WHOIS", String(manufacturerText.c_str()));
  } else if (cmd == "GET_ACQUISITION_STATE") {
    sendCommand("ACQUISITION_STATE", String(acquire));
  } else if (cmd == "DISCONNECT") {
    if (deviceConnected)
      pServer->disconnect(currentConnHandle);
  } else if (cmd == "GET_SETTINGS") {
    settingsParser(payload, SETTING_MODE_READ);
  } else if (cmd == "SET_SETTINGS") {
    settingsParser(payload, SETTING_MODE_WRITE);
  } else if (cmd == "SET_HW_CALIBRATION_REF") {
    const int newVal = payload.toInt();
    if (newVal < 400 || newVal > 2000)
      return;
    calibrationWithReference(newVal);
    sendCommand("HW_CALIBRATION_REF",
                String(scd30.getForcedCalibrationReference()));
  } else if (cmd == "GET_HW_CALIBRATION_REF") {
    sendCommand("HW_CALIBRATION_REF",
                String(scd30.getForcedCalibrationReference()));
  }
}
