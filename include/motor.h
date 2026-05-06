#pragma once

#include <Arduino.h>
#include <Wire.h>

// -------------------- Пины ESP32 -> TB6612FNG --------------------

// PWM для каждого мотора (6 моторов = 6 PWM линий)
constexpr int PWM_MOTOR_1 = 25;   // D25    || 1st driver
constexpr int PWM_MOTOR_2 = 33;   // D33    ||

constexpr int PWM_MOTOR_3 = 27;   // D27    || 2nd driver
constexpr int PWM_MOTOR_4 = 26;   // D26    ||

constexpr int PWM_MOTOR_5 = 13;   // D13    || 3rd driver
constexpr int PWM_MOTOR_6 = 14;   // D14    ||

constexpr int PIN_STBY = 4;       // STBY (HIGH = драйверы включены)

// -------------------- PCF8574 --------------------

// Адреса PCF8574
constexpr uint8_t PCF8574_ADDR_MAIN = 0x20;
constexpr uint8_t PCF8574_ADDR_REAR = 0x21;
constexpr uint8_t PCF8574_ADDR = PCF8574_ADDR_MAIN; // совместимость со старым кодом

struct DirPin {
    uint8_t addr;
    uint8_t pin;
};

// Соответствие линий PCF8574 -> TB6612
constexpr DirPin DRIVER_1_PIN_AIN1 = { PCF8574_ADDR_MAIN, 0 }; // 0x20 P0
constexpr DirPin DRIVER_1_PIN_AIN2 = { PCF8574_ADDR_MAIN, 1 }; // 0x20 P1
constexpr DirPin DRIVER_1_PIN_BIN1 = { PCF8574_ADDR_MAIN, 2 }; // 0x20 P2
constexpr DirPin DRIVER_1_PIN_BIN2 = { PCF8574_ADDR_MAIN, 3 }; // 0x20 P3

constexpr DirPin DRIVER_2_PIN_AIN1 = { PCF8574_ADDR_MAIN, 4 }; // 0x20 P4
constexpr DirPin DRIVER_2_PIN_AIN2 = { PCF8574_ADDR_MAIN, 5 }; // 0x20 P5
constexpr DirPin DRIVER_2_PIN_BIN1 = { PCF8574_ADDR_MAIN, 6 }; // 0x20 P6
constexpr DirPin DRIVER_2_PIN_BIN2 = { PCF8574_ADDR_MAIN, 7 }; // 0x20 P7

constexpr DirPin DRIVER_3_PIN_AIN1 = { PCF8574_ADDR_REAR, 0 }; // 0x21 P0
constexpr DirPin DRIVER_3_PIN_AIN2 = { PCF8574_ADDR_REAR, 1 }; // 0x21 P1
constexpr DirPin DRIVER_3_PIN_BIN1 = { PCF8574_ADDR_REAR, 2 }; // 0x21 P2
constexpr DirPin DRIVER_3_PIN_BIN2 = { PCF8574_ADDR_REAR, 3 }; // 0x21 P3

// I2C пины ESP32
constexpr int PIN_I2C_SDA = 21;
constexpr int PIN_I2C_SCL = 22;

// Текущее состояние линий PCF8574 (P0..P7)
// 1 = HIGH, 0 = LOW
extern uint8_t pcfState;
extern uint8_t pcfStateRear;

// -------------------- PWM / LEDC ------------------------

constexpr int PWM_FREQ  = 20000; // 20 кГц
constexpr int PWM_RES   = 8;     // 8 бит (0..255)

// LEDC каналы (по одному на мотор)
constexpr int PWM_CH_M1 = 0;
constexpr int PWM_CH_M2 = 1;
constexpr int PWM_CH_M3 = 2;
constexpr int PWM_CH_M4 = 3;
constexpr int PWM_CH_M5 = 4;
constexpr int PWM_CH_M6 = 5;

// -------------------- Управление моторами --------------------

enum Motor {
    MOTOR_1,
    MOTOR_2,
    MOTOR_3,
    MOTOR_4,
    MOTOR_5,
    MOTOR_6
};

enum Direction {
    DIR_STOP,
    DIR_FORWARD,
    DIR_BACKWARD,
    DIR_BRAKE
};

bool pcfWriteState();
bool pcfWriteState(uint8_t addr);
void pcfSetPin(uint8_t pin, bool level);
void pcfSetPin(uint8_t addr, uint8_t pin, bool level);

const char* dirToStr(Direction d);

void motorPwmInit();                 // настроить 6 PWM каналов
void setMotorDirection(Motor m, Direction dir);
void setMotorSpeed(Motor m, uint8_t duty);
