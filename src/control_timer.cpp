#include "control_timer.h"

#include "debug_log.h"

static const uint32_t CONTROL_TIMER_PERIOD_US = 1000000; // 1 Hz

static hw_timer_t* controlTimer = nullptr;
static portMUX_TYPE controlTimerMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool controlTimerPending = false;

static void IRAM_ATTR onControlTimerInterrupt() {
  portENTER_CRITICAL_ISR(&controlTimerMux);
  controlTimerPending = true;
  portEXIT_CRITICAL_ISR(&controlTimerMux);
}

static bool takeControlTimerTick() {
  bool pending;

  portENTER_CRITICAL(&controlTimerMux);
  pending = controlTimerPending;
  controlTimerPending = false;
  portEXIT_CRITICAL(&controlTimerMux);

  return pending;
}

void initControlTimer() {
  if (controlTimer) return;

  controlTimer = timerBegin(1, 80, true); // 80 MHz / 80 = 1 MHz
  timerAttachInterrupt(controlTimer, &onControlTimerInterrupt, true);
  timerAlarmWrite(controlTimer, CONTROL_TIMER_PERIOD_US, true);
  timerAlarmEnable(controlTimer);
  Debug.printf("[TIMER] control timer started: %lu us\n",
               (unsigned long)CONTROL_TIMER_PERIOD_US);
}

bool processControlTimer() {
  return takeControlTimerTick();
}
