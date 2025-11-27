#include "accelerometerProcessor.hpp"

extern int currentGameMode;
extern bool processingButtons;
extern bool nudgeActive;
extern MPU6050 mpu;
extern bool accelerometerEnabled;

int16_t ax, ay, az;
int16_t baseX = 0, baseY = 0, baseZ = 0;  // Calibration values

char activeNudgeKey = 0;

unsigned long nudgeStartTime = 0;
unsigned long lastNudgeTime = 0;

void tryToStartAccelerometer(){
	// Try to configure accelerometer aka MPU6050
	if (mpu.testConnection()) {
		Serial.println("MPU6050 connected!");
		accelerometerEnabled = true;
		
		// Calibrate baseline (average of 10 readings)
		long sumX = 0, sumY = 0, sumZ = 0;
		for(int i = 0; i < 10; i++) {
			mpu.getAcceleration(&ax, &ay, &az);
			sumX += ax;
			sumY += ay;
			sumZ += az;
			delay(10);
		}
		baseX = sumX / 10;
		baseY = sumY / 10;
		baseZ = sumZ / 10;
	} else {
		Serial.println("MPU6050 not found!");
		accelerometerEnabled = false;
	}
}

// Non-blocking nudge check
void checkNudge(BleKeyboard* keyboard){
	if (!accelerometerEnabled) return;
	
	// Handle active nudge release
	if (nudgeActive && (millis() - nudgeStartTime >= NUDGE_PRESS_TIME)) {
		if (keyboard && activeNudgeKey != 0) {
			keyboard->release(activeNudgeKey);
		}
		nudgeActive = false;
		activeNudgeKey = 0;
	}
	
	// Check cooldown
	if (nudgeActive || (millis() - lastNudgeTime < NUDGE_COOLDOWN)) return;
	
	mpu.getAcceleration(&ax, &ay, &az);
	
	int16_t deltaX = ax - baseX;
	int16_t deltaY = ay - baseY;
	
	switch(currentGameMode) {
		case MODE_QUEST_PINBALLFXVR:
			// Quest uses A/S/D/F for 4-way nudge
			if (abs(deltaX) > NUDGE_THRESHOLD || abs(deltaY) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				// Determine primary axis
				if (abs(deltaX) > abs(deltaY)) {
					// X-axis dominates
					if (deltaX > 0) {
						activeNudgeKey = KEY_RMAGNASAVE_QPVR;
						if (keyboard) keyboard->press(KEY_RMAGNASAVE_QPVR);
					} else {
						activeNudgeKey = KEY_LMAGNASAVE_QPVR;
						if (keyboard) keyboard->press(KEY_LMAGNASAVE_QPVR);
					}
				}
			}
			break;
			
		case MODE_PC_VISUALPINBALL:
			// PC pinball uses Z/X/Space for nudge
			if (abs(deltaX) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				if (deltaX > 0) {
					activeNudgeKey = '/';
					if (keyboard) keyboard->press('/');
				} else {
					activeNudgeKey = 'z';
					if (keyboard) keyboard->press('z');
				}
			} else if (abs(deltaY) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				activeNudgeKey = ' ';  // Space for forward/back
				if (keyboard) keyboard->press(' ');
			}
			break;
	}
}