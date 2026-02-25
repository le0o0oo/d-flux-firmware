#pragma once

#include <Adafruit_SCD30.h>
#include <Arduino.h>

extern Adafruit_SCD30 scd30;
extern bool acquire;

void setupSensor();
void readSensorData();
void calibrationWithReference(uint16_t ref);