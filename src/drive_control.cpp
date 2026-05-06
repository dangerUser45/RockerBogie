#include "drive_control.h"

#include "debug_log.h"
#include "i2c_bus.h"
#include "motor.h"

static const Motor LEFT_MOTORS[3]  = { MOTOR_1, MOTOR_3, MOTOR_5 };
static const Motor RIGHT_MOTORS[3] = { MOTOR_2, MOTOR_4, MOTOR_6 };

static uint8_t gTargetSpeed = 255;
static uint8_t gAppliedSpeed = 0;
static int gDir = 0; // 0 stop, 1 fwd, 2 back, 3 left, 4 right
static bool hardwareReady = false;
static bool stbyPinReady = false;
static bool stbyEnabled = true;

static const bool STOP_ON_WS_DISCONNECT = false;
static const bool STOP_ON_CMD_TIMEOUT = false;
static uint32_t lastCmdMs = 0;
static const uint32_t CMD_TIMEOUT_MS = 3000;
static const uint32_t ACCEL_RAMP_MS = 1200;
static uint32_t accelStartMs = 0;

static inline uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

static inline int clampS255(int v) {
  if (v < -255) return -255;
  if (v > 255) return 255;
  return v;
}

void initStbyPin() {
  if (stbyPinReady) return;

  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, stbyEnabled ? HIGH : LOW);
  stbyPinReady = true;
  Debug.printf("[STBY] GPIO%d -> %s\n", PIN_STBY, stbyEnabled ? "HIGH" : "LOW");
}

static bool initDriveHardware() {
  if (hardwareReady) return true;

  initI2C();
  initStbyPin();

  pcfWriteState();
  motorPwmInit();

  gAppliedSpeed = 0;
  for (int i=0;i<6;i++){
    setMotorSpeed((Motor)i, 0);
    setMotorDirection((Motor)i, DIR_STOP);
  }

  hardwareReady = true;
  Debug.println("[HW] drive hardware ready");
  return true;
}

void stopAll() {
  if (!hardwareReady) {
    gAppliedSpeed = 0;
    gDir = 0;
    return;
  }

  gAppliedSpeed = 0;
  for (int i=0;i<6;i++){
    setMotorSpeed((Motor)i, 0);
    setMotorDirection((Motor)i, DIR_STOP);
  }
  gDir = 0;
}

static void setStbyEnabled(bool enabled) {
  initStbyPin();

  if (!enabled) {
    stopAll();
  }

  stbyEnabled = enabled;
  digitalWrite(PIN_STBY, enabled ? HIGH : LOW);
  Debug.printf("[STBY] GPIO%d -> %s (%s)\n",
               PIN_STBY,
               enabled ? "HIGH" : "LOW",
               enabled ? "drivers enabled" : "drivers disabled");
}

void toggleStby() {
  setStbyEnabled(!stbyEnabled);
}

static void setAllDir(Direction d) {
  for (int i=0;i<6;i++) setMotorDirection((Motor)i, d);
}

static void setAllSpeed(uint8_t s) {
  for (int i=0;i<6;i++) setMotorSpeed((Motor)i, s);
}

static void applyDriveDirection() {
  if (gDir == 1) {
    setAllDir(DIR_FORWARD);
  } else if (gDir == 2) {
    setAllDir(DIR_BACKWARD);
  } else if (gDir == 3) {
    for (int i=0;i<3;i++){
      setMotorDirection(LEFT_MOTORS[i],  DIR_FORWARD);
      setMotorDirection(RIGHT_MOTORS[i], DIR_BACKWARD);
    }
  } else if (gDir == 4) {
    for (int i=0;i<3;i++){
      setMotorDirection(LEFT_MOTORS[i],  DIR_BACKWARD);
      setMotorDirection(RIGHT_MOTORS[i], DIR_FORWARD);
    }
  }
}

