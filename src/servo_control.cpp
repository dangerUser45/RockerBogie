#include "servo_control.h"

#include <Adafruit_PWMServoDriver.h>

#include "debug_log.h"
#include "i2c_bus.h"

static Adafruit_PWMServoDriver pca9685(0x40);

static const uint16_t SERVO_FREQ = 50;
static const uint16_t SERVO_MIN_US = 500;
static const uint16_t SERVO_MAX_US = 2500;

static uint8_t servoAngle[6] = {90, 90, 90, 90, 90, 90};
static bool servoReady = false;

static inline uint8_t clampAngle(int a) {
  if (a < 0) return 0;
  if (a > 180) return 180;
  return (uint8_t)a;
}

static uint16_t usToTicks(uint16_t us) {
  const uint32_t ticks = (uint32_t)us * (uint32_t)SERVO_FREQ * 4096UL / 1000000UL;
  return (uint16_t)ticks;
}

static bool initServoHardware() {
  if (servoReady) return true;

  initI2C();
  pca9685.begin();
  pca9685.setPWMFreq(SERVO_FREQ);
  servoReady = true;
  Debug.println("[PCA9685] servo controller started");
  return true;
}

void servoWriteAngle(uint8_t idx, uint8_t angle) {
  if (!initServoHardware()) return;

  angle = clampAngle(angle);
  servoAngle[idx] = angle;

  const uint16_t us = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle / 180UL;
  const uint16_t ticks = usToTicks(us);

  pca9685.setPWM(idx, 0, ticks);
}
