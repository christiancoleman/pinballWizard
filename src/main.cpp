#include <NeoPixelBus.h>
#include <Adafruit_NeoPixel.h>
#include <BleKeyboard.h>
#include <BleGamepad.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Wire.h>
#include <MPU6050.h>

// Onboard NeoPixel
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8

// 74HC165 Shift Register Pins
#define SR_DATA           7    // QH - Serial data out
#define SR_CLK            14   // CLK - Clock pin
#define SR_LOAD           12   // SH/LD - Latch pin

// Button mappings from shift register (active LOW)
// Bit 0 = A, Bit 1 = B, etc.
#define BTN_BIT_RMAGNASAVE    0  // A - Right MagnaSave
#define BTN_BIT_RFLIPPER      1  // B - Right Flipper
#define BTN_BIT_PLUNGER       2  // C - Plunger
#define BTN_BIT_UNUSED        3  // D - Unused
#define BTN_BIT_SPECIAL       4  // E - Special
#define BTN_BIT_LFLIPPER      5  // F - Left Flipper
#define BTN_BIT_LMAGNASAVE    6  // G - Left MagnaSave
#define BTN_BIT_LFIPPERNEW	  7	 // H - Left flipper (new)

// Quest Pinball FX VR key mappings
#define KEY_RFLIPPER      '6'
#define KEY_LFLIPPER      'u'
#define KEY_PLUNGER       '8'
#define KEY_SPECIAL       '5'
#define KEY_RMAGNASAVE    'd'
#define KEY_LMAGNASAVE    'f'

BleKeyboard keyboard("Quest-PinballFXVR", "QPBFXVR", 100);

// Track previous button states for edge detection
uint8_t lastButtonState = 0xFF;  // All HIGH (unpressed) with pull-ups

// Connection state tracking
bool wasConnected = false;

uint8_t readShiftRegister() {
	// Pulse LOAD low to latch parallel inputs
	digitalWrite(SR_LOAD, LOW);
	delayMicroseconds(10);
	digitalWrite(SR_LOAD, HIGH);
	delayMicroseconds(10);
	
	// Shift in 8 bits
	uint8_t data = 0;
	for (int i = 0; i < 8; i++) {
		data <<= 1;
		if (digitalRead(SR_DATA)) {
			data |= 1;
		}
		digitalWrite(SR_CLK, HIGH);
		delayMicroseconds(10);
		digitalWrite(SR_CLK, LOW);
		delayMicroseconds(10);
	}
	
	return data;
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
	neopixelWrite(PIN_NEOPIXEL, r, g, b);
}

void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("=== Pinball Controller Starting ===");
	
	// Configure NeoPixel power
	pinMode(NEOPIXEL_POWER, OUTPUT);
	digitalWrite(NEOPIXEL_POWER, HIGH);
	
	// Configure shift register pins
	pinMode(SR_LOAD, OUTPUT);
	pinMode(SR_CLK, OUTPUT);
	pinMode(SR_DATA, INPUT);
	
	// Set initial states
	digitalWrite(SR_LOAD, HIGH);
	digitalWrite(SR_CLK, LOW);
	
	// Show yellow while starting up
	setLED(255, 255, 0);
	
	// Start BLE keyboard
	keyboard.begin();
	Serial.println("BLE Keyboard started - waiting for connection...");
	
	// Show green when ready
	setLED(0, 255, 0);
}

