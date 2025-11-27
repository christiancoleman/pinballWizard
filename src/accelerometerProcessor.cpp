#include "accelerometerProcessor.hpp"

MPU6050 mpu; // accelerometer
int16_t ax, ay, az;
int16_t baseX = 0, baseY = 0, baseZ = 0;  // Calibration values

char activeNudgeKey = 0;

unsigned long nudgeStartTime = 0;
unsigned long lastNudgeTime = 0;

bool nudgeActive = false;
bool accelerometerEnabled = false;  // Track if accelerometer is available