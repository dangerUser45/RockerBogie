#include "web_server.h"

#include <Arduino_JSON.h>
#include <AsyncTCP.h>

#include "control_html.h"
#include "control_timer.h"
#include "debug_log.h"
#include "drive_control.h"
#include "network.h"
#include "servo_control.h"

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static AsyncWebSocket logWs("/log");

static void sendIndex(AsyncWebServerRequest *request) {
  request->send(200, "text/html", CONTROL_HTML);
}

void sendState(AsyncWebSocketClient *client) {
  JSONVar o;
  o["speed"] = (int)getTargetSpeed();
  o["appliedSpeed"] = (int)getAppliedSpeed();
  o["dir"] = (int)getDriveDirection();
  o["stbyEnabled"] = isStbyEnabled();
  o["wifiClients"] = (int)getStationCount();
  String s = JSON.stringify(o);
  if (client) client->text(s);
  else ws.textAll(s);
}

static void handleWsText(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)) return;

  String message;
  message.reserve(len + 1);
  for (size_t i = 0; i < len; i++) {
    message += (char)data[i];
  }

  JSONVar obj = JSON.parse(message);
  if (JSON.typeof(obj) == "undefined") return;

  noteCommandReceived();
  bool replyWithState = false;

  if (obj.hasOwnProperty("speed")) {
    setTargetSpeed((int)obj["speed"]);
    replyWithState = true;
  }

  if (obj.hasOwnProperty("stbyToggle")) {
    toggleStby();
    replyWithState = true;
  }

  if (obj.hasOwnProperty("joyThrottle") || obj.hasOwnProperty("joyTurn")) {
    const int throttle = obj.hasOwnProperty("joyThrottle") ? (int)obj["joyThrottle"] : 0;
    const int turn = obj.hasOwnProperty("joyTurn") ? (int)obj["joyTurn"] : 0;
    applyJoystickDrive(throttle, turn);
  }

  if (obj.hasOwnProperty("dir")) {
    startDrive((int)obj["dir"]);
  }

  if (obj.hasOwnProperty("servo") && obj.hasOwnProperty("angle")) {
    int i = (int)obj["servo"];
    int a = (int)obj["angle"];
    if (i >= 0 && i < 6) servoWriteAngle((uint8_t)i, (uint8_t)a);
  }

  if (obj.hasOwnProperty("servos")) {
    JSONVar arr = obj["servos"];
    if (arr.length() >= 6) {
      for (int i=0;i<6;i++) servoWriteAngle(i, (int)arr[i]);
    }
  }

  if (replyWithState) sendState();
}

static void onWsEvent(AsyncWebSocket *server,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg,
                      uint8_t *data,
                      size_t len) {
  (void)server;

  switch (type) {
    case WS_EVT_CONNECT:
      Debug.printf("[WS] client #%u connected\n", client->id());
      sendState(client);
      break;
    case WS_EVT_DISCONNECT:
      Debug.printf("[WS] client #%u disconnected\n", client->id());
      if (shouldStopOnWsDisconnect()) stopAll();
      break;
    case WS_EVT_DATA:
      handleWsText(arg, data, len);
      break;
    default:
      break;
  }
}

static void onLogWsEvent(AsyncWebSocket *server,
                         AsyncWebSocketClient *client,
                         AwsEventType type,
                         void *arg,
                         uint8_t *data,
                         size_t len) {
  (void)server;
  (void)arg;
  (void)data;
  (void)len;

  if (type == WS_EVT_CONNECT) {
    Debug.sendRecent(client);
  }
}

void initWebServer() {
  Debug.setWebSocket(&logWs);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  logWs.onEvent(onLogWsEvent);
  server.addHandler(&logWs);

  server.on("/", HTTP_GET, sendIndex);
  server.on("/generate_204", HTTP_GET, sendIndex);
  server.on("/gen_204", HTTP_GET, sendIndex);
  server.on("/hotspot-detect.html", HTTP_GET, sendIndex);
  server.on("/fwlink", HTTP_GET, sendIndex);
  server.on("/connecttest.txt", HTTP_GET, sendIndex);
  server.onNotFound(sendIndex);

  server.begin();
  Debug.println("[HTTP] server started");
}

void webServerLoop() {
  ws.cleanupClients();
  logWs.cleanupClients();
}
