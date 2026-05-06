#pragma once

#include <Arduino.h>

void initWiFi();
void initOTA();
void networkLoop();
bool updateStationCount();
uint8_t getStationCount();
