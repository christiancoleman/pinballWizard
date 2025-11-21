#include <Adafruit_NeoPixel.h>
#include <BleKeyboard.h>
#include <BleGamepad.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <Wire.h>
#include <MPU6050.h>

#define NUMPIXELS         1
#define PIN_NEOPIXEL      5
#define NEOPIXEL_POWER    8
#define NUM_STRIP_LEDS    50    // LED COUNT

// Button pins
#define BTN_RIGHT_FLIPPER 26    // A0
#define BTN_LEFT_FLIPPER  25    // A1
#define BTN_PLUNGER       27    // A2
#define BTN_SPECIAL       15    // A3
#define BTN_START_GAME    12    // MOSI
#define BTN_RMAGNASAVE    13    // MISO
#define BTN_LMAGNASAVE    32    // TX

// Haptic motor pins
#define FLIPPER_MOTORS    7     // RX    

// LED pin
#define PIN_LED_STRIP     14    // SCK

// Accelerometer pins
#define ACCELEROMETER_SDA  4     // SDA
#define ACCELEROMETER_SCL 33     // SCL

// Task handles
TaskHandle_t LEDTaskHandle = NULL;
TaskHandle_t MainTaskHandle = NULL;

// Mutex for shared resources
SemaphoreHandle_t layoutMutex;
SemaphoreHandle_t connectionMutex;
SemaphoreHandle_t ledModeMutex;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip(NUM_STRIP_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

// Layout definitions
enum Layout {
	LAYOUT_QUEST_PINBALL = 0,  // Green - Quest 3 Pinball FX VR (4 buttons)
	LAYOUT_PC_PINBALL = 1,     // Blue - PC full pinball (all buttons)
	LAYOUT_GAMEPAD = 2         // Purple - XInput gamepad
};

// LED Mode definitions
enum LEDMode {
	LED_MODE_CHASE = 0,        // Chase pattern
	LED_MODE_SOLID = 1,        // Solid color
	LED_MODE_RAINBOW = 2       // Color changing
};

const uint32_t layoutColors[] = {
	0x00FF00,  // Green
	0x0000FF,  // Blue
	0xFF00FF   // Purple/Magenta
};

// Variables for the chase pattern
unsigned long lastChaseUpdate = 0;
const unsigned long CHASE_SPEED = 100;  // milliseconds between updates
int chasePosition = 0;

// Variables for rainbow mode
unsigned long lastRainbowUpdate = 0;
const unsigned long RAINBOW_SPEED = 20;  // milliseconds between color updates
uint16_t rainbowHue = 0;  // 0-65535 for full color wheel

// LED mode variables
LEDMode currentLEDMode = LED_MODE_CHASE;
bool ledModeSwitchHandled = false;

// Connection state (shared between tasks)
volatile bool deviceConnected = false;

Preferences preferences;

// Only create the appropriate object based on layout
BleKeyboard* keyboard = nullptr;
BleGamepad* gamepad = nullptr;

Layout currentLayout = LAYOUT_QUEST_PINBALL;
bool isGamepadMode = false;

// Track button states
bool lastRightState = HIGH;
bool lastLeftState = HIGH;
bool lastPlungerState = HIGH;
bool lastSpecialState = HIGH;
bool lastStartState = HIGH;
bool lastRightMagnaSave = HIGH;
bool lastLeftMagnaSave = HIGH;

// Haptic motor timing
unsigned long motorsStartTime = 0;
bool areMotorsActive = false;
const unsigned long HAPTIC_DURATION = 30; // 30ms pulse duration

// Layout switching
unsigned long startPressTime = 0;
bool layoutSwitchHandled = false;

// LED blinking for disconnected state
unsigned long lastBlinkTime = 0;
bool ledState = false;

// Nudge detection variables
const unsigned long NUDGE_PRESS_TIME = 50;
const unsigned long NUDGE_COOLDOWN = 200;  // Increased from 100ms to prevent spam
const int NUDGE_THRESHOLD = 8000;  // Adjust this for sensitivity
MPU6050 mpu; // accelerometer
int16_t ax, ay, az;
int16_t baseX = 0, baseY = 0, baseZ = 0;  // Calibration values
unsigned long lastNudgeTime = 0;
bool nudgeActive = false;
unsigned long nudgeStartTime = 0;
char activeNudgeKey = 0;
bool accelerometerEnabled = false;  // Track if accelerometer is available

// Add mutex-like flag for button processing
bool processingButtons = false;

// Bluetooth specific arrays
const char* layoutNames[] = {"Quest-PinballFXVR", "PC-VisualPinball", "Gamepad-4-Pinball"};
const char* mfgNames[] = {"QPBFXVR", "PCPBVP", "GPAD4PB"};

// Forward declarations for task functions
void LEDTask(void *pvParameters);
void MainTask(void *pvParameters);

void saveLayout(Layout layout) {
	preferences.begin("pinball", false);
	preferences.putUChar("layout", (uint8_t)layout);
	preferences.end();
	Serial.print("Saved layout: ");
	Serial.println(layout);
}

void saveLEDMode(LEDMode mode) {
	preferences.begin("pinball", false);
	preferences.putUChar("ledmode", (uint8_t)mode);
	preferences.end();
	Serial.print("Saved LED mode: ");
	Serial.println(mode);
}

Layout loadLayout() {
	preferences.begin("pinball", true);
	uint8_t saved = preferences.getUChar("layout", 0);
	preferences.end();
	if(saved > 2) saved = 0;  // Safety check
	Serial.print("Loaded layout: ");
	Serial.println(saved);
	return (Layout)saved;
}

LEDMode loadLEDMode() {
	preferences.begin("pinball", true);
	uint8_t saved = preferences.getUChar("ledmode", 0);
	preferences.end();
	if(saved > 2) saved = 0;  // Safety check
	Serial.print("Loaded LED mode: ");
	Serial.println(saved);
	return (LEDMode)saved;
}

void setBLEAddress(uint8_t offset) {
	uint8_t newMAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF, 0x00};
	newMAC[5] = 0x10 + offset;  // Different last byte for each layout
	esp_base_mac_addr_set(newMAC);
}

