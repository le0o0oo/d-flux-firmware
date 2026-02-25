#include "bluetooth.h"
#include "commands.h"
#include "config.h"

NimBLECharacteristic *pTxCharacteristic;
NimBLEServer *pServer = NULL;
bool deviceConnected = false;
uint16_t currentConnHandle = 0;

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

void ClientCallbacks::onWrite(NimBLECharacteristic *pCharacteristic,
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

void ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
  deviceConnected = true;
  digitalWrite(LED_PIN, LOW);
  currentConnHandle = connInfo.getConnHandle();
  Serial.println("Client Connected");

  pServer->updateConnParams(currentConnHandle, 12, 24, 0, 400);
}

void ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo,
                                  int reason) {
  deviceConnected = false;
  Serial.print("Client Disconnected, reason: ");
  Serial.println(reason);

  NimBLEDevice::startAdvertising();
}

void setupBluetooth() {
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
