#include <BleKeyboard.h>
#include "arcadeButtonProcessor.hpp"

// Onboard NeoPixel
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8

BleKeyboard keyboard("Quest-PinballFXVR", "QPBFXVR", 100);

bool wasConnected = false;

void setLED(uint8_t r, uint8_t g, uint8_t b) {
	neopixelWrite(PIN_NEOPIXEL, r, g, b);
}

void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("=== Pinball Controller Starting ===");
	
	pinMode(NEOPIXEL_POWER, OUTPUT);
	digitalWrite(NEOPIXEL_POWER, HIGH);
	
	pinMode(SR_LOAD, OUTPUT);
	pinMode(SR_CLK, OUTPUT);
	pinMode(SR_DATA, INPUT);
	
	digitalWrite(SR_LOAD, HIGH);
	digitalWrite(SR_CLK, LOW);
	
	setLED(255, 255, 0);
	
	keyboard.begin();
	Serial.println("BLE Keyboard started");
	
	setLED(0, 255, 0);
}

void loop() {
	bool connected = keyboard.isConnected();
	
	if (connected != wasConnected) {
		if (connected) {
			Serial.println("Connected!");
			setLED(0, 255, 0);
		} else {
			Serial.println("Disconnected");
		}
		wasConnected = connected;
	}
	
	if (!connected) {
		static unsigned long lastBlink = 0;
		static bool ledOn = false;
		if (millis() - lastBlink > 500) {
			lastBlink = millis();
			ledOn = !ledOn;
			setLED(ledOn ? 0 : 0, ledOn ? 255 : 0, 0);
		}
		return;
	}
	
	processQuestButtons(&keyboard);
}