void initLayout(Layout layout) {
	// Set unique MAC address for each layout
	setBLEAddress(layout);  // 0x10 for Quest, 0x11 for PC, 0x12 for Gamepad

	xSemaphoreTake(layoutMutex, portMAX_DELAY);
	currentLayout = layout;
	isGamepadMode = (layout == LAYOUT_GAMEPAD);
	xSemaphoreGive(layoutMutex);
	
	// Reset all button states when switching layouts
	lastRightState = HIGH;
	lastLeftState = HIGH;
	lastPlungerState = HIGH;
	lastSpecialState = HIGH;
	lastStartState = HIGH;
	lastRightMagnaSave = HIGH;
	lastLeftMagnaSave = HIGH;
	
	// Reset nudge state
	nudgeActive = false;
	activeNudgeKey = 0;
	lastNudgeTime = 0;
	
	if(isGamepadMode) {
		Serial.println("Creating gamepad...");
		gamepad = new BleGamepad(layoutNames[layout], mfgNames[layout], 100);
		delay(200);
		Serial.println("Starting gamepad...");
		gamepad->begin();
		delay(1000);  // Give BLE time to start advertising
		Serial.println("Gamepad mode ready - device should be discoverable");
	} else {
		// Create keyboard with appropriate name
		Serial.print("Creating keyboard: ");
		keyboard = new BleKeyboard(layoutNames[layout], mfgNames[layout], 100);
		delay(200);
		Serial.println("Starting keyboard...");
		keyboard->begin();
		delay(1000);  // Give BLE time to start advertising
		Serial.print("Keyboard mode ready - device should be discoverable as: ");
		Serial.println(layoutNames[layout]);
	}
}

