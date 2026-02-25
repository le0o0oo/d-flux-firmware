#pragma once

#include <Arduino.h>
#include <NimBLECharacteristic.h>
#include <NimBLEServer.h>
#include <Preferences.h>

// External variables
extern bool deviceConnected;
extern bool acquire;
extern Preferences preferences;
extern NimBLECharacteristic *pTxCharacteristic;
extern NimBLEServer *pServer;
extern uint16_t currentConnHandle;
extern const std::string manufacturerText;

// Function declarations
void sendCommand(String cmd, String payload);
void settingsParser(String payload, int mode);
void commandHandler(String cmd, String payload);