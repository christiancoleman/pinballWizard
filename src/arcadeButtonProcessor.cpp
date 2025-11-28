#include "arcadeButtonProcessor.hpp"

extern bool nudgeActive; 

uint8_t stableState = 0xFF;           // debounced state (1 = released)
uint8_t lastRawState = 0xFF;          // last raw read
unsigned long lastChangeTime[8] = {0};

#ifdef BUTTON_DEBUG
static void logButtonEvent(uint8_t bit, uint8_t key, const char* action) {
	Serial.printf("Button bit %u -> key 0x%02X %s\n", bit, key, action);
}
#endif

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
	const ButtonMapping* map = questButtonMap;
	uint8_t totalButtons = QUEST_NUM_BUTTONS;

	if(currentGameMode == PC_VISUAL_PINBALL){
		map = pcButtonMap;
		totalButtons = PC_NUM_BUTTONS;
	}

	uint8_t raw = readShiftRegister();
	unsigned long now = millis();

	for (uint8_t i = 0; i < totalButtons; i++) {
		uint8_t bit = map[i].bit;
		char key = map[i].key;
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
				bool leftNudge = (bit == BTN_BIT_LMAGNASAVE);
				bool rightNudge = (bit == BTN_BIT_RMAGNASAVE);

				// Edge detected on debounced state
				if (rawPressed) {
					if(leftNudge && nudgeActive) return;
					if(rightNudge && nudgeActive) return;
					keyboard->press(key);
					if(leftFlipper) sendLeftFlipperDataHigh();
					else if(rightFlipper) sendRightFlipperDataHigh();
#ifdef BUTTON_DEBUG
					logButtonEvent(bit, key, "pressed");
#endif
				} else {
					keyboard->release(key);
					if(leftFlipper) sendLeftFlipperDataLow();
					else if(rightFlipper) sendRightFlipperDataLow();
#ifdef BUTTON_DEBUG
					logButtonEvent(bit, key, "released");
#endif
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