void switchLayout(Layout newLayout) {
	if(newLayout == currentLayout) return;
	
	Serial.print("Switching to layout: ");
	Serial.println(newLayout);
	
	// Release any active inputs before switching
	if(nudgeActive && activeNudgeKey != 0) {
		if(keyboard) keyboard->release(activeNudgeKey);
		if(gamepad) gamepad->setLeftThumb(0, 0);
		nudgeActive = false;
		activeNudgeKey = 0;
	}
	
	// Check if we're switching between keyboard modes (can do live)
	// or switching to/from gamepad mode (needs restart)
	bool needsRestart = (isGamepadMode != (newLayout == LAYOUT_GAMEPAD));
	
	if(needsRestart) {
		// Save new layout and restart
		Serial.println("Mode change requires restart...");
		saveLayout(newLayout);
		
		// Flash LED to indicate restart
		for(int i = 0; i < 6; i++) {
			pixels.fill(layoutColors[newLayout]);
			pixels.show();
			delay(150);
			pixels.fill(0x000000);
			pixels.show();
			delay(150);
		}
		
		Serial.println("Restarting...");
		delay(100);
		ESP.restart();
	} else {
		// Switching between keyboard layouts (Quest <-> PC)
		Serial.println("Switching keyboard modes...");
		if(keyboard != nullptr) {
			Serial.println("Ending current keyboard...");
			keyboard->end();
			delay(500);  // Wait for disconnection
			delete keyboard;
			keyboard = nullptr;
			delay(500);  // Wait for cleanup
		}
		
		xSemaphoreTake(layoutMutex, portMAX_DELAY);
		currentLayout = newLayout;
		xSemaphoreGive(layoutMutex);
		
		initLayout(newLayout);
		saveLayout(newLayout);
		
		Serial.print("Switched to: ");
		Serial.println(layoutNames[newLayout]);
		Serial.println("Device should now be discoverable");
	}
}

void cycleLayout() {
	Layout nextLayout = (Layout)((currentLayout + 1) % 3);
	switchLayout(nextLayout);
}

void cycleLEDMode() {
	xSemaphoreTake(ledModeMutex, portMAX_DELAY);
	currentLEDMode = (LEDMode)((currentLEDMode + 1) % 3);
	xSemaphoreGive(ledModeMutex);
	
	saveLEDMode(currentLEDMode);
	
	Serial.print("LED Mode changed to: ");
	switch(currentLEDMode) {
		case LED_MODE_CHASE:
			Serial.println("Chase");
			break;
		case LED_MODE_SOLID:
			Serial.println("Solid");
			break;
		case LED_MODE_RAINBOW:
			Serial.println("Rainbow");
			break;
	}
}

bool isConnected() {
	bool connected = false;
	
	if(isGamepadMode && gamepad != nullptr) {
		connected = gamepad->isConnected();
	} else if(!isGamepadMode && keyboard != nullptr) {
		connected = keyboard->isConnected();
	}
	
	// Update shared connection state
	xSemaphoreTake(connectionMutex, portMAX_DELAY);
	deviceConnected = connected;
	xSemaphoreGive(connectionMutex);
	
	return connected;
}

void updateLED() {
	bool connected = false;
	Layout layout;
	
	// Get connection state
	xSemaphoreTake(connectionMutex, portMAX_DELAY);
	connected = deviceConnected;
	xSemaphoreGive(connectionMutex);
	
	// Get current layout
	xSemaphoreTake(layoutMutex, portMAX_DELAY);
	layout = currentLayout;
	xSemaphoreGive(layoutMutex);
	
	if(connected) {
		// Solid color when connected
		pixels.fill(layoutColors[layout]);
		pixels.show();
	} else {
		// Blink when disconnected
		if(millis() - lastBlinkTime >= 500) {
			lastBlinkTime = millis();
			ledState = !ledState;
			
			if(ledState) {
				pixels.fill(layoutColors[layout]);
			} else {
				pixels.fill(0x000000);
			}
			pixels.show();
		}
	}
}

