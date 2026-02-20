#include "NimBLEClient.h"
#include <Adafruit_SCD30.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <NimBLEConnInfo.h>
#include <NimBLEDevice.h>

#define LED_PIN 2
#define SERVO_PIN 12

const int NOT_CONNECTED_FLASH_INTERVAL = 500;
int lastFlashTime = 0;

int servoPos = 0;
int servoStep = 2; // Speed of rotation
unsigned long lastServoMove = 0;
const int SERVO_INTERVAL = 20; // ms between steps

#define SERVICE_UUID "DB594551-159C-4DA8-B59E-1C98587348E1"
#define CHARACTERISTIC_RX_UUID                                                 \
  "7B6B12CD-CA54-46A6-B3F4-3A848A3ED00B" // App writes commands here
#define CHARACTERISTIC_TX_UUID                                                 \
  "907BAC5D-92ED-4D90-905E-A3A7B9899F21" // notifies sensor data here

const char *deviceName = "ESP32_SCD30";
// Metadata to broadcast
const char *DEVICE_ID = "ESP32_01";
const char *DEVICE_ORG = "INGV";
const char *DEVICE_FW = "1.0";
// Use a real Bluetooth SIG Company Identifier when available.
const uint16_t COMPANY_ID = 0xB71E;

const std::string manufacturerText =
    std::string("ID=") + DEVICE_ID + ";ORG=" + DEVICE_ORG + ";FW=" + DEVICE_FW;
const std::string manufacturerData = []() {
  std::string data;
  data.reserve(manufacturerText.size() + 2);
  // Manufacturer Specific Data starts with 2-byte company ID, little-endian.
  data.push_back(static_cast<char>(COMPANY_ID & 0xFF));
  data.push_back(static_cast<char>((COMPANY_ID >> 8) & 0xFF));
  data += manufacturerText;
  return data;
}();

Adafruit_SCD30 scd30;
NimBLECharacteristic *pTxCharacteristic;
Servo servo;

bool deviceConnected = false;
bool acquire = false;

const bool DEBUG = true;

void sendCommand(String cmd, String payload) {
  if (!deviceConnected)
    return;

  pTxCharacteristic->setValue((cmd + ' ' + payload + '\n').c_str());
  pTxCharacteristic->notify();
  if (DEBUG)
    Serial.println("-> TX: " + cmd + " " + payload);
}
void commandHandler(String cmd, String payload) {
  if (cmd == "START_ACQUISITION") {
    acquire = true;
  } else if (cmd == "STOP_ACQUISITION") {
    acquire = false;
  } else if (cmd == "WHOIS") {
    sendCommand("WHOIS", String(manufacturerText.c_str()));
  } else if (cmd == "GET_ACQUISITION_STATE") {
    sendCommand("ACQUISITION_STATE", String(acquire));
  }
}

class ClientCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic,
               NimBLEConnInfo &connInfo) {
    std::string rxValue = pCharacteristic->getValue();
    String cmd = String(rxValue.c_str());
    cmd.trim(); // Remove whitespace
    if (DEBUG)
      Serial.println("<- RX: " + cmd);

    commandHandler(cmd, String(rxValue.c_str()));
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
    deviceConnected = true;
    digitalWrite(LED_PIN, LOW);
    Serial.println("Client Connected");

    pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 0, 400);
  };

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo,
                    int reason) {
    deviceConnected = false;
    Serial.print("Client Disconnected, reason: ");
    Serial.println(reason);

    NimBLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  servo.attach(SERVO_PIN);

  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SCD30 Found!");

  Serial.println("Starting BLE...");
  NimBLEDevice::init(deviceName);
  Serial.print("BLE Address: ");
  Serial.println(NimBLEDevice::getAddress().toString().c_str());

  // Disable encryption
  NimBLEDevice::setSecurityAuth(false, false, false);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::deleteAllBonds();

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  // NIMBLE_PROPERTY::READ_ENC / WRITE_ENC for encryption
  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_TX_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRxCharacteristic->setCallbacks(new ClientCallbacks());

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();

  pAdvertising->setManufacturerData(manufacturerData);
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->enableScanResponse(true);

  if (!pAdvertising->start()) {
    Serial.println("BLE Advertising FAILED to start");
  } else {
    Serial.println("BLE Advertising started. Waiting for App...");
  }
}

void loop() {
  if (!deviceConnected &&
      millis() - lastFlashTime > NOT_CONNECTED_FLASH_INTERVAL) {
    lastFlashTime = millis();
    digitalWrite(LED_PIN, HIGH);
  } else if (millis() - lastFlashTime > NOT_CONNECTED_FLASH_INTERVAL / 2) {
    digitalWrite(LED_PIN, LOW);
  }

  if (deviceConnected && acquire) {

    if (millis() - lastServoMove > SERVO_INTERVAL) {
      lastServoMove = millis();

      servoPos += servoStep;

      if (servoPos >= 180 || servoPos <= 0) {
        servoStep = -servoStep; // reverse direction
      }

      servo.write(servoPos);
    }

    if (scd30.dataReady() && scd30.read()) {
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

  delay(20);
}