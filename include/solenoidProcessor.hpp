#pragma once

#include <Arduino.h>

#define LEFT_SOLENOID         26
#define RIGHT_SOLENOID        25

void sendLeftFlipperDataHigh();
void sendRightFlipperDataHigh();
void sendLeftFlipperDataLow();
void sendRightFlipperDataLow();
