#include "i2c_bus.h"

#include <Arduino.h>
#include <Wire.h>

#include "debug_log.h"
#include "motor.h"

static bool i2cReady = false;

void initI2C() {
  if (i2cReady) return;

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setTimeOut(50);
  i2cReady = true;
  Debug.println("[I2C] bus started");
}