void updateChasePattern() {
	unsigned long currentTime = millis();
	
	if (currentTime - lastChaseUpdate >= CHASE_SPEED) {
		lastChaseUpdate = currentTime;
		
		// Clear all LEDs
		strip.clear();
		
		// Get current layout for color
		Layout layout;
		xSemaphoreTake(layoutMutex, portMAX_DELAY);
		layout = currentLayout;
		xSemaphoreGive(layoutMutex);
		
		// Set the current position and trailing LEDs with fading effect
		for (int i = 0; i < 3; i++) {  // 3 LED "tail"
			int pos = (chasePosition - i + NUM_STRIP_LEDS) % NUM_STRIP_LEDS;
			int brightness = 255 - (i * 85);  // Fade the tail
			
			// Use the current layout color
			uint32_t color = layoutColors[layout];
			uint8_t r = (color >> 16) & 0xFF;
			uint8_t g = (color >> 8) & 0xFF;
			uint8_t b = color & 0xFF;
			
			// Apply brightness
			r = (r * brightness) / 255;
			g = (g * brightness) / 255;
			b = (b * brightness) / 255;
			
			strip.setPixelColor(pos, strip.Color(r, g, b));
		}
		
		strip.show();
		
		// Move to next position
		chasePosition = (chasePosition + 1) % NUM_STRIP_LEDS;
	}
}

void updateSolidPattern() {
	// Get current layout for color
	Layout layout;
	xSemaphoreTake(layoutMutex, portMAX_DELAY);
	layout = currentLayout;
	xSemaphoreGive(layoutMutex);
	
	// Fill all LEDs with the current layout color
	strip.fill(layoutColors[layout]);
	strip.show();
}

void updateRainbowPattern() {
	unsigned long currentTime = millis();
	
	if (currentTime - lastRainbowUpdate >= RAINBOW_SPEED) {
		lastRainbowUpdate = currentTime;
		
		// Pre-calculate color once
		uint32_t color = strip.gamma32(strip.ColorHSV(rainbowHue));
		
		// Use fill instead of individual pixel sets
		strip.fill(color);
		strip.show();
		
		rainbowHue += 256;
		if(rainbowHue > 65535) {
			rainbowHue = 0;
		}
	}
}

void updateStripLEDs() {
	LEDMode mode;
	
	xSemaphoreTake(ledModeMutex, portMAX_DELAY);
	mode = currentLEDMode;
	xSemaphoreGive(ledModeMutex);
	
	switch(mode) {
		case LED_MODE_CHASE:
			updateChasePattern();
			break;
		case LED_MODE_SOLID:
			updateSolidPattern();
			break;
		case LED_MODE_RAINBOW:
			updateRainbowPattern();
			break;
	}
}

void setup() {
	// Configure motor pins FIRST, to TRY to prevent vibration at startup
	pinMode(FLIPPER_MOTORS, OUTPUT);
	digitalWrite(FLIPPER_MOTORS, LOW);
	
	Serial.begin(115200);
	delay(1000);
	
	// Create mutexes
	layoutMutex = xSemaphoreCreateMutex();
	connectionMutex = xSemaphoreCreateMutex();
	ledModeMutex = xSemaphoreCreateMutex();
	
	pinMode(NEOPIXEL_POWER, OUTPUT);
	digitalWrite(NEOPIXEL_POWER, HIGH);
	
	// shine onboard light
	pixels.begin();
	pixels.setBrightness(20);

	// Start accelerometer pins and init (chip is MPU6050)
	Wire.begin(ACCELEROMETER_SDA, ACCELEROMETER_SCL);
	mpu.initialize();

	// Initialize LED strip
	strip.begin();
	strip.setBrightness(50);  // Adjust brightness as needed
	strip.show();  // Initialize all pixels to 'off'

	// Set pin modes for all buttons
	pinMode(BTN_RIGHT_FLIPPER, INPUT_PULLUP);
	pinMode(BTN_LEFT_FLIPPER, INPUT_PULLUP);
	pinMode(BTN_PLUNGER, INPUT_PULLUP);
	pinMode(BTN_SPECIAL, INPUT_PULLUP);
	pinMode(BTN_START_GAME, INPUT_PULLUP);
	pinMode(BTN_RMAGNASAVE, INPUT_PULLUP);
	pinMode(BTN_LMAGNASAVE, INPUT_PULLUP);

	Serial.println("=== Pinball Controller Starting ===");
	
	// Load and start with last used layout
	Layout savedLayout = loadLayout();
	initLayout(savedLayout);
	
	// Load LED mode
	currentLEDMode = loadLEDMode();
	
	Serial.println("=== Pinball controller ready ===");
	Serial.println("Device should now be visible for pairing");

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
	
	// Create tasks
	xTaskCreatePinnedToCore(
		LEDTask,          // Task function
		"LED Task",       // Task name
		4096,            // Stack size
		NULL,            // Parameters
		1,               // Priority (1 = low priority)
		&LEDTaskHandle,  // Task handle
		0                // Core 0
	);
	
	xTaskCreatePinnedToCore(
		MainTask,         // Task function
		"Main Task",      // Task name
		8192,            // Stack size (larger for BLE operations)
		NULL,            // Parameters
		2,               // Priority (higher than LED task)
		&MainTaskHandle, // Task handle
		1                // Core 1
	);
	
	// Delete the default Arduino loop task since we're using our own tasks
	vTaskDelete(NULL);
}

