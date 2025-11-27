#pragma once

#include <BleKeyboard.h>
#include <MPU6050.h>
#include "preferencesManager.hpp"
#include "arcadeButtonProcessor.hpp"

#define ACCELEROMETER_SDA  4     // SDA
#define ACCELEROMETER_SCL 33     // SCL

const unsigned long NUDGE_PRESS_TIME = 50;
const unsigned long NUDGE_COOLDOWN = 100;  // Increased from 100ms to prevent spam
const int NUDGE_THRESHOLD = 8000;  // Adjust this for sensitivity

void tryToStartAccelerometer();
void checkNudge(BleKeyboard* keyboard);