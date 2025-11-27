#include "arcadeButtonProcessor.hpp"
#include "ledStripProcessor.hpp"
#include "accelerometerProcessor.hpp"
#include "solenoidProcessor.hpp"
#include "preferencesManager.hpp"

// Onboard NeoPixel; single LED
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8

bool connected = false;
bool wasConnected = false;
bool ledOn = false;
bool resetHeld = false;
bool accelerometerEnabled = false;
bool processingButtons = false;
bool nudgeActive = false;
int currentGameMode = 0;
unsigned long lastBlink = 0;
unsigned long lastResetPress = 0;
BleKeyboard keyboard("pinballWizard", "cc", 68);
MPU6050 mpu;

void setLED(uint8_t r, uint8_t g, uint8_t b) {
	neopixelWrite(PIN_NEOPIXEL, r, g, b);
}

void setup() {
	Serial.begin(115200);
	delay(1000);
	Serial.println("=== Pinball Controller Starting ===");

	// Start accelerometer pins and init (chip is MPU6050)
	Wire.begin(ACCELEROMETER_SDA, ACCELEROMETER_SCL);
	mpu.initialize();
	
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

	tryToStartAccelerometer();
	
	currentGameMode = getControllerMode();
	keyboard.begin();

	setLEDStrip(currentGameMode);
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
				Serial.println("Restarting the ESP 32...");
				ESP.restart();
			} else {
				return;
			}
		}
	}
	lastResetPress = 0;
	resetHeld = false;

	if(keyboard.isConnected()){
		connected = true;
	}
	
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
		if (millis() - lastBlink > 500) {
			lastBlink = millis();
			ledOn = !ledOn;
			setLED(0, ledOn ? 255 : 0, 0);
		}
		return;
	} 

	processKeyboardButtons(&keyboard);
	if(accelerometerEnabled) {
		if(!processingButtons) checkNudge(&keyboard);
	}
}