#include <Arduino.h>
#include <esp32-hal-ledc.h> 
#include "chassis.h"


void initChassis() {
  
  ledcSetup(PWM_CHANNEL_LEFT_1, PWM_MOTOR_FREQUENCY, PWM_MOTOR_RESOLUTION);
  ledcSetup(PWM_CHANNEL_LEFT_2, PWM_MOTOR_FREQUENCY, PWM_MOTOR_RESOLUTION);
  ledcSetup(PWM_CHANNEL_RIGHT_1, PWM_MOTOR_FREQUENCY, PWM_MOTOR_RESOLUTION);
  ledcSetup(PWM_CHANNEL_RIGHT_2, PWM_MOTOR_FREQUENCY, PWM_MOTOR_RESOLUTION);

  ledcAttachPin(PIN_LEFT_1, PWM_CHANNEL_LEFT_1);
  ledcAttachPin(PIN_LEFT_2, PWM_CHANNEL_LEFT_2);
  ledcAttachPin(PIN_RIGHT_1, PWM_CHANNEL_RIGHT_1);
  ledcAttachPin(PIN_RIGHT_2, PWM_CHANNEL_RIGHT_2);
}


void setLeftMotor(int16_t speed) {
  speed = -speed;
  if (speed > 0) {
    ledcWrite(PWM_CHANNEL_LEFT_1,0);
    ledcWrite(PWM_CHANNEL_LEFT_2,abs(speed));
  } else if (speed < 0) {
    ledcWrite(PWM_CHANNEL_LEFT_1,abs(speed));
    ledcWrite(PWM_CHANNEL_LEFT_2,0);
  } else {
    ledcWrite(PWM_CHANNEL_LEFT_1,0);
    ledcWrite(PWM_CHANNEL_LEFT_2,0);
  }
}


void setRightMotor(int16_t speed) {
  if (speed > 0) {
    ledcWrite(PWM_CHANNEL_RIGHT_1,0);
    ledcWrite(PWM_CHANNEL_RIGHT_2,abs(speed));
  } else if (speed < 0) {
    ledcWrite(PWM_CHANNEL_RIGHT_1,abs(speed));
    ledcWrite(PWM_CHANNEL_RIGHT_2,0);
  } else {
    ledcWrite(PWM_CHANNEL_RIGHT_1,0);
    ledcWrite(PWM_CHANNEL_RIGHT_2,0);
  }
}

void setLedBrightness(int16_t brightness) {
  
}
