#include "arcadeButtonProcessor.hpp"
#include "ledStripProcessor.hpp"
#include "accelerometerProcessor.hpp"
#include "solenoidProcessor.hpp"

// Onboard NeoPixel
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8

// define modes
#define MODE_QUEST_PINBALLFXVR         0
#define MODE_PC_VISUALPINBALL          1
#define MODE_GAMEPAD_STARWARSVR        2

BleKeyboard keyboard("Quest-PinballFXVR", "QPBFXVR", 100);
//BleKeyboard keyboard("Quest-PinballFXVR", "QPBFXVR", 100);
//BleGamepad gamepad("Quest")

bool wasConnected = false;
bool toggled = false;

void setLED(uint8_t r, uint8_t g, uint8_t b) {
	neopixelWrite(PIN_NEOPIXEL, r, g, b);
}

void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("=== Pinball Controller Starting ===");
	
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
	
	setLED(255, 255, 0);
	
	keyboard.begin();
	Serial.println("BLE Keyboard started");
	
	setLED(0, 255, 0);

	setLEDStrip();
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
	
	// sendLeftFlipperDataHigh();
	// delay(1000);
	// sendLeftFlipperDataLow();
	// delay(1000);
	// sendRightFlipperDataHigh();
	// delay(1000);
	// sendRightFlipperDataLow();
	// delay(1000);
	processQuestButtons(&keyboard);
}