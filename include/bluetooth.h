#pragma once

#include <Arduino.h>
#include <NimBLEConnInfo.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>

extern NimBLECharacteristic *pTxCharacteristic;
extern NimBLEServer *pServer;
extern bool deviceConnected;
extern uint16_t currentConnHandle;
extern const std::string manufacturerData;

class ClientCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo);
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo);
  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo,
                    int reason);
};

void setupBluetooth();