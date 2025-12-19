#include <Arduino.h>
#include <Wire.h>

#include "motor.h"

// -------------------- SETUP --------------------

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=== 3x TB6612FNG + PCF8574 (6 motors) simple test ===");

    // I2C
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // Проверим PCF8574
    Wire.beginTransmission(PCF8574_ADDR);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        Serial.print("[INFO] PCF8574 found at 0x");
        Serial.println(PCF8574_ADDR, HEX);
    } else {
        Serial.print("[ERROR] PCF8574 NOT FOUND at 0x");
        Serial.print(PCF8574_ADDR, HEX);
        Serial.print(", error = ");
        Serial.println(error);
        Serial.println("Проверь питание, SDA/SCL и джамперы A0-A2!");
    }

    // Инициализация PCF8574: все линии HIGH
    Serial.print("[INIT] pcfState init: 0b");
    Serial.println(pcfState, BIN);
    pcfWriteState();

    // STBY — включаем драйверы
    pinMode(PIN_STBY, OUTPUT);
    digitalWrite(PIN_STBY, HIGH);
    Serial.printf("[INFO] STBY pin %d set HIGH (drivers enabled)\n", PIN_STBY);

    // PWM на 6 моторов
    motorPwmInit();

    // На старте: всё выключено
    for (int i = 0; i < 6; i++) {
        setMotorSpeed((Motor)i, 0);
        setMotorDirection((Motor)i, DIR_STOP);
    }

    Serial.println("[INIT] Setup done. Starting loop...");
}

// -------------------- LOOP --------------------

static void setAllDir(Direction d) {
    for (int i = 0; i < 6; i++) setMotorDirection((Motor)i, d);
}

static void setAllSpeed(uint8_t s) {
    for (int i = 0; i < 6; i++) setMotorSpeed((Motor)i, s);
}

void loop()
{
  const uint32_t WAITING_TIME = 500;
  const uint8_t SPEED = 0;
  for(int i = 0; i < 6; ++i) {
    setMotorSpeed((Motor)i, SPEED);

    setMotorDirection((Motor)i, DIR_FORWARD);
    delay(WAITING_TIME);

    setMotorDirection((Motor)i, DIR_BRAKE);
    delay(WAITING_TIME);

    setMotorDirection((Motor)i, DIR_BACKWARD);
    delay(WAITING_TIME);

    setMotorDirection((Motor)i, DIR_BRAKE);
    delay(WAITING_TIME);
  }
  // setMotorSpeed(MOTOR_3, 0);
  // // тест направления через P6/P7
  // for(int i = 0; i < 7; ++i) pcfSetPin(i, false);
  // delay(5000);
}
