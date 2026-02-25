#include "servo.h"
#include "config.h"
#include "esp32-hal-ledc.h"

void startSpin() {
  ledcAttachPin(SERVO_PIN, pwmChannel);
  ledcWrite(pwmChannel, 4500);
}

void stopSpin() { 
  ledcDetachPin(SERVO_PIN); 
}