// LED Task - runs on Core 0
void LEDTask(void *pvParameters) {
	(void) pvParameters;
	
	Serial.println("LED Task started on core 0");
	
	while(1) {
		updateLED();        // Update onboard NeoPixel
		updateStripLEDs();  // Update LED strip
		
		// Small delay to prevent watchdog issues
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}

void startHaptic(int motorPin, unsigned long &startTime, bool &isActive) {
	// Always turn off first to prevent stuck-on state
	digitalWrite(motorPin, LOW);
	
	// Small delay to ensure motor is off
	delayMicroseconds(100);
	
	// Now start fresh
	digitalWrite(motorPin, HIGH);
	startTime = millis();
	isActive = true;
}

void updateHaptics() {
	unsigned long currentTime = millis();
	
	if (areMotorsActive) {
		unsigned long elapsed = currentTime - motorsStartTime;
		
		// Add safety timeout - never run more than 100ms total
		if (elapsed > 100) {
			digitalWrite(FLIPPER_MOTORS, LOW);
			areMotorsActive = false;
			return;
		}
		
		if (elapsed < 25) {
			// First pulse - keep motor on
			digitalWrite(FLIPPER_MOTORS, HIGH);
		} else if (elapsed < 40) {
			// Gap between pulses - motor off
			digitalWrite(FLIPPER_MOTORS, LOW);
		} else if (elapsed < 65) {
			// Second pulse - motor on
			digitalWrite(FLIPPER_MOTORS, HIGH);
		} else {
			// Done - turn off and reset
			digitalWrite(FLIPPER_MOTORS, LOW);
			areMotorsActive = false;
		}
	}
}

void handleQuestPinballLayout(bool rightPressed, bool leftPressed, bool plungerPressed, bool specialPressed, 
													bool rMagnaSavePressed, bool lMagnaSavePressed) {
	if(keyboard == nullptr || processingButtons) return;
	processingButtons = true;
	
	// Right flipper
	if(rightPressed && lastRightState == HIGH) {
		keyboard->press('6');
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!rightPressed && lastRightState == LOW) {
		keyboard->release('6');
	}
	lastRightState = rightPressed ? LOW : HIGH;
	
	// Left flipper
	if(leftPressed && lastLeftState == HIGH) {
		keyboard->press('u');
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!leftPressed && lastLeftState == LOW) {
		keyboard->release('u');
	}
	lastLeftState = leftPressed ? LOW : HIGH;
	
	// Plunger
	if(plungerPressed && lastPlungerState == HIGH) {
		keyboard->press('8');
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!plungerPressed && lastPlungerState == LOW) {
		keyboard->release('8');
	}
	lastPlungerState = plungerPressed ? LOW : HIGH;

	// Special
	if(specialPressed && lastSpecialState == HIGH) {
		keyboard->press('5');
	} else if(!specialPressed && lastSpecialState == LOW) {
		keyboard->release('5');
	}
	lastSpecialState = specialPressed ? LOW : HIGH;

	// Right Nudge - D (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(rMagnaSavePressed && lastRightMagnaSave == HIGH) {
			keyboard->press('d');
			startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
		} else if(!rMagnaSavePressed && lastRightMagnaSave == LOW) {
			keyboard->release('d');
		}
	}
	lastRightMagnaSave = rMagnaSavePressed ? LOW : HIGH;

	// Left Nudge - F (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(lMagnaSavePressed && lastLeftMagnaSave == HIGH) {
			keyboard->press('f');
			startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
		} else if(!lMagnaSavePressed && lastLeftMagnaSave == LOW) {
			keyboard->release('f');
		}
	}
	lastLeftMagnaSave = lMagnaSavePressed ? LOW : HIGH;
	
	processingButtons = false;
}

