#include <Arduino.h>
#include <Wire.h>

#include "motor.h"

uint8_t pcfState = 0xFF;

// -------------------- Работа с PCF8574 --------------------

bool pcfWriteState() {
    Wire.beginTransmission(PCF8574_ADDR);
    Wire.write(pcfState);
    uint8_t error = Wire.endTransmission();

    if (error != 0) {
        Serial.print("[PCF] write ERROR = ");
        Serial.println(error);
        return false;
    } else {
        Serial.print("[PCF] state written: 0b");
        Serial.println(pcfState, BIN);
        return true;
    }
}

void pcfSetPin(uint8_t pin, bool level) {
    if (pin > 7) {
        Serial.print("[PCF] invalid pin: ");
        Serial.println(pin);
        return;
    }

    if (level) {
        pcfState |= (1 << pin);
    } else {
        pcfState &= ~(1 << pin);
    }

    Serial.print("[PCF] pin ");
    Serial.print(pin);
    Serial.print(" <- ");
    Serial.print(level ? "HIGH" : "LOW");
    Serial.print(" | new state: 0b");
    Serial.println(pcfState, BIN);

    pcfWriteState();
}

// -------------------- Общий вывод на "пин направления" --------------------

static inline bool isPcfPin(uint8_t pin) {
    return pin <= 7;
}

// Для PCF: меняем бит и пишем по I2C
// Для GPIO: digitalWrite напрямую
static void writeDirPin(uint8_t pin, bool level) {
    if (isPcfPin(pin)) {
        pcfSetPin(pin, level);
        return;
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, level ? HIGH : LOW);

    Serial.printf("[GPIO] pin %u <- %s\n", pin, level ? "HIGH" : "LOW");
}

// -------------------- Управление моторами --------------------

const char* dirToStr(Direction d) {
    switch (d) {
        case DIR_STOP:     return "STOP";
        case DIR_FORWARD:  return "FORWARD";
        case DIR_BACKWARD: return "BACKWARD";
        case DIR_BRAKE:    return "BRAKE";
        default:           return "UNKNOWN";
    }
}

struct MotorCfg {
    const char* name;
    uint8_t in1;
    uint8_t in2;
    int pwmPin;
    int pwmCh;
};

static const MotorCfg cfgs[] = {
    // name     in1                 in2                 pwmPin       pwmCh
    { "MOTOR_1", DRIVER_1_PIN_AIN1, DRIVER_1_PIN_AIN2,  PWM_MOTOR_1, PWM_CH_M1 },
    { "MOTOR_2", DRIVER_1_PIN_BIN1, DRIVER_1_PIN_BIN2,  PWM_MOTOR_2, PWM_CH_M2 },
    { "MOTOR_3", DRIVER_2_PIN_AIN1, DRIVER_2_PIN_AIN2,  PWM_MOTOR_3, PWM_CH_M3 },
    { "MOTOR_4", DRIVER_2_PIN_BIN1, DRIVER_2_PIN_BIN2,  PWM_MOTOR_4, PWM_CH_M4 },
    { "MOTOR_5", DRIVER_3_PIN_AIN1, DRIVER_3_PIN_AIN2,  PWM_MOTOR_5, PWM_CH_M5 },
    { "MOTOR_6", DRIVER_3_PIN_BIN1, DRIVER_3_PIN_BIN2,  PWM_MOTOR_6, PWM_CH_M6 },
};

static const MotorCfg& getCfg(Motor m) {
    return cfgs[(int)m];
}

void motorPwmInit() {
    for (int i = 0; i < 6; i++) {
        ledcSetup(cfgs[i].pwmCh, PWM_FREQ, PWM_RES);
        ledcAttachPin(cfgs[i].pwmPin, cfgs[i].pwmCh);
        Serial.printf("[PWM] %-7s pin=%d channel=%d freq=%dHz res=%dbit\n",
                      cfgs[i].name, cfgs[i].pwmPin, cfgs[i].pwmCh, PWM_FREQ, PWM_RES);
    }
}

void setMotorDirection(Motor m, Direction dir) {
    const MotorCfg& c = getCfg(m);

    Serial.printf("[MOTOR %s] direction -> %s\n", c.name, dirToStr(dir));

    switch (dir) {
        case DIR_STOP:
            // IN1=1, IN2=1 (как у вас было)
            writeDirPin(c.in1, true);
            writeDirPin(c.in2, true);
            break;

        case DIR_FORWARD:
            // IN1=1, IN2=0
            writeDirPin(c.in1, true);
            writeDirPin(c.in2, false);
            break;

        case DIR_BACKWARD:
            // IN1=0, IN2=1
            writeDirPin(c.in1, false);
            writeDirPin(c.in2, true);
            break;

        case DIR_BRAKE:
            // IN1=0, IN2=0
            writeDirPin(c.in1, false);
            writeDirPin(c.in2, false);
            break;
    }
}

void setMotorSpeed(Motor m, uint8_t duty) {
    const MotorCfg& c = getCfg(m);
    Serial.printf("[%s] PWM = %u\n", c.name, duty);
    ledcWrite(c.pwmCh, duty);
}
