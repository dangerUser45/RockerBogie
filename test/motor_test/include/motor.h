#pragma once

#include <Arduino.h>
#include <Wire.h>

// -------------------- Пины ESP32 -> TB6612FNG --------------------

// PWM для каждого мотора (6 моторов = 6 PWM линий)
const int PWM_MOTOR_1 = 25 /*13*/;   // D13
const int PWM_MOTOR_2 = 33 /*14*/;   // D14
const int PWM_MOTOR_3 = 27;   // D27
const int PWM_MOTOR_4 = 26;   // D26
const int PWM_MOTOR_5 = 13;   // D25
const int PWM_MOTOR_6 = 14;   // D33

const int PIN_STBY = 4;       // STBY (HIGH = драйверы включены)

// -------------------- PCF8574 --------------------

// Адрес PCF8574 (A0=A1=A2=0 → 0x20)
const uint8_t PCF8574_ADDR = 0x21;

// Соответствие линий PCF8574 -> TB6612 (две микросхемы TB6612 = 8 линий направлений)
const uint8_t DRIVER_1_PIN_AIN1 = 0; // P0
const uint8_t DRIVER_1_PIN_AIN2 = 1; // P1
const uint8_t DRIVER_1_PIN_BIN1 = 2; // P2
const uint8_t DRIVER_1_PIN_BIN2 = 3; // P3

const uint8_t DRIVER_2_PIN_AIN1 = 4; // P4
const uint8_t DRIVER_2_PIN_AIN2 = 5; // P5
const uint8_t DRIVER_2_PIN_BIN1 = 6; // P6
const uint8_t DRIVER_2_PIN_BIN2 = 7; // P7

/*
  DRIVER_3 направления — напрямую на ESP32 (временно или постоянно):
    AIN1 - GPIO16
    AIN2 - GPIO17
    BIN1 - GPIO18
    BIN2 - GPIO19
*/
const uint8_t DRIVER_3_PIN_AIN1 = 16;
const uint8_t DRIVER_3_PIN_AIN2 = 17;
const uint8_t DRIVER_3_PIN_BIN1 = 18;
const uint8_t DRIVER_3_PIN_BIN2 = 19;

// I2C пины ESP32
const int PIN_I2C_SDA = 21;
const int PIN_I2C_SCL = 22;

// Текущее состояние 8 линий PCF8574 (P0..P7)
// 1 = HIGH, 0 = LOW
extern uint8_t pcfState;

// -------------------- PWM / LEDC ------------------------

const int PWM_FREQ  = 20000; // 20 кГц
const int PWM_RES   = 8;     // 8 бит (0..255)

// LEDC каналы (по одному на мотор)
const int PWM_CH_M1 = 0;
const int PWM_CH_M2 = 1;
const int PWM_CH_M3 = 2;
const int PWM_CH_M4 = 3;
const int PWM_CH_M5 = 4;
const int PWM_CH_M6 = 5;

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
void pcfSetPin(uint8_t pin, bool level);

const char* dirToStr(Direction d);

void motorPwmInit();                 // настроить 6 PWM каналов
void setMotorDirection(Motor m, Direction dir);
void setMotorSpeed(Motor m, uint8_t duty);