static void applyDriveSpeed() {
  setAllSpeed(gAppliedSpeed);
}

static void setMotorSigned(Motor motor, int signedSpeed) {
  signedSpeed = clampS255(signedSpeed);
  const uint8_t duty = (uint8_t)abs(signedSpeed);

  if (duty == 0) {
    setMotorSpeed(motor, 0);
    setMotorDirection(motor, DIR_STOP);
    return;
  }

  setMotorDirection(motor, signedSpeed > 0 ? DIR_FORWARD : DIR_BACKWARD);
  setMotorSpeed(motor, duty);
}

static void setSideSigned(const Motor motors[3], int signedSpeed) {
  for (int i=0;i<3;i++) setMotorSigned(motors[i], signedSpeed);
}

void applyJoystickDrive(int throttle, int turn) {
  if (!stbyEnabled) {
    stopAll();
    return;
  }

  if (!initDriveHardware()) return;

  throttle = clampS255(throttle);
  turn = clampS255(turn);

  if (abs(throttle) < 10) throttle = 0;
  if (abs(turn) < 10) turn = 0;

  throttle = throttle * (int)gTargetSpeed / 255;
  turn = turn * (int)gTargetSpeed / 255;

  const int leftSpeed = clampS255(throttle - turn);
  const int rightSpeed = clampS255(throttle + turn);

  gDir = 0;
  gAppliedSpeed = (uint8_t)max(abs(leftSpeed), abs(rightSpeed));

  if (leftSpeed == 0 && rightSpeed == 0) {
    stopAll();
    return;
  }

  setSideSigned(LEFT_MOTORS, leftSpeed);
  setSideSigned(RIGHT_MOTORS, rightSpeed);
}

static void driveApply() {
  if (!stbyEnabled) {
    stopAll();
    return;
  }

  if (!initDriveHardware()) {
    gDir = 0;
    return;
  }

  if (gDir == 0) {
    stopAll();
    return;
  }

  applyDriveDirection();
  applyDriveSpeed();
}

void startDrive(int dir) {
  if (!stbyEnabled) {
    stopAll();
    return;
  }

  if (!initDriveHardware()) {
    gDir = 0;
    return;
  }

  if (dir <= 0 || dir > 4) {
    stopAll();
    return;
  }

  if (dir != gDir) {
    gDir = dir;
    gAppliedSpeed = 0;
    accelStartMs = millis();
    driveApply();
  }
}

void updateAcceleration() {
  if (!hardwareReady || gDir == 0) return;

  const uint32_t elapsed = millis() - accelStartMs;
  uint8_t nextSpeed = gTargetSpeed;
  if (elapsed < ACCEL_RAMP_MS) {
    nextSpeed = (uint32_t)gTargetSpeed * elapsed / ACCEL_RAMP_MS;
  }

  if (nextSpeed != gAppliedSpeed) {
    gAppliedSpeed = nextSpeed;
    applyDriveSpeed();
  }
}

void setTargetSpeed(int speed) {
    gTargetSpeed = clampU8(speed);
    if (gAppliedSpeed > gTargetSpeed) {
        gAppliedSpeed = gTargetSpeed;
        if (gDir != 0) applyDriveSpeed();
    }
}

uint8_t getTargetSpeed()
{
    return gTargetSpeed;
}

uint8_t getAppliedSpeed() {
    return gAppliedSpeed;
}

int getDriveDirection() {
    return gDir;
}

bool isStbyEnabled() {
    return stbyEnabled;
}

void resetCommandTimer() {
    lastCmdMs = millis();
}

void noteCommandReceived() {
    lastCmdMs = millis();
}

bool shouldStopForCommandTimeout() {
    return STOP_ON_CMD_TIMEOUT && gDir != 0 && (millis() - lastCmdMs) > CMD_TIMEOUT_MS;
}

bool shouldStopOnWsDisconnect() {
  return STOP_ON_WS_DISCONNECT;
}
