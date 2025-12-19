#include <Arduino.h>
#include <Wire.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

#include <Adafruit_PWMServoDriver.h>

#include "motor.h"

// ===================== WiFi =====================
// Вариант A: подключение к роутеру (STA)
static const char* WIFI_SSID = "Galaxy S22+";
static const char* WIFI_PASS = "00011122";

// Вариант B: точка доступа (AP) — удобнее для робота без роутера
static const bool USE_AP = false;
static const char* AP_SSID = "rockerbogie";
static const char* AP_PASS = "12345678"; // минимум 8 символов

// ===================== Web server =====================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ===================== PCA9685 (servos) =====================
Adafruit_PWMServoDriver pca9685(0x40);

static const uint16_t SERVO_FREQ = 50;       // 50Hz
static const uint16_t SERVO_MIN_US = 500;    // подстрой под свои сервы
static const uint16_t SERVO_MAX_US = 2500;

static uint8_t servoAngle[6] = {90, 90, 90, 90, 90, 90};

// ===================== Motors mapping =====================
// ВАЖНО: если у тебя физическая раскладка другая — поменяй тут.
// По умолчанию считаем: левый борт = 1,3,5; правый борт = 2,4,6
static const Motor LEFT_MOTORS[3]  = { MOTOR_1, MOTOR_3, MOTOR_5 };
static const Motor RIGHT_MOTORS[3] = { MOTOR_2, MOTOR_4, MOTOR_6 };

static uint8_t gSpeed = 0;   // 0..255
static int gDir = 0;         // 0 stop, 1 fwd, 2 back, 3 left, 4 right

// safety timeout: если не было команд долго — стоп
static uint32_t lastCmdMs = 0;
static const uint32_t CMD_TIMEOUT_MS = 1500;

