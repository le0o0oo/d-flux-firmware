#include "config.h"

// Device metadata definitions
const char *deviceName = "ESP32_SCD30";
const char *DEVICE_ID = "ESP32_01";
const char *DEVICE_ORG = "INGV";
const char *DEVICE_FW = "1.0";

// Settings keys definition
ConfigItem settingsKeys[] = {{"offset", 0.0}, {"multiplier", 1.0}};
const int settingsKeysCount = 2;