void loop() {
	bool connected = keyboard.isConnected();
	
	// Log connection changes
	if (connected != wasConnected) {
		if (connected) {
			Serial.println("Connected!");
			setLED(0, 255, 0);  // Solid green
		} else {
			Serial.println("Disconnected");
		}
		wasConnected = connected;
	}
	
	// Blink LED when disconnected
	if (!connected) {
		static unsigned long lastBlink = 0;
		static bool ledOn = false;
		if (millis() - lastBlink > 500) {
			lastBlink = millis();
			ledOn = !ledOn;
			if (ledOn) {
				setLED(0, 255, 0);
			} else {
				setLED(0, 0, 0);
			}
		}
		return;  // Don't process buttons if not connected
	}
	
	// Read shift register
	uint8_t buttonState = readShiftRegister();
	
	// Debug: print raw value when it changes
	static uint8_t lastDebugState = 0xFF;
	if (buttonState != lastDebugState) {
		Serial.print("Raw SR: 0b");
		for (int i = 7; i >= 0; i--) {
			Serial.print((buttonState >> i) & 1);
		}
		Serial.print(" (0x");
		Serial.print(buttonState, HEX);
		Serial.println(")");
		lastDebugState = buttonState;
	}
	
	// Process each button (active LOW with pull-ups - pressed when bit is 0)
	// Right Flipper (B)
	if (!(buttonState & (1 << BTN_BIT_RFLIPPER)) && (lastButtonState & (1 << BTN_BIT_RFLIPPER))) {
		Serial.println("Right Flipper PRESSED");
		keyboard.press(KEY_RFLIPPER);
	} else if ((buttonState & (1 << BTN_BIT_RFLIPPER)) && !(lastButtonState & (1 << BTN_BIT_RFLIPPER))) {
		Serial.println("Right Flipper RELEASED");
		keyboard.release(KEY_RFLIPPER);
	}
	
	// Left Flipper (F)
	if (!(buttonState & (1 << BTN_BIT_LFIPPERNEW)) && (lastButtonState & (1 << BTN_BIT_LFIPPERNEW))) {
		Serial.println("Left Flipper PRESSED");
		keyboard.press(KEY_LFLIPPER);
	} else if ((buttonState & (1 << BTN_BIT_LFIPPERNEW)) && !(lastButtonState & (1 << BTN_BIT_LFIPPERNEW))) {
		Serial.println("Left Flipper RELEASED");
		keyboard.release(KEY_LFLIPPER);
	}
	
	// Plunger (C)
	if (!(buttonState & (1 << BTN_BIT_PLUNGER)) && (lastButtonState & (1 << BTN_BIT_PLUNGER))) {
		Serial.println("Plunger PRESSED");
		keyboard.press(KEY_PLUNGER);
	} else if ((buttonState & (1 << BTN_BIT_PLUNGER)) && !(lastButtonState & (1 << BTN_BIT_PLUNGER))) {
		Serial.println("Plunger RELEASED");
		keyboard.release(KEY_PLUNGER);
	}
	
	// Special (E)
	if (!(buttonState & (1 << BTN_BIT_SPECIAL)) && (lastButtonState & (1 << BTN_BIT_SPECIAL))) {
		Serial.println("Special PRESSED");
		keyboard.press(KEY_SPECIAL);
	} else if ((buttonState & (1 << BTN_BIT_SPECIAL)) && !(lastButtonState & (1 << BTN_BIT_SPECIAL))) {
		Serial.println("Special RELEASED");
		keyboard.release(KEY_SPECIAL);
	}
	
	// Right MagnaSave (A)
	if (!(buttonState & (1 << BTN_BIT_RMAGNASAVE)) && (lastButtonState & (1 << BTN_BIT_RMAGNASAVE))) {
		Serial.println("Right MagnaSave PRESSED");
		keyboard.press(KEY_RMAGNASAVE);
	} else if ((buttonState & (1 << BTN_BIT_RMAGNASAVE)) && !(lastButtonState & (1 << BTN_BIT_RMAGNASAVE))) {
		Serial.println("Right MagnaSave RELEASED");
		keyboard.release(KEY_RMAGNASAVE);
	}
	
	// Left MagnaSave (G)
	if (!(buttonState & (1 << BTN_BIT_LMAGNASAVE)) && (lastButtonState & (1 << BTN_BIT_LMAGNASAVE))) {
		Serial.println("Left MagnaSave PRESSED");
		keyboard.press(KEY_LMAGNASAVE);
	} else if ((buttonState & (1 << BTN_BIT_LMAGNASAVE)) && !(lastButtonState & (1 << BTN_BIT_LMAGNASAVE))) {
		Serial.println("Left MagnaSave RELEASED");
		keyboard.release(KEY_LMAGNASAVE);
	}
	
	lastButtonState = buttonState;
	
	delay(5);  // Delay for stability
}
