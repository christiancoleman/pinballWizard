#include <Arduino.h>
#include <BleKeyboard.h>

// Onboard NeoPixel
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8

// 74HC165 Shift Register Pins
#define SR_DATA           7    // QH - Serial data out
#define SR_CLK            14   // CLK - Clock pin
#define SR_LOAD           12   // SH/LD - Latch pin

// Button mappings from shift register (active LOW)
#define BTN_BIT_RMAGNASAVE    0  // A - Right MagnaSave
#define BTN_BIT_RFLIPPER      1  // B - Right Flipper
#define BTN_BIT_PLUNGER       2  // C - Plunger
#define BTN_BIT_UNUSED        3  // D - Unused
#define BTN_BIT_SPECIAL       4  // E - Special
#define BTN_BIT_UNUSED2       5  // F - Unused (was Left Flipper)
#define BTN_BIT_LMAGNASAVE    6  // G - Left MagnaSave
#define BTN_BIT_LFLIPPER      7  // H - Left Flipper

// Quest Pinball FX VR key mappings
#define KEY_RFLIPPER      '6'
#define KEY_LFLIPPER      'u'
#define KEY_PLUNGER       '8'
#define KEY_SPECIAL       '5'
#define KEY_RMAGNASAVE    'd'
#define KEY_LMAGNASAVE    'f'

BleKeyboard keyboard("Quest-PinballFXVR", "QPBFXVR", 100);

uint8_t lastButtonState = 0xFF;
bool wasConnected = false;

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

void setLED(uint8_t r, uint8_t g, uint8_t b) {
	neopixelWrite(PIN_NEOPIXEL, r, g, b);
}

void handleButton(uint8_t buttonState, uint8_t lastState, uint8_t bit, char key) {
	bool pressed = !(buttonState & (1 << bit));
	bool wasPressed = !(lastState & (1 << bit));
	
	if (pressed && !wasPressed) {
		keyboard.press(key);
	} else if (!pressed && wasPressed) {
		keyboard.release(key);
	}
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
	
	uint8_t buttonState = readShiftRegister();
	
	if (buttonState != lastButtonState) {
		handleButton(buttonState, lastButtonState, BTN_BIT_RFLIPPER, KEY_RFLIPPER);
		handleButton(buttonState, lastButtonState, BTN_BIT_LFLIPPER, KEY_LFLIPPER);
		handleButton(buttonState, lastButtonState, BTN_BIT_PLUNGER, KEY_PLUNGER);
		handleButton(buttonState, lastButtonState, BTN_BIT_SPECIAL, KEY_SPECIAL);
		handleButton(buttonState, lastButtonState, BTN_BIT_RMAGNASAVE, KEY_RMAGNASAVE);
		handleButton(buttonState, lastButtonState, BTN_BIT_LMAGNASAVE, KEY_LMAGNASAVE);
		
		lastButtonState = buttonState;
	}
}