void handlePCPinballLayout(bool rightPressed, bool leftPressed, bool plungerPressed, bool specialPressed, 
													bool startPressed, bool rMagnaSavePressed, bool lMagnaSavePressed) {
	if(keyboard == nullptr || processingButtons) return;
	processingButtons = true;
	
	// Right flipper - Right Shift
	if(rightPressed && lastRightState == HIGH) {
		keyboard->press(KEY_RIGHT_SHIFT);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!rightPressed && lastRightState == LOW) {
		keyboard->release(KEY_RIGHT_SHIFT);
	}
	lastRightState = rightPressed ? LOW : HIGH;
	
	// Left flipper - Left Shift
	if(leftPressed && lastLeftState == HIGH) {
		keyboard->press(KEY_LEFT_SHIFT);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!leftPressed && lastLeftState == LOW) {
		keyboard->release(KEY_LEFT_SHIFT);
	}
	lastLeftState = leftPressed ? LOW : HIGH;
	
	// Plunger - Enter
	if(plungerPressed && lastPlungerState == HIGH) {
		keyboard->press(KEY_RETURN);
	} else if(!plungerPressed && lastPlungerState == LOW) {
		keyboard->release(KEY_RETURN);
		// going to try vibrating when the button is released
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	}
	lastPlungerState = plungerPressed ? LOW : HIGH;

	// Coin - 5
	if(specialPressed && lastSpecialState == HIGH) {
		keyboard->press('5');
	} else if(!specialPressed && lastSpecialState == LOW) {
		keyboard->release('5');
	}
	lastSpecialState = specialPressed ? LOW : HIGH;

	// Start game - 1
	if(startPressed && lastStartState == HIGH) {
		keyboard->press('1');
	} else if(!startPressed && lastStartState == LOW) {
		keyboard->release('1');
	}
	lastStartState = startPressed ? LOW : HIGH;

	// Right MagnaSave - Right Ctrl (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(rMagnaSavePressed && lastRightMagnaSave == HIGH) {
			keyboard->press(KEY_RIGHT_CTRL);
		} else if(!rMagnaSavePressed && lastRightMagnaSave == LOW) {
			keyboard->release(KEY_RIGHT_CTRL);
		}
	}
	lastRightMagnaSave = rMagnaSavePressed ? LOW : HIGH;

	// Left MagnaSave - Left Ctrl (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(lMagnaSavePressed && lastLeftMagnaSave == HIGH) {
			keyboard->press(KEY_LEFT_CTRL);
		} else if(!lMagnaSavePressed && lastLeftMagnaSave == LOW) {
			keyboard->release(KEY_LEFT_CTRL);
		}
	}
	lastLeftMagnaSave = lMagnaSavePressed ? LOW : HIGH;
	
	processingButtons = false;
}