// ===================== HTML =====================
// (упрощённый UI + 6 слайдеров серв)
static const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
  body { margin:0; font-family: sans-serif; }
  .wrap { display:grid; grid-template-columns: 1fr 1fr; gap:12px; padding:12px; }
  .card { border:1px solid #ddd; border-radius:12px; padding:12px; }
  .btns { display:grid; grid-template-columns: repeat(3, 1fr); gap:10px; }
  button { padding:18px 10px; font-size:18px; border-radius:12px; border:1px solid #bbb; background:#fff; }
  button:active { background:#eee; }
  .stop { border-color:#f55; }
  .row { display:flex; gap:10px; align-items:center; }
  input[type=range] { width:100%; }
  .small { color:#666; font-size:12px; }
  .servo { display:grid; grid-template-columns: 60px 1fr 50px; gap:10px; align-items:center; margin:8px 0; }
  @media (max-width: 800px){ .wrap { grid-template-columns: 1fr; } }
</style>

<div class="wrap">
  <div class="card">
    <h3>Drive</h3>
    <div class="btns">
      <div></div>
      <button id="fwd">F</button>
      <div></div>

      <button id="left">L</button>
      <button id="stop" class="stop">STOP</button>
      <button id="right">R</button>

      <div></div>
      <button id="back">B</button>
      <div></div>
    </div>

    <div style="margin-top:14px;">
      <div class="row">
        <div style="min-width:90px;">Speed:</div>
        <div id="spdTxt" style="font-size:22px;">0</div>
      </div>
      <input id="spd" type="range" min="0" max="255" value="0"/>
      <div class="small">Слайдер меняет скорость (0..255). Кнопки задают направление.</div>
    </div>
  </div>

  <div class="card">
    <h3>Servos (PCA9685)</h3>
    <div id="servos"></div>
    <div class="small">Отправка угла идёт при отпускании слайдера.</div>
  </div>
</div>

<script>
let gateway = `ws://${window.location.hostname}/ws`;
let ws;

const spd = document.querySelector("#spd");
const spdTxt = document.querySelector("#spdTxt");

function send(obj){
  if(ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
}

function dirButtons(){
  // удержание (pointerdown/pointerup) безопаснее
  const bindHold = (id, dir) => {
    const el = document.getElementById(id);
    el.addEventListener('pointerdown', () => send({dir}));
    el.addEventListener('pointerup',   () => send({dir:0}));
    el.addEventListener('pointercancel',() => send({dir:0}));
    el.addEventListener('mouseleave',  () => send({dir:0}));
  };
  bindHold("fwd", 1);
  bindHold("back",2);
  bindHold("left",3);
  bindHold("right",4);

  document.getElementById("stop").addEventListener('click', ()=> send({dir:0}));
}

function servoUI(){
  const root = document.getElementById("servos");
  for(let i=0;i<6;i++){
    const div = document.createElement("div");
    div.className="servo";
    div.innerHTML = `
      <div>S${i}:</div>
      <input type="range" min="0" max="180" value="90" id="sv${i}">
      <div id="svt${i}">90</div>
    `;
    root.appendChild(div);

    const s = div.querySelector(`#sv${i}`);
    const t = div.querySelector(`#svt${i}`);

    s.addEventListener('input', ()=> t.textContent = s.value);
    s.addEventListener('change', ()=> send({servo:i, angle: parseInt(s.value)}));
  }
}

function initWS(){
  ws = new WebSocket(gateway);
  ws.onopen = () => console.log("WS open");
  ws.onclose = () => { console.log("WS close"); setTimeout(initWS, 1000); };
  ws.onmessage = (e) => {
    try{
      const o = JSON.parse(e.data);
      if(o.speed !== undefined){
        spd.value = o.speed;
        spdTxt.textContent = o.speed;
      }
    }catch(_){}
  };
}

spd.addEventListener('input', ()=> spdTxt.textContent = spd.value);
spd.addEventListener('change', ()=> send({speed: parseInt(spd.value)}));

window.addEventListener('load', ()=>{
  dirButtons();
  servoUI();
  initWS();
});
</script>
</html>
)rawliteral";

// ===================== Helpers =====================
static inline uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

static inline uint8_t clampAngle(int a) {
  if (a < 0) return 0;
  if (a > 180) return 180;
  return (uint8_t)a;
}

static uint16_t usToTicks(uint16_t us) {
  // ticks = us * freq * 4096 / 1e6
  const uint32_t ticks = (uint32_t)us * (uint32_t)SERVO_FREQ * 4096UL / 1000000UL;
  return (uint16_t)ticks;
}

static void servoWriteAngle(uint8_t idx, uint8_t angle) {
  angle = clampAngle(angle);
  servoAngle[idx] = angle;

  const uint16_t us = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle / 180UL;
  const uint16_t ticks = usToTicks(us);

  pca9685.setPWM(idx, 0, ticks);
}

// ===================== Motors control =====================
static void stopAll() {
  for (int i=0;i<6;i++){
    setMotorSpeed((Motor)i, 0);
    setMotorDirection((Motor)i, DIR_STOP);
  }
  gDir = 0;
}

static void setAllDir(Direction d) {
  for (int i=0;i<6;i++) setMotorDirection((Motor)i, d);
}

static void setAllSpeed(uint8_t s) {
  for (int i=0;i<6;i++) setMotorSpeed((Motor)i, s);
}

static void driveApply() {
  // по текущему gDir и gSpeed
  if (gDir == 0) {
    stopAll();
    return;
  }

  if (gDir == 1) { // forward
    setAllDir(DIR_FORWARD);
    setAllSpeed(gSpeed);
  } else if (gDir == 2) { // back
    setAllDir(DIR_BACKWARD);
    setAllSpeed(gSpeed);
  } else if (gDir == 3) { // left (turn in place)
    for (int i=0;i<3;i++){
      setMotorDirection(LEFT_MOTORS[i],  DIR_BACKWARD);
      setMotorSpeed(LEFT_MOTORS[i], gSpeed);
      setMotorDirection(RIGHT_MOTORS[i], DIR_FORWARD);
      setMotorSpeed(RIGHT_MOTORS[i], gSpeed);
    }
  } else if (gDir == 4) { // right (turn in place)
    for (int i=0;i<3;i++){
      setMotorDirection(LEFT_MOTORS[i],  DIR_FORWARD);
      setMotorSpeed(LEFT_MOTORS[i], gSpeed);
      setMotorDirection(RIGHT_MOTORS[i], DIR_BACKWARD);
      setMotorSpeed(RIGHT_MOTORS[i], gSpeed);
    }
  }
}

// ===================== WiFi init =====================
static void initWiFi() {
  if (USE_AP) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("[WIFI] AP IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WIFI] Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(400);
    }
    Serial.println();
    Serial.print("[WIFI] STA IP: ");
    Serial.println(WiFi.localIP());
  }
}

// ===================== WebSocket handlers =====================
static void sendState(AsyncWebSocketClient *client=nullptr) {
  JSONVar o;
  o["speed"] = (int)gSpeed;
  o["dir"] = (int)gDir;
  String s = JSON.stringify(o);
  if (client) client->text(s);
  else ws.textAll(s);
}

static void handleWsText(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)) return;

  data[len] = 0; // сделаем C-string (буфер в Async обычно позволяет, но если боишься — копируй в String)
  JSONVar obj = JSON.parse((const char*)data);
  if (JSON.typeof(obj) == "undefined") return;

  lastCmdMs = millis();

  // speed
  if (obj.hasOwnProperty("speed")) {
    gSpeed = clampU8((int)obj["speed"]);
    // если сейчас едем — обновим скорость
    if (gDir != 0) driveApply();
  }

  // direction
  if (obj.hasOwnProperty("dir")) {
    gDir = (int)obj["dir"];
    driveApply();
  }

  // servo single: {servo: i, angle: a}
  if (obj.hasOwnProperty("servo") && obj.hasOwnProperty("angle")) {
    int i = (int)obj["servo"];
    int a = (int)obj["angle"];
    if (i >= 0 && i < 6) servoWriteAngle((uint8_t)i, (uint8_t)a);
  }

  // optional: set all servos: {servos:[90,90,90,90,90,90]}
  if (obj.hasOwnProperty("servos")) {
    JSONVar arr = obj["servos"];
    if (arr.length() >= 6) {
      for (int i=0;i<6;i++) servoWriteAngle(i, (int)arr[i]);
    }
  }

  sendState();
}

static void onWsEvent(AsyncWebSocket *server,
                      AsyncWebSocketClient *client,
                      AwsEventType type,
                      void *arg,
                      uint8_t *data,
                      size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] client #%u connected\n", client->id());
      sendState(client);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] client #%u disconnected\n", client->id());
      // safety: стоп при потере клиента (можно убрать если не надо)
      stopAll();
      break;
    case WS_EVT_DATA:
      handleWsText(arg, data, len);
      break;
    default:
      break;
  }
}

// ===================== Setup / loop =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("=== RockerBogie WiFi WS (6 motors + 6 servos) ===");

  // I2C
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  // PCF init (твои функции)
  pcfWriteState();

  // STBY enable
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);

  // PWM init for 6 motors (твоя функция)
  motorPwmInit();

  // stop everything on boot
  stopAll();

  // PCA9685 init
  pca9685.begin();
  pca9685.setPWMFreq(SERVO_FREQ);
  // выставим стартовые углы
  for (int i=0;i<6;i++) servoWriteAngle(i, servoAngle[i]);

  initWiFi();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  lastCmdMs = millis();
}

void loop() {
  // safety timeout
  if (gDir != 0 && (millis() - lastCmdMs) > CMD_TIMEOUT_MS) {
    stopAll();
  Serial.println("[alive] loop tick");
  }
}
