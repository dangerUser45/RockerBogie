#include <Arduino.h>
#include <Wire.h>

#include "debug_log.h"
#include "motor.h"

uint8_t pcfState = 0xFF;
uint8_t pcfStateRear = 0xFF;

// -------------------- Работа с PCF8574 --------------------

static uint8_t* pcfStateForAddr(uint8_t addr) {
    if (addr == PCF8574_ADDR_MAIN) return &pcfState;
    if (addr == PCF8574_ADDR_REAR) return &pcfStateRear;
    return nullptr;
}

bool pcfWriteState(uint8_t addr) {
    uint8_t* state = pcfStateForAddr(addr);
    if (state == nullptr) {
        Debug.printf("[PCF 0x%02X] unknown address\n", addr);
        return false;
    }

    Wire.beginTransmission(addr);
    Wire.write(*state);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Debug.printf("[PCF 0x%02X] write ERROR = %u\n", addr, error);
        return false;
    } else {
        Debug.printf("[PCF 0x%02X] state written: 0b", addr);
        Debug.println(*state, BIN);
        return true;
    }
}

bool pcfWriteState() {
    bool ok = pcfWriteState(PCF8574_ADDR_MAIN);
    ok = pcfWriteState(PCF8574_ADDR_REAR) && ok;
    return ok;
}

void pcfSetPin(uint8_t addr, uint8_t pin, bool level) {
    if (pin > 7) {
        Debug.printf("[PCF 0x%02X] invalid pin: %u\n", addr, pin);
        return;
    }

    uint8_t* state = pcfStateForAddr(addr);
    if (state == nullptr) {
        Debug.printf("[PCF 0x%02X] unknown address\n", addr);
        return;
    }

    if (level) {
        *state |= (1 << pin);
    } else {
        *state &= ~(1 << pin);
    }

    Debug.printf("[PCF 0x%02X] pin %u <- %s | new state: 0b",
                  addr, pin, level ? "HIGH" : "LOW");
    Debug.println(*state, BIN);

    pcfWriteState(addr);
}

void pcfSetPin(uint8_t pin, bool level) {
    pcfSetPin(PCF8574_ADDR_MAIN, pin, level);
}

// -------------------- Общий вывод на "пин направления" --------------------

static bool stagePcfPin(const DirPin& dirPin, bool level) {
    if (dirPin.pin > 7) {
        Debug.printf("[PCF 0x%02X] invalid pin: %u\n", dirPin.addr, dirPin.pin);
        return false;
    }

    uint8_t* state = pcfStateForAddr(dirPin.addr);
    if (state == nullptr) {
        Debug.printf("[PCF 0x%02X] unknown address\n", dirPin.addr);
        return false;
    }

    if (level) {
        *state |= (1 << dirPin.pin);
    } else {
        *state &= ~(1 << dirPin.pin);
    }

    Debug.printf("[PCF 0x%02X] pin %u <- %s\n",
                  dirPin.addr, dirPin.pin, level ? "HIGH" : "LOW");
    return true;
}

static void markPcfChanged(uint8_t addr, bool& mainChanged, bool& rearChanged) {
    if (addr == PCF8574_ADDR_MAIN) mainChanged = true;
    if (addr == PCF8574_ADDR_REAR) rearChanged = true;
}

// Направление мотора меняем атомарно: обе линии направления готовятся
// сначала, и только потом уходит одна запись в PCF8574.
static void writeDirPins(const DirPin& in1, bool in1Level, const DirPin& in2, bool in2Level) {
    bool mainChanged = false;
    bool rearChanged = false;

    if (stagePcfPin(in1, in1Level)) markPcfChanged(in1.addr, mainChanged, rearChanged);
    if (stagePcfPin(in2, in2Level)) markPcfChanged(in2.addr, mainChanged, rearChanged);

    if (mainChanged) {
        Debug.print("[PCF 0x20] new state: 0b");
        Debug.println(pcfState, BIN);
        pcfWriteState(PCF8574_ADDR_MAIN);
    }

    if (rearChanged) {
        Debug.print("[PCF 0x21] new state: 0b");
        Debug.println(pcfStateRear, BIN);
        pcfWriteState(PCF8574_ADDR_REAR);
    }
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
    DirPin in1;
    DirPin in2;
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
        Debug.printf("[PWM] %-7s pin=%d channel=%d freq=%dHz res=%dbit\n",
                      cfgs[i].name, cfgs[i].pwmPin, cfgs[i].pwmCh, PWM_FREQ, PWM_RES);
    }
}

void setMotorDirection(Motor m, Direction dir) {
    const MotorCfg& c = getCfg(m);

    Debug.printf("[MOTOR %s] direction -> %s\n", c.name, dirToStr(dir));

    switch (dir) {
        case DIR_STOP:
            // IN1=0, IN2=0
            writeDirPins(c.in1, false, c.in2, false);
            break;

        case DIR_FORWARD:
            // IN1=1, IN2=0
            writeDirPins(c.in1, true, c.in2, false);
            break;

        case DIR_BACKWARD:
            // IN1=0, IN2=1
            writeDirPins(c.in1, false, c.in2, true);
            break;

        case DIR_BRAKE:
            // IN1=1, IN2=1
            writeDirPins(c.in1, true, c.in2, true);
            break;
    }
}

void setMotorSpeed(Motor m, uint8_t duty) {
    const MotorCfg& c = getCfg(m);
    Debug.printf("[%s] PWM = %u\n", c.name, duty);
    ledcWrite(c.pwmCh, duty);
}
