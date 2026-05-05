# Rocker Bogie

Прошивка для ESP32-робота Rocker Bogie: ESP32 сама поднимает Wi-Fi сеть, отдаёт веб-интерфейс управления, принимает команды моторов/серв через WebSocket и поддерживает OTA-загрузку прошивки по Wi-Fi.

## Схема и питание

Файл схемы лежит здесь: [readme_assets/scheme.svg](readme_assets/scheme.svg).

![Схема подключения](readme_assets/scheme.svg)


### Что выдают батарейки

Используется сборка `3S1P`: три Li-ion 18650 последовательно.

| Состояние сборки | Напряжение одной ячейки | Напряжение всей 3S сборки |
| --- | ---: | ---: |
| Полностью заряжена | `4.2V` | `12.6V` |
| Номинал | `3.7V` | `11.1V` |
| Рекомендуемый нижний предел под нагрузкой | `~3.0V` | `~9.0V` |
| Жёсткое отключение BMS, зависит от платы | `~2.4-2.7V` | `~7.2-8.1V` |

## Пины и адреса из прошивки

| Назначение | ESP32 / I2C |
| --- | --- |
| Wi-Fi AP | `RockerBogie`, `192.168.4.1` |
| I2C SDA / SCL | GPIO `21` / GPIO `22` |
| TB6612FNG STBY | GPIO `4` |
| PCA9685 | I2C `0x40`, servo channels `0-5` |
| PCF8574 main | I2C `0x20`, motor direction pins for motors `1-4` |
| PCF8574 rear | I2C `0x21`, motor direction pins for motors `5-6` |
| Motor PWM 1..6 | GPIO `25`, `33`, `27`, `26`, `13`, `14` |

Соответствие бортов в прошивке:

- левый борт: motors `1`, `3`, `5`;
- правый борт: motors `2`, `4`, `6`.

## Источники для электрических пределов

- ESP32 Datasheet, Espressif: https://documentation.espressif.com/esp32_datasheet_en.html
- TB6612FNG, Toshiba: https://toshiba.semicon-storage.com/us/semiconductor/product/motor-driver-ics/brushed-dc-motor-driver-ics/detail.TB6612FNG.html
- PCF8574, NXP/TI-compatible datasheet values: https://www.digikey.com/en/htmldatasheets/production/97867/0/0/1/pcf8574.html
- PCA9685, NXP: https://www.nxp.com/products/power-management/lighting-driver-and-controller-ics/led-drivers/16-channel-12-bit-pwm-fm-plus-ic-bus-led-driver%3APCA9685
- Adafruit PCA9685 servo board notes: https://learn.adafruit.com/16-channel-pwm-servo-driver/pinouts
- Rexant 18650 30-2020 product card: https://rexant.kz/product/akkumulyator-li-ion-18650-3-7v-2600mach-bez-platy-zashchity-vysokiy-kontakt-10-sht-korobka-rexant-30-2020/
- Typical 3S 40A BMS values: https://www.robotics.org.za/BMS-3S-40A
