#pragma once

#include <Arduino.h>

void initStbyPin();
void stopAll();
void toggleStby();
void startDrive(int dir);
void applyJoystickDrive(int throttle, int turn);
void updateAcceleration();

void setTargetSpeed(int speed);
uint8_t getTargetSpeed();
uint8_t getAppliedSpeed();
int getDriveDirection();
bool isStbyEnabled();

void resetCommandTimer();
void noteCommandReceived();
bool shouldStopForCommandTimeout();
bool shouldStopOnWsDisconnect();
