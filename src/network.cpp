#include "network.h"

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFi.h>

#include "debug_log.h"
#include "drive_control.h"

static constexpr const char* AP_SSID = "RockerBogie";
static constexpr const char* AP_PASS = "12345678";
static constexpr const char* OTA_HOSTNAME = "RockerBogie";
static constexpr const char* OTA_PASS = "12345678";
static const IPAddress AP_IP(10, 0, 0, 1);
static const IPAddress AP_GATEWAY(10, 0, 0, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);
static constexpr uint8_t DNS_PORT = 53;

static constexpr uint8_t STANDARD_TELNET_PORT = 23;

static DNSServer dnsServer;
static uint8_t lastStationCount = 0;

void initWiFi()
{
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  const bool started = WiFi.softAP(AP_SSID, AP_PASS);
  Debug.print("[WIFI] AP ");
  Debug.println(started ? "started" : "start FAILED");
  Debug.print("[WIFI] SSID: ");
  Debug.println(AP_SSID);
  Debug.print("[WIFI] IP: ");
  Debug.println(WiFi.softAPIP());
  lastStationCount = WiFi.softAPgetStationNum();

  dnsServer.start(DNS_PORT, "*", AP_IP);
  Debug.println("[DNS] Captive portal DNS started");
  Debug.beginTelnet(STANDARD_TELNET_PORT);
  Debug.print("[LOG] connect with: telnet ");
  Debug.print(WiFi.softAPIP());
  Debug.printf(" %u\n", STANDARD_TELNET_PORT);
}

void initOTA()
{
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([]() {
    Debug.println("[OTA] start");
    stopAll();
  });
  ArduinoOTA.onEnd([]() {
    Debug.println();
    Debug.println("[OTA] end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int lastPercent = -1;
    const int percent = total == 0 ? 0 : (progress * 100 / total);
    if (percent != lastPercent && percent % 10 == 0) {
      lastPercent = percent;
      Debug.printf("[OTA] progress: %d%%\n", percent);
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Debug.printf("[OTA] error %u\n", error);
  });

  ArduinoOTA.begin();
  String apIp = WiFi.softAPIP().toString();
  Debug.printf("[OTA] ready: %s.local / %s\n", OTA_HOSTNAME, apIp.c_str());
}

void networkLoop() {
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
}

bool updateStationCount() {
  const uint8_t stationCount = WiFi.softAPgetStationNum();
  if (stationCount == lastStationCount) return false;

  lastStationCount = stationCount;
  Debug.printf("[TIMER] Wi-Fi clients: %u\n", lastStationCount);
  return true;
}

uint8_t getStationCount() {
  return lastStationCount;
}