void handleGamepadLayout(bool rightPressed, bool leftPressed, bool plungerPressed, bool specialPressed,
												bool startPressed, bool rMagnaSavePressed, bool lMagnaSavePressed) {
	if(gamepad == nullptr || processingButtons) return;
	processingButtons = true;

	// Right flipper (Right shoulder)
	if(rightPressed && lastRightState == HIGH) {
		gamepad->press(BUTTON_10);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!rightPressed && lastRightState == LOW) {
		gamepad->release(BUTTON_10);
	}
	lastRightState = rightPressed ? LOW : HIGH;
	
	// Left flipper (Left shoulder)
	if(leftPressed && lastLeftState == HIGH) {
		gamepad->press(BUTTON_9);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!leftPressed && lastLeftState == LOW) {
		gamepad->release(BUTTON_9);
	}
	lastLeftState = leftPressed ? LOW : HIGH;
	
	// Plunger (A button)
	if(plungerPressed && lastPlungerState == HIGH) {
		gamepad->press(BUTTON_1);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!plungerPressed && lastPlungerState == LOW) {
		gamepad->release(BUTTON_1);
	}
	lastPlungerState = plungerPressed ? LOW : HIGH;
	
	// Special (B button)
	if(specialPressed && lastSpecialState == HIGH) {
		gamepad->press(BUTTON_2);
	} else if(!specialPressed && lastSpecialState == LOW) {
		gamepad->release(BUTTON_2);
	}
	lastSpecialState = specialPressed ? LOW : HIGH;
	
	// Switch Camera (Y button)
	if(startPressed && lastStartState == HIGH) {
		gamepad->press(BUTTON_5);
	} else if(!startPressed && lastStartState == LOW) {
		gamepad->release(BUTTON_5);
	}
	// Note: lastStartState is updated in main loop
	
	// Right/Left Nudge via buttons (only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(rMagnaSavePressed && !lMagnaSavePressed) {
			gamepad->setLeftThumb(32767, 0);  // Right
			if(lastRightMagnaSave == HIGH) {
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
		} else if(lMagnaSavePressed && !rMagnaSavePressed) {
			gamepad->setLeftThumb(-32767, 0);  // Left
			if(lastLeftMagnaSave == HIGH) {
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
		} else {
			gamepad->setLeftThumb(0, 0);  // Center
		}
	}
	lastRightMagnaSave = rMagnaSavePressed ? LOW : HIGH;
	lastLeftMagnaSave = lMagnaSavePressed ? LOW : HIGH;
	
	processingButtons = false;
}

