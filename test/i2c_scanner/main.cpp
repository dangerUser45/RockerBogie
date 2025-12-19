#include <Arduino.h>
#include <Wire.h>

const int PIN_I2C_SDA = 21;
const int PIN_I2C_SCL = 22;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nI2C scanner start");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
}

void loop() {
  Serial.println("Scanning...");

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at 0x");
      Serial.println(addr, HEX);
    }
  }

  Serial.println("Scan done.\n");
  delay(2000);
}
