#include "arcadeButtonProcessor.hpp"
#include "ledStripProcessor.hpp"
#include "accelerometerProcessor.hpp"
#include "solenoidProcessor.hpp"
#include "preferencesManager.hpp"

// Onboard NeoPixel; single LED
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8

bool wasConnected = false;
bool ledOn = false;
bool resetHeld = false;
int currentGameMode = 0;
BleKeyboard* keyboard = nullptr;
BleGamepad* gamepad = nullptr;
unsigned long lastBlink = 0;
unsigned long lastResetPress = 0;

void setLED(uint8_t r, uint8_t g, uint8_t b) {
	neopixelWrite(PIN_NEOPIXEL, r, g, b);
}

void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("=== Pinball Controller Starting ===");
	
	pinMode(BOOT_BUTTON, INPUT_PULLUP);
	pinMode(NEOPIXEL_POWER, OUTPUT);
	pinMode(SR_LOAD, OUTPUT);
	pinMode(SR_CLK, OUTPUT);
	pinMode(SR_DATA, INPUT);
	pinMode(LEFT_SOLENOID, OUTPUT);
  	pinMode(RIGHT_SOLENOID, OUTPUT);
	
	digitalWrite(NEOPIXEL_POWER, HIGH);
	digitalWrite(SR_LOAD, HIGH);
	digitalWrite(SR_CLK, LOW);
	digitalWrite(LEFT_SOLENOID, LOW); 
	digitalWrite(RIGHT_SOLENOID, LOW); 
	
	currentGameMode = getControllerMode();
	if(currentGameMode == QUEST_PINBALL_FX_VR){
		keyboard = new BleKeyboard("Quest-PinballFXVR", "QPBFXVR", 100);
		keyboard->begin();
	} else if(currentGameMode == PC_VISUAL_PINBALL){
		keyboard = new BleKeyboard("PC-VisualPinball", "PCPBVP", 100);
		keyboard->begin();
	} else if(currentGameMode == GAMPEPAD_STAR_WARS_VR){
		gamepad = new BleGamepad("Gamepad-4-Pinball", "GPAD4PB", 100);
		gamepad->begin();
	} else {
		Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
		Serial.println("~~Current mode could not be found: " + currentGameMode);
		Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	}

	setLEDStrip();
}

void loop() {
	if (digitalRead(BOOT_BUTTON) == LOW) {
		if(lastResetPress == 0){
			resetHeld = true;
			lastResetPress = millis();
		}
		if(resetHeld){
			if(millis() - lastResetPress >= 3000) {
				gotoNextMode(currentGameMode);
				delay(1000);
				Serial.println("Killing bluetooth stack");
				btStop();
				delay(1000);
				Serial.println("Restarting the ESP 32");
				ESP.restart();
			} else {
				return;
			}
		}
	}
	lastResetPress = 0;
	resetHeld = false;

	bool connected = false;

	if (keyboard != nullptr) {
		connected |= keyboard->isConnected();
	}
	if (gamepad != nullptr) {
		connected |= gamepad->isConnected();
	}
	
	if (connected != wasConnected) {
		if (connected) {
			Serial.println("Connected!");
			if(currentGameMode == QUEST_PINBALL_FX_VR){
				setLED(0, 255, 0);              // green
			} else if(currentGameMode == PC_VISUAL_PINBALL){
				setLED(160, 32, 240);           // purple
			} else if(currentGameMode == GAMPEPAD_STAR_WARS_VR){
				setLED(0, 0, 255);              // blue
			} else {
				setLED(255, 0, 0);              // red
			}
		} else {
			Serial.println("Disconnected");
		}
		wasConnected = connected;
	}
	
	if (!connected) {
		if (millis() - lastBlink > 500) {
			lastBlink = millis();
			ledOn = !ledOn;
			if(ledOn){
				if(currentGameMode == QUEST_PINBALL_FX_VR){
					setLED(0, 255, 0);              // green
				} else if(currentGameMode == PC_VISUAL_PINBALL){
					setLED(160, 32, 240);           // purple
				} else if(currentGameMode == GAMPEPAD_STAR_WARS_VR){
					setLED(0, 0, 255);              // blue
				}
			} else {
				setLED(0, 0, 0);
			}
		}
		return;
	} else {
		if(currentGameMode < 2){
			processKeyboardButtons(keyboard);
		} else {
		
		}
	}
}