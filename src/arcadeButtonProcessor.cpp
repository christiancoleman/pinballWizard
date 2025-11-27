#include "arcadeButtonProcessor.hpp"

uint8_t stableState = 0xFF;           // debounced state (1 = released)
uint8_t lastRawState = 0xFF;          // last raw read
unsigned long lastChangeTime[8] = {0};

uint8_t readShiftRegister() {
	digitalWrite(SR_LOAD, LOW);
	delayMicroseconds(1);
	digitalWrite(SR_LOAD, HIGH);
	
	uint8_t data = 0;
	for (int i = 0; i < 8; i++) {
		data <<= 1;
		if (digitalRead(SR_DATA)) {
			data |= 1;
		}
		digitalWrite(SR_CLK, HIGH);
		delayMicroseconds(1);
		digitalWrite(SR_CLK, LOW);
	}
	
	return data;
}

void processKeyboardButtons(BleKeyboard* keyboard){
	uint8_t raw = readShiftRegister();
	unsigned long now = millis();

	for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
		uint8_t bit = buttonBits[i];
		char key = buttonKeys[i];
		
		bool rawPressed = !(raw & (1 << bit));
		bool lastRawPressed = !(lastRawState & (1 << bit));

		// If raw level changed, reset debounce timer
		if (rawPressed != lastRawPressed) {
			lastChangeTime[i] = now;
		}

		// Only update stable state after it has stayed the same for DEBOUNCE_MS
		if ((now - lastChangeTime[i]) >= DEBOUNCE_MS) {
			bool stablePressed = !(stableState & (1 << bit));
			if (rawPressed != stablePressed) {
				bool leftFlipper = (bit == BTN_BIT_LFLIPPER);
				bool rightFlipper = (bit == BTN_BIT_RFLIPPER);
				// Edge detected on debounced state
				if (rawPressed) {
					if(leftFlipper) sendLeftFlipperDataHigh();
					else if(rightFlipper) sendRightFlipperDataHigh();
					keyboard->press(key);
				} else {
					if(leftFlipper) sendLeftFlipperDataLow();
					else if(rightFlipper) sendRightFlipperDataLow();
					keyboard->release(key);
				}

				// Update debounced state bit
				if (rawPressed) {
					stableState &= ~(1 << bit);
				} else {
					stableState |= (1 << bit);
				}
			}
		}

		// Update lastRawState bit
		if (rawPressed) {
			lastRawState &= ~(1 << bit);
		} else {
			lastRawState |= (1 << bit);
		}
	}
}


void processGamepadButtons(BleGamepad* gamepad){
}