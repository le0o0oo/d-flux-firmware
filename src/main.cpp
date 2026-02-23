#include "NimBLEClient.h"
#include "NimBLEServer.h"
#include "esp32-hal-ledc.h"
#include <Adafruit_SCD30.h>
#include <Arduino.h>
#include <NimBLEConnInfo.h>
#include <NimBLEDevice.h>

#define LED_PIN 2
#define SERVO_PIN 12

#define SERVICE_UUID "DB594551-159C-4DA8-B59E-1C98587348E1"
#define CHARACTERISTIC_RX_UUID                                                 \
  "7B6B12CD-CA54-46A6-B3F4-3A848A3ED00B" // App writes commands here
#define CHARACTERISTIC_TX_UUID                                                 \
  "907BAC5D-92ED-4D90-905E-A3A7B9899F21" // notifies sensor data here

const int NOT_CONNECTED_FLASH_INTERVAL = 500;
int lastFlashTime = 0;

// Servo
const int pwmChannel = 0;
const int pwmFreq = 50;       // 50 Hz for servo
const int pwmResolution = 16; // 16-bit resolution

uint16_t currentConnHandle = 0;

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

NimBLEServer *pServer = NULL;

bool deviceConnected = false;
bool acquire = false;

const bool DEBUG = true;

void startSpin() {
  ledcAttachPin(SERVO_PIN, pwmChannel);
  ledcWrite(pwmChannel, 4500);
}
void stopSpin() { ledcDetachPin(SERVO_PIN); }
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
  }
}
class ClientCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic,
               NimBLEConnInfo &connInfo) {
    std::string rxValue = pCharacteristic->getValue();
    String value = String(rxValue.c_str());
    value.trim(); // Remove whitespace
    if (DEBUG)
      Serial.println("<- RX: " + value);

    int separator = value.indexOf(' ');
    String cmd, payload;

    if (separator != -1) {
      cmd = value.substring(0, separator);
      payload = value.substring(separator + 1);
    } else {
      cmd = value;
    }

    commandHandler(cmd, payload);
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
    deviceConnected = true;
    digitalWrite(LED_PIN, LOW);
    currentConnHandle = connInfo.getConnHandle();
    Serial.println("Client Connected");

    pServer->updateConnParams(currentConnHandle, 12, 24, 0, 400);
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
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);

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

  pServer = NimBLEDevice::createServer();
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

  delay(20);
}