// Non-blocking nudge check
void checkNudge() {
	if (!accelerometerEnabled) return;
	
	// Handle active nudge release
	if (nudgeActive && (millis() - nudgeStartTime >= NUDGE_PRESS_TIME)) {
		if (keyboard && activeNudgeKey != 0) {
			keyboard->release(activeNudgeKey);
		} else if (gamepad) {
			gamepad->setLeftThumb(0, 0);
		}
		nudgeActive = false;
		activeNudgeKey = 0;
	}
	
	// Check cooldown
	if (nudgeActive || (millis() - lastNudgeTime < NUDGE_COOLDOWN)) return;
	
	// Don't check nudge if we're processing buttons to avoid conflicts
	if (processingButtons) return;
	
	mpu.getAcceleration(&ax, &ay, &az);
	
	int16_t deltaX = ax - baseX;
	int16_t deltaY = ay - baseY;
	
	switch(currentLayout) {
		case LAYOUT_QUEST_PINBALL:
			// Quest uses A/S/D/F for 4-way nudge
			if (abs(deltaX) > NUDGE_THRESHOLD || abs(deltaY) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				// Determine primary axis
				if (abs(deltaX) > abs(deltaY)) {
					// X-axis dominates
					if (deltaX > 0) {
						activeNudgeKey = 'f';  // Right
						if (keyboard) keyboard->press('d');
					} else {
						activeNudgeKey = 'd';  // Left
						if (keyboard) keyboard->press('f');
					}
				} else {
					// Y-axis dominates
					if (deltaY > 0) {
						activeNudgeKey = 'a';  // Forward
						if (keyboard) keyboard->press('a');
					} else {
						activeNudgeKey = 's';  // Back
						if (keyboard) keyboard->press('s');
					}
				}
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
			break;
			
		case LAYOUT_PC_PINBALL:
			// PC pinball uses Z/X/Space for nudge
			if (abs(deltaX) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				if (deltaX > 0) {
					activeNudgeKey = '/';  // Right
					if (keyboard) keyboard->press('/');
				} else {
					activeNudgeKey = 'z';  // Left
					if (keyboard) keyboard->press('z');
				}
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			} else if (abs(deltaY) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				activeNudgeKey = ' ';  // Space for forward/back
				if (keyboard) keyboard->press(' ');
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
			break;
			
		case LAYOUT_GAMEPAD:
			// Gamepad uses analog stick
			if (abs(deltaX) > NUDGE_THRESHOLD || abs(deltaY) > NUDGE_THRESHOLD) {
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				activeNudgeKey = 1;  // Just a marker
				
				// Map accelerometer to analog stick
				int16_t stickX = 0, stickY = 0;
				
				if (abs(deltaX) > NUDGE_THRESHOLD) {
					stickX = (deltaX > 0) ? 32767 : -32767;
				}
				if (abs(deltaY) > NUDGE_THRESHOLD) {
					stickY = (deltaY > 0) ? 32767 : -32767;
				}
				
				if (gamepad) gamepad->setLeftThumb(stickX, stickY);
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
			break;
	}
}

// Main Task - runs on Core 1
void MainTask(void *pvParameters) {
	(void) pvParameters;
	
	Serial.println("Main Task started on core 1");
	
	while(1) {
		// Update connection status
		isConnected();
		
		// Update haptics
		updateHaptics();
		
		// Always check nudge if accelerometer is enabled
		if (accelerometerEnabled) {
			checkNudge();
		}
		
		// Read all button states
		bool rightPressed = (digitalRead(BTN_RIGHT_FLIPPER) == LOW);
		bool leftPressed = (digitalRead(BTN_LEFT_FLIPPER) == LOW);
		bool plungerPressed = (digitalRead(BTN_PLUNGER) == LOW);
		bool specialPressed = (digitalRead(BTN_SPECIAL) == LOW);
		bool startPressed = (digitalRead(BTN_START_GAME) == LOW);
		bool rMagnaSavePressed = (digitalRead(BTN_RMAGNASAVE) == LOW);
		bool lMagnaSavePressed = (digitalRead(BTN_LMAGNASAVE) == LOW);
		
		// Handle layout switching (works in all modes, connected or not)
		if(startPressed && lastStartState == HIGH) {
			startPressTime = millis();
			layoutSwitchHandled = false;
		}
		
		if(startPressed && !layoutSwitchHandled) {
			if(millis() - startPressTime >= 3000) {
				cycleLayout();
				layoutSwitchHandled = true;
			}
		}
		
		if(!startPressed) {
			layoutSwitchHandled = false;
		}
		
		// Check for LED mode switching combo
		// Must hold: left flipper + left magna save + right flipper + right magna save
		// Then press special to cycle
		bool ledModeCombo = leftPressed && lMagnaSavePressed && rightPressed && rMagnaSavePressed;
		
		if(ledModeCombo && specialPressed && !ledModeSwitchHandled) {
			cycleLEDMode();
			ledModeSwitchHandled = true;
		}
		
		// Reset the LED mode switch handler when special is released or combo is broken
		if(!specialPressed || !ledModeCombo) {
			ledModeSwitchHandled = false;
		}
		
		// Only process button inputs if connected AND not in LED mode switching combo
		if(deviceConnected && !ledModeCombo) {
			// Route to appropriate layout handler
			switch(currentLayout) {
				case LAYOUT_QUEST_PINBALL:
					handleQuestPinballLayout(rightPressed, leftPressed, plungerPressed, specialPressed,
															rMagnaSavePressed, lMagnaSavePressed);
					break;
					
				case LAYOUT_PC_PINBALL:
					handlePCPinballLayout(rightPressed, leftPressed, plungerPressed, specialPressed,
														startPressed, rMagnaSavePressed, lMagnaSavePressed);
					break;
					
				case LAYOUT_GAMEPAD:
					handleGamepadLayout(rightPressed, leftPressed, plungerPressed, specialPressed,
														 startPressed, rMagnaSavePressed, lMagnaSavePressed);
					break;
			}
		}
		
		// Always update lastStartState at the end (after it's been used in handlers)
		lastStartState = startPressed ? LOW : HIGH;
		
		// Small delay to prevent watchdog issues
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}
}

// Empty loop function since we're using FreeRTOS tasks
void loop() {
	// This will never be called since we deleted the loop task in setup()
}