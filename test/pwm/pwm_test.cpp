#include <Arduino.h>

const int PWM_PIN = 25;      // D25 на ESP32 = GPIO25
const int PWM_CH  = 0;       // любой канал 0..15
const int PWM_FREQ = 5000;   // частота ШИМ, Гц
const int PWM_RES  = 8;      // разрешение 8 бит (0..255)

void setup() {
  Serial.begin(115200);          // <<< ОБЯЗАТЕЛЬНО
  delay(1000);                   // дать Serial стабилизироваться

  Serial.println("Serial started!");

  // Настройка канала PWM
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);

  // Привязываем канал к пину
  ledcAttachPin(PWM_PIN, PWM_CH);
}

void loop() {
  // Плавное увеличение яркости (скважности)
  for (int duty = 0; duty <= 255; duty+=16) {
    ledcWrite(PWM_CH, duty);
    Serial.println("PWM UP");
    delay(500);
  }

  // Плавное уменьшение
  for (int duty = 255; duty >= 0; duty-=16) {
    ledcWrite(PWM_CH, duty);
    Serial.println("PWM DOWN");
    delay(500);
  }
}
