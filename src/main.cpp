#include <Arduino.h>

/* Мотор 1 и 2 
  PWMA - D13 ESP32
  PWMB - D14 ESP32

*/

/* Мотор 3 и 4
  PWMA - D27 ESP32
  PWMB - D26 ESP32
  
*/

/* Мотор 5 и 6 
  PWMA - D25 ESP32
  PWMB - D33 ESP32
  
*/

/* General Info

  Жёлтый провод     - AIN1
  Оранжевый провод  - AIN2
  Зелёный провод    - BIN1
  Фиолетовый провод - BIN2

  STBY                            - D4  ESP32
  OE (как STBY только на servo)   - D32 ESP32

  */

const int PWM_PIN  = 25;    // D25 на ESP32 = GPIO25
const int PWM_CH   = 0;     // любой канал 0..15
const int PWM_FREQ = 5000;  // частота ШИМ, Гц
const int PWM_RES  = 8;     // разрешение 8 бит (0..255)

const int STBY_PIN = 32;    // D32 на ESP32

void setup() {
  Serial.begin(115200);     // <<< ОБЯЗАТЕЛЬНО
  delay(1000);              // дать Serial стабилизироваться

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
