#include <Arduino.h>

#include <esp_system.h>

#include "control_timer.h"
#include "debug_log.h"
#include "drive_control.h"
#include "network.h"
#include "web_server.h"

static const char* resetReasonToStr(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

void setup() {
  Debug.begin(115200);
  delay(200);

  Debug.println();
  Debug.println("=== RockerBogie WiFi ===");
  Debug.print("[RESET] reason: ");
  Debug.println(resetReasonToStr(esp_reset_reason()));
  initStbyPin();

  initWiFi();
  initOTA();
  initWebServer();
  initControlTimer();
  Debug.println("[BOOT] OTA and web are ready; hardware init is lazy");
  resetCommandTimer();
}

void loop() {
  networkLoop();
  Debug.loop();
  webServerLoop();

  if (processControlTimer()) {
    updateStationCount();
    sendState();
  }

  updateAcceleration();

  if (shouldStopForCommandTimeout()) {
    stopAll();
    Debug.println("[SAFETY] command timeout, stop");
  }
}
