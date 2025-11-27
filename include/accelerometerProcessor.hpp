#pragma once

#include <MPU6050.h>

#define ACCELEROMETER_SDA  4     // SDA
#define ACCELEROMETER_SCL 33     // SCL

const unsigned long NUDGE_PRESS_TIME = 50;

const unsigned long NUDGE_COOLDOWN = 200;  // Increased from 100ms to prevent spam

const int NUDGE_THRESHOLD = 8000;  // Adjust this for sensitivity