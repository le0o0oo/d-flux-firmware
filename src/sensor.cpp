#include "sensor.h"
#include "commands.h"

Adafruit_SCD30 scd30;
bool acquire = false;

void setupSensor() {
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SCD30 Found!");
}

void readSensorData() {
  if (deviceConnected && acquire) {
    if (scd30.dataReady()) {
      if (scd30.read()) {
        float temp = scd30.temperature;
        float hum = scd30.relative_humidity;
        float co2 = scd30.CO2;

        String payload = "CO2=" + String(co2, 2) + ";TMP=" + String(temp, 2) +
                         ";HUM=" + String(hum, 2);

        sendCommand("DATA", payload);

      } else {
        Serial.println("Error reading sensor");
      }
    }
  }
}

void calibrationWithReference(uint16_t ref) {
  scd30.forceRecalibrationWithReference(ref);
}