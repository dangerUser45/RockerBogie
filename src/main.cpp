#include <Arduino.h>
#include <Wire.h>

#include <WiFi.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Arduino_JSON.h>

#include <Adafruit_PWMServoDriver.h>

#include <esp_system.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "debug_log.h"
#include "motor.h"

// ===================== WiFi =====================
// ESP32 сама создает Wi-Fi сеть. Подключайся к ней с ноутбука.
static const char* AP_SSID = "RockerBogie";
static const char* AP_PASS = "12345678"; // минимум 8 символов
static const char* OTA_HOSTNAME = "RockerBogie";
static const char* OTA_PASS = "12345678";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);
static const uint8_t DNS_PORT = 53;

// ===================== Web server =====================
DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocket logWs("/log");

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

static uint8_t gTargetSpeed = 255;  // максимум PWM, к которому разгоняемся
static uint8_t gAppliedSpeed = 0;   // текущий PWM на моторах
static int gDir = 0;         // 0 stop, 1 fwd, 2 back, 3 left, 4 right
static bool hardwareReady = false;
static bool i2cReady = false;
static bool servoReady = false;
static bool stbyPinReady = false;
static bool stbyEnabled = true;
static uint32_t lastApCheckMs = 0;

// Программные автостопы отключены: на резком старте 255 краткая просадка
// или reconnect WebSocket не должны сами гасить моторы.
static const bool STOP_ON_WS_DISCONNECT = false;
static const bool STOP_ON_CMD_TIMEOUT = false;
static uint32_t lastCmdMs = 0;
static const uint32_t CMD_TIMEOUT_MS = 3000;
static const uint32_t ACCEL_RAMP_MS = 1200;
static uint32_t accelStartMs = 0;

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
  .stop { border-color:#f55; color:#b00000; }
  .stop.disabled { background:#b00000; border-color:#b00000; color:#fff; }
  .row { display:flex; gap:10px; align-items:center; }
  input[type=range] { width:100%; }
  .small { color:#666; font-size:12px; }
  .servo { display:grid; grid-template-columns: 60px 1fr 50px; gap:10px; align-items:center; margin:8px 0; }
  .joysticks { display:grid; grid-template-columns: 1fr 1fr; gap:14px; }
  .stickBox { display:grid; gap:8px; justify-items:center; }
  .stickLabel { color:#333; font-size:13px; }
  .stick {
    width:min(42vw, 170px);
    height:min(42vw, 170px);
    max-width:170px;
    max-height:170px;
    min-width:130px;
    min-height:130px;
    position:relative;
    border-radius:50%;
    border:2px solid #bbb;
    background:radial-gradient(circle at center, #fafafa 0 34%, #eee 35% 100%);
    touch-action:none;
    user-select:none;
  }
  .knob {
    width:42%;
    height:42%;
    position:absolute;
    left:50%;
    top:50%;
    border-radius:50%;
    transform:translate(-50%, -50%);
    background:#333;
    box-shadow:0 3px 10px rgba(0,0,0,.25);
    pointer-events:none;
  }
  .logbox { height:240px; overflow:auto; margin:0; padding:10px; background:#111; color:#ddd; font:12px/1.35 monospace; border-radius:8px; white-space:pre-wrap; }
  @media (max-width: 800px){
    .wrap { grid-template-columns: 1fr; }
    .joysticks { grid-template-columns: 1fr 1fr; }
  }
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
        <div id="spdTxt" style="font-size:22px;">255</div>
      </div>
      <input id="spd" type="range" min="0" max="255" value="255"/>
      <div class="small">Max speed (0..255)</div>
    </div>
  </div>

  <div class="card">
    <h3>Joysticks</h3>
    <div class="joysticks">
      <div class="stickBox">
        <div id="leftStick" class="stick"><div class="knob"></div></div>
        <div class="stickLabel">Forward / Back</div>
      </div>
      <div class="stickBox">
        <div id="rightStick" class="stick"><div class="knob"></div></div>
        <div class="stickLabel">Left / Right</div>
      </div>
    </div>
  </div>

  <div class="card">
    <h3>Servos (PCA9685)</h3>
    <div id="servos"></div>
    <div class="small">The angle is sent when the slider is released.</div>
  </div>

  <div class="card">
    <h3>Log</h3>
    <pre id="log" class="logbox"></pre>
  </div>
</div>

<script>
let gateway = `ws://${window.location.hostname}/ws`;
let logGateway = `ws://${window.location.hostname}/log`;
let ws;
let logWs;

const spd = document.querySelector("#spd");
const spdTxt = document.querySelector("#spdTxt");
const stopBtn = document.querySelector("#stop");
const logEl = document.querySelector("#log");
const keyDirs = new Map([
  ["w", 1],
  ["s", 2],
  ["a", 4],
  ["d", 3],
]);
const keyCodeDirs = new Map([
  ["KeyW", 1],
  ["KeyS", 2],
  ["KeyA", 4],
  ["KeyD", 3],
]);
const pressedKeys = new Map();
let activeDir = 0;
let holdTimer = null;
const joystick = {
  throttle: 0,
  turn: 0,
  leftActive: false,
  rightActive: false,
  leftPointer: null,
  rightPointer: null,
  zeroRepeats: 0,
  timer: null,
  resetters: [],
};

function send(obj){
  if(ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
}

function dirFromKeyEvent(e){
  if(keyCodeDirs.has(e.code)) return keyCodeDirs.get(e.code);
  const key = e.key.toLowerCase();
  return keyDirs.has(key) ? keyDirs.get(key) : 0;
}

function ensureHoldTimer(){
  if(holdTimer) return;
  holdTimer = setInterval(() => {
    if(activeDir === 0){
      clearInterval(holdTimer);
      holdTimer = null;
      return;
    }
    send({dir: activeDir});
  }, 250);
}

function setDriveDir(dir){
  if(activeDir === dir) return;
  activeDir = dir;
  send({dir});
  if(dir !== 0) ensureHoldTimer();
}

function cancelHoldDrive(){
  pressedKeys.clear();
  activeDir = 0;
  if(holdTimer){
    clearInterval(holdTimer);
    holdTimer = null;
  }
}

function stopDrive(){
  cancelHoldDrive();
  send({dir:0});
}

function lastPressedDir(){
  let dir = 0;
  for(const value of pressedKeys.values()) dir = value;
  return dir;
}

function dirButtons(){
  const bindHold = (id, dir) => {
    const el = document.getElementById(id);
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      el.setPointerCapture?.(e.pointerId);
      setDriveDir(dir);
    });
    el.addEventListener('pointerup', stopDrive);
    el.addEventListener('pointercancel', stopDrive);
    el.addEventListener('lostpointercapture', stopDrive);
  };
  bindHold("fwd", 1);
  bindHold("back",2);
  bindHold("left",4);
  bindHold("right",3);

  stopBtn.addEventListener('click', () => {
    cancelHoldDrive();
    sendJoystickStop();
    send({stbyToggle:1});
  });
}

function keyboardDrive(){
  document.addEventListener('keydown', (e) => {
    const dir = dirFromKeyEvent(e);
    if(!dir) return;
    e.preventDefault();
    e.stopPropagation();
    if(!pressedKeys.has(e.code)) pressedKeys.set(e.code, dir);
    setDriveDir(dir);
  }, true);

  document.addEventListener('keyup', (e) => {
    const dir = dirFromKeyEvent(e);
    if(!dir) return;
    e.preventDefault();
    e.stopPropagation();
    pressedKeys.delete(e.code);
    setDriveDir(lastPressedDir());
  }, true);

  window.addEventListener('blur', stopDrive);
  document.addEventListener('visibilitychange', () => {
    if(document.hidden) stopDrive();
  });
}

function clamp(v, min, max){
  return Math.max(min, Math.min(max, v));
}

function ensureJoystickTimer(){
  if(joystick.timer) return;
  joystick.timer = setInterval(() => {
    if(!joystick.leftActive && !joystick.rightActive && joystick.throttle === 0 && joystick.turn === 0 && joystick.zeroRepeats > 0){
      sendJoystick();
      joystick.zeroRepeats--;
      return;
    }
    if(!joystick.leftActive && !joystick.rightActive && joystick.throttle === 0 && joystick.turn === 0){
      clearInterval(joystick.timer);
      joystick.timer = null;
      return;
    }
    sendJoystick();
  }, 80);
}

function sendJoystick(){
  send({joyThrottle: joystick.throttle, joyTurn: joystick.turn});
}

function sendJoystickStop(){
  joystick.throttle = 0;
  joystick.turn = 0;
  joystick.zeroRepeats = 8;
  sendJoystick();
  ensureJoystickTimer();
}

function setupJoystick(id, axis){
  const el = document.getElementById(id);
  const knob = el.querySelector(".knob");
  const activeKey = axis === "y" ? "leftActive" : "rightActive";
  const pointerKey = axis === "y" ? "leftPointer" : "rightPointer";

  const setKnob = (x, y) => {
    knob.style.transform = `translate(calc(-50% + ${x}px), calc(-50% + ${y}px))`;
  };

  const update = (e) => {
    if(joystick[pointerKey] !== null && e.pointerId !== joystick[pointerKey]) return;

    const rect = el.getBoundingClientRect();
    const radius = Math.min(rect.width, rect.height) * 0.38;
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    let dx = e.clientX - cx;
    let dy = e.clientY - cy;
    const length = Math.hypot(dx, dy);

    if(length > radius){
      dx = dx / length * radius;
      dy = dy / length * radius;
    }

    if(axis === "y"){
      dx = 0;
      joystick.throttle = Math.round(clamp(-dy / radius, -1, 1) * 255);
    }else{
      dy = 0;
      joystick.turn = Math.round(clamp(dx / radius, -1, 1) * 255);
    }

    setKnob(dx, dy);
    sendJoystick();
  };

  const reset = (e) => {
    if(joystick[pointerKey] === null && !joystick[activeKey]) return;
    if(e && joystick[pointerKey] !== null && e.pointerId !== joystick[pointerKey]) return;

    joystick[activeKey] = false;
    joystick[pointerKey] = null;
    if(axis === "y") joystick.throttle = 0;
    else joystick.turn = 0;
    setKnob(0, 0);
    if(!joystick.leftActive && !joystick.rightActive) joystick.zeroRepeats = 8;
    sendJoystick();
    ensureJoystickTimer();
  };

  el.addEventListener("pointerdown", (e) => {
    e.preventDefault();
    cancelHoldDrive();
    el.setPointerCapture?.(e.pointerId);
    joystick[activeKey] = true;
    joystick[pointerKey] = e.pointerId;
    joystick.zeroRepeats = 0;
    update(e);
    ensureJoystickTimer();
  });
  el.addEventListener("pointermove", (e) => {
    if(joystick[activeKey]) update(e);
  });
  el.addEventListener("pointerup", reset);
  el.addEventListener("pointercancel", reset);
  el.addEventListener("lostpointercapture", reset);

  document.addEventListener("pointerup", reset, true);
  document.addEventListener("pointercancel", reset, true);
  joystick.resetters.push(reset);
}

function joystickUI(){
  setupJoystick("leftStick", "y");
  setupJoystick("rightStick", "x");
  window.addEventListener("blur", () => {
    for(const reset of joystick.resetters) reset();
    sendJoystickStop();
  });
}

function updateStbyButton(enabled){
  stopBtn.classList.toggle("disabled", !enabled);
  stopBtn.textContent = enabled ? "STOP" : "ENABLE";
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
  ws.onopen = () => {
    console.log("WS open");
    if(activeDir !== 0) send({dir: activeDir});
  };
  ws.onclose = () => { console.log("WS close"); setTimeout(initWS, 1000); };
  ws.onmessage = (e) => {
    try{
      const o = JSON.parse(e.data);
      if(o.speed !== undefined){
        spd.value = o.speed;
        spdTxt.textContent = o.speed;
      }
      if(o.stbyEnabled !== undefined){
        updateStbyButton(!!o.stbyEnabled);
      }
    }catch(_){}
  };
}

function appendLog(text){
  logEl.textContent += text;
  if(logEl.textContent.length > 12000){
    logEl.textContent = logEl.textContent.slice(-12000);
  }
  logEl.scrollTop = logEl.scrollHeight;
}

function initLogWS(){
  logWs = new WebSocket(logGateway);
  logWs.onmessage = (e) => appendLog(e.data);
  logWs.onclose = () => setTimeout(initLogWS, 1000);
}

spd.addEventListener('input', ()=> spdTxt.textContent = spd.value);
spd.addEventListener('change', ()=> send({speed: parseInt(spd.value)}));

window.addEventListener('load', ()=>{
  dirButtons();
  keyboardDrive();
  joystickUI();
  servoUI();
  initWS();
  initLogWS();
});
</script>
</html>
)rawliteral";

static void sendIndex(AsyncWebServerRequest *request) {
  request->send(200, "text/html", index_html);
}

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

static void disableBrownoutDetector() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}

// ===================== Helpers =====================
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

static void initI2C() {
  if (i2cReady) return;

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setTimeOut(50);
  i2cReady = true;
  Debug.println("[I2C] bus started");
}

static bool initServoHardware() {
  if (servoReady) return true;

  initI2C();
  pca9685.begin();
  pca9685.setPWMFreq(SERVO_FREQ);
  servoReady = true;
  Debug.println("[PCA9685] servo controller started");
  return true;
}

static void servoWriteAngle(uint8_t idx, uint8_t angle) {
  if (!initServoHardware()) return;

  angle = clampAngle(angle);
  servoAngle[idx] = angle;

  const uint16_t us = SERVO_MIN_US + (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle / 180UL;
  const uint16_t ticks = usToTicks(us);

  pca9685.setPWM(idx, 0, ticks);
}

// ===================== Motors control =====================
static void initStbyPin() {
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

  // PCF init
  pcfWriteState();

  // PWM init for 6 motors
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

static void stopAll() {
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

static void toggleStby() {
  setStbyEnabled(!stbyEnabled);
}

static void setAllDir(Direction d) {
  for (int i=0;i<6;i++) setMotorDirection((Motor)i, d);
}

static void setAllSpeed(uint8_t s) {
  for (int i=0;i<6;i++) setMotorSpeed((Motor)i, s);
}

static void applyDriveDirection() {
  if (gDir == 1) { // forward
    setAllDir(DIR_BACKWARD);
  } else if (gDir == 2) { // back
    setAllDir(DIR_FORWARD);
  } else if (gDir == 3) { // left (turn in place)
    for (int i=0;i<3;i++){
      setMotorDirection(LEFT_MOTORS[i],  DIR_BACKWARD);
      setMotorDirection(RIGHT_MOTORS[i], DIR_FORWARD);
    }
  } else if (gDir == 4) { // right (turn in place)
    for (int i=0;i<3;i++){
      setMotorDirection(LEFT_MOTORS[i],  DIR_FORWARD);
      setMotorDirection(RIGHT_MOTORS[i], DIR_BACKWARD);
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

  // В текущей механике DIR_BACKWARD соответствует движению вперед.
  setMotorDirection(motor, signedSpeed > 0 ? DIR_BACKWARD : DIR_FORWARD);
  setMotorSpeed(motor, duty);
}

static void setSideSigned(const Motor motors[3], int signedSpeed) {
  for (int i=0;i<3;i++) setMotorSigned(motors[i], signedSpeed);
}

static void applyJoystickDrive(int throttle, int turn) {
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

static void startDrive(int dir) {
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

static void updateAcceleration() {
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

// ===================== WiFi init =====================
static void initWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  const bool started = WiFi.softAP(AP_SSID, AP_PASS);
  Debug.print("[WIFI] AP ");
  Debug.println(started ? "started" : "start FAILED");
  Debug.print("[WIFI] SSID: ");
  Debug.println(AP_SSID);
  Debug.print("[WIFI] IP: ");
  Debug.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", AP_IP);
  Debug.println("[DNS] Captive portal DNS started");
  Debug.beginTelnet(23);
  Debug.println("[LOG] connect with: telnet 192.168.4.1 23");
}

static void initOTA() {
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
  Debug.printf("[OTA] ready: %s.local / 192.168.4.1\n", OTA_HOSTNAME);
}

// ===================== WebSocket handlers =====================
static void sendState(AsyncWebSocketClient *client=nullptr) {
  JSONVar o;
  o["speed"] = (int)gTargetSpeed;
  o["appliedSpeed"] = (int)gAppliedSpeed;
  o["dir"] = (int)gDir;
  o["stbyEnabled"] = stbyEnabled;
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

  lastCmdMs = millis();

  // speed
  if (obj.hasOwnProperty("speed")) {
    gTargetSpeed = clampU8((int)obj["speed"]);
    if (gAppliedSpeed > gTargetSpeed) {
      gAppliedSpeed = gTargetSpeed;
      if (gDir != 0) applyDriveSpeed();
    }
  }

  if (obj.hasOwnProperty("stbyToggle")) {
    toggleStby();
  }

  // joystick: left stick = throttle, right stick = turn
  if (obj.hasOwnProperty("joyThrottle") || obj.hasOwnProperty("joyTurn")) {
    const int throttle = obj.hasOwnProperty("joyThrottle") ? (int)obj["joyThrottle"] : 0;
    const int turn = obj.hasOwnProperty("joyTurn") ? (int)obj["joyTurn"] : 0;
    applyJoystickDrive(throttle, turn);
  }

  // direction
  if (obj.hasOwnProperty("dir")) {
    startDrive((int)obj["dir"]);
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
      Debug.printf("[WS] client #%u connected\n", client->id());
      sendState(client);
      break;
    case WS_EVT_DISCONNECT:
      Debug.printf("[WS] client #%u disconnected\n", client->id());
      if (STOP_ON_WS_DISCONNECT) stopAll();
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

static void initWebServer() {
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

// ===================== Setup / loop =====================
void setup() {
  disableBrownoutDetector();

  Debug.begin(115200);
  delay(200);

  Debug.println();
  Debug.println("=== RockerBogie WiFi WS (6 motors + 6 servos) ===");
  Debug.print("[RESET] reason: ");
  Debug.println(resetReasonToStr(esp_reset_reason()));
  Debug.println("[POWER] brownout detector disabled");
  initStbyPin();

  initWiFi();
  initOTA();
  initWebServer();
  Debug.println("[BOOT] OTA and web are ready; hardware init is lazy");
  lastCmdMs = millis();
}

void loop() {
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
  Debug.loop();
  ws.cleanupClients();
  logWs.cleanupClients();
  updateAcceleration();

  if (STOP_ON_CMD_TIMEOUT && gDir != 0 && (millis() - lastCmdMs) > CMD_TIMEOUT_MS) {
    stopAll();
    Debug.println("[SAFETY] command timeout, stop");
  }
}
