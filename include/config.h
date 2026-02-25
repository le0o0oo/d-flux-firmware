#pragma once

#include <Arduino.h>

// Pin definitions
#define LED_PIN 2
#define SERVO_PIN 12

// BLE UUIDs
#define SERVICE_UUID "DB594551-159C-4DA8-B59E-1C98587348E1"
#define CHARACTERISTIC_RX_UUID                                                 \
  "7B6B12CD-CA54-46A6-B3F4-3A848A3ED00B" // App writes commands here
#define CHARACTERISTIC_TX_UUID                                                 \
  "907BAC5D-92ED-4D90-905E-A3A7B9899F21" // notifies sensor data here

// Settings modes
#define SETTING_MODE_READ 0
#define SETTING_MODE_WRITE 1

// Timing constants
const int NOT_CONNECTED_FLASH_INTERVAL = 500;

// Servo PWM configuration
const int pwmChannel = 0;
const int pwmFreq = 50;       // 50 Hz for servo
const int pwmResolution = 16; // 16-bit resolution

// Device metadata (see config.cpp to change)
extern const char *deviceName;
extern const char *DEVICE_ID;
extern const char *DEVICE_ORG;
extern const char *DEVICE_FW;

// BLE manufacturer data
const uint16_t COMPANY_ID = 0xB71E;

// Debug flag
const bool DEBUG = true;

// Configuration structure
struct ConfigItem {
  const char *key;
  float defaultValue;
};

// Settings keys
extern ConfigItem settingsKeys[2];
extern const int settingsKeysCount;