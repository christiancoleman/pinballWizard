#include <NeoPixelBus.h>
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

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt1Ws2812xMethod> strip(NUM_STRIP_LEDS, PIN_LED_STRIP);

// Game mode definitions
enum GameMode {
	GAMEMODE_QUEST_PINBALL = 0,  // Green - Quest 3 Pinball FX VR
	GAMEMODE_PC_PINBALL = 1,     // Blue - PC Visual Pinball
	GAMEMODE_GAMEPAD = 2         // Purple - XInput gamepad
};

// LED Mode definitions
enum LEDMode {
	LED_MODE_CHASE = 0,        // Chase pattern
	LED_MODE_SOLID = 1,        // Solid color
	LED_MODE_RAINBOW = 2       // Color changing
};

const RgbColor ledLayoutColors[] = {
	RgbColor(0, 255, 0),    // Green
	RgbColor(0, 0, 255),    // Blue
	RgbColor(255, 0, 255)   // Purple/Magenta
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

// Only create the appropriate object based on game mode
BleKeyboard* keyboard = nullptr;
BleGamepad* gamepad = nullptr;

// sets to quest upon boot, but setup will get the correct one
GameMode currentGameMode = GAMEMODE_QUEST_PINBALL;
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

// Game mode switching
unsigned long startPressTime = 0;
bool gameModeSwitchHandled = false;

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

// Bluetooth specific arrays
const char* gameModeNames[] = {"Quest-PinballFXVR", "PC-VisualPinball", "Gamepad-4-Pinball"};
const char* mfgNames[] = {"QPBFXVR", "PCPBVP", "GP4PB"};

// uchar names for saving settings
#define TOPLEVELNAME "pinballv3"
#define LEDMODE "ledmode"
#define GAMEMODE "gamemode"

// Forward declarations for task functions
void LEDTask(void *pvParameters);

void releaseAllKeys() {
	Serial.println("[releaseAllKeys] Releasing all keys and buttons");
	if(keyboard != nullptr) {
		keyboard->releaseAll();
		// Release specific keys that might be stuck
		keyboard->release('6');
		keyboard->release('u');
		keyboard->release('8');
		keyboard->release('5');
		keyboard->release('d');
		keyboard->release('f');
		keyboard->release('a');
		keyboard->release('s');
		keyboard->release('z');
		keyboard->release('/');
		keyboard->release(' ');
		keyboard->release('1');
		keyboard->release(KEY_RIGHT_SHIFT);
		keyboard->release(KEY_LEFT_SHIFT);
		keyboard->release(KEY_RETURN);
		keyboard->release(KEY_RIGHT_CTRL);
		keyboard->release(KEY_LEFT_CTRL);
		Serial.println("[releaseAllKeys] Keyboard keys released");
	}
	if(gamepad != nullptr) {
		gamepad->release(BUTTON_10);
		gamepad->release(BUTTON_9);
		gamepad->release(BUTTON_1);
		gamepad->release(BUTTON_2);
		gamepad->release(BUTTON_5);
		gamepad->setLeftThumb(0, 0);
		gamepad->setRightThumb(0, 0);
		Serial.println("[releaseAllKeys] Gamepad buttons released");
	}
}

void saveGameMode(GameMode gameMode) {
	preferences.begin(TOPLEVELNAME, false);
	preferences.putUChar(GAMEMODE, (uint8_t)gameMode);
	preferences.end();
	Serial.print("[saveGameMode] Saved gameMode: ");
	Serial.println(gameMode);
}

void saveLEDMode(LEDMode ledMode) {
	preferences.begin(TOPLEVELNAME, false);
	preferences.putUChar(LEDMODE, (uint8_t)ledMode);
	preferences.end();
	Serial.print("[saveLEDMode] Saved ledMode: ");
	Serial.println(ledMode);
}

GameMode loadGameMode() {
	preferences.begin(TOPLEVELNAME, true);
	uint8_t saved = preferences.getUChar(GAMEMODE, 0);
	preferences.end();
	Serial.print("[loadGameMode] Raw saved value: ");
	Serial.println(saved);
	if(saved > 2) saved = 0;  // Safety check
	Serial.print("[loadGameMode] Loaded gameMode: ");
	Serial.println(saved);
	return (GameMode)saved;
}

LEDMode loadLEDMode() {
	preferences.begin(TOPLEVELNAME, true);
	uint8_t saved = preferences.getUChar(LEDMODE, 0);
	preferences.end();
	if(saved > 2) saved = 0;  // Safety check
	Serial.print("[loadLEDMode] Loaded ledMode: ");
	Serial.println(saved);
	return (LEDMode)saved;
}

void setBLEAddress(uint8_t offset) {
	uint8_t newMAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF, 0x00};
	newMAC[5] = 0x10 + offset;  // Different last byte for each game mode
	esp_base_mac_addr_set(newMAC);
	Serial.print("[setBLEAddress] MAC set with offset: ");
	Serial.println(offset);
}

void initGameMode(GameMode gameMode) {
	Serial.print("[initGameMode] Initializing gameMode: ");
	Serial.println(gameMode);
	
	// Set unique MAC address for each game mode
	setBLEAddress(gameMode);

	currentGameMode = gameMode;
	isGamepadMode = (gameMode == GAMEMODE_GAMEPAD);
	Serial.print("[initGameMode] isGamepadMode: ");
	Serial.println(isGamepadMode);
	
	// Reset all button states
	lastRightState = HIGH;
	lastLeftState = HIGH;
	lastPlungerState = HIGH;
	lastSpecialState = HIGH;
	lastStartState = HIGH;
	lastRightMagnaSave = HIGH;
	lastLeftMagnaSave = HIGH;
	Serial.println("[initGameMode] Button states reset to HIGH");
	
	// Reset nudge state
	nudgeActive = false;
	activeNudgeKey = 0;
	lastNudgeTime = 0;
	nudgeStartTime = 0;
	Serial.println("[initGameMode] Nudge state reset");
	
	// Reset haptic state
	areMotorsActive = false;
	motorsStartTime = 0;
	
	// Clean up any existing objects
	if(keyboard != nullptr) {
		Serial.println("[initGameMode] Cleaning up existing keyboard");
		keyboard->releaseAll();  // CRITICAL: Release all keys before deleting
		delay(100);
		keyboard->end();
		delay(100);
		delete keyboard;
		keyboard = nullptr;
	}
	if(gamepad != nullptr) {
		Serial.println("[initGameMode] Cleaning up existing gamepad");
		gamepad->release(BUTTON_10);
		gamepad->release(BUTTON_9);
		gamepad->release(BUTTON_1);
		gamepad->release(BUTTON_2);
		gamepad->release(BUTTON_5);
		gamepad->setLeftThumb(0, 0);
		gamepad->setRightThumb(0, 0);
		delay(100);
		gamepad->end();
		delay(100);
		delete gamepad;
		gamepad = nullptr;
	}
	
	delay(500);  // Give BLE stack time to clean up
	Serial.println("[initGameMode] BLE cleanup delay complete");
	
	if(isGamepadMode) {
		Serial.println("[initGameMode] Creating gamepad...");
		gamepad = new BleGamepad(gameModeNames[gameMode], mfgNames[gameMode], 100);
		delay(200);
		Serial.println("[initGameMode] Starting gamepad...");
		gamepad->begin();
		delay(1000);
		Serial.println("[initGameMode] Gamepad mode ready - device should be discoverable");
	} else {
		Serial.print("[initGameMode] Creating keyboard: ");
		Serial.println(gameModeNames[gameMode]);
		keyboard = new BleKeyboard(gameModeNames[gameMode], mfgNames[gameMode], 100);
		delay(200);
		Serial.println("[initGameMode] Starting keyboard...");
		keyboard->begin();
		delay(1000);
		Serial.print("[initGameMode] Keyboard mode ready - device should be discoverable as: ");
		Serial.println(gameModeNames[gameMode]);
	}
}

void switchGameMode(GameMode gameMode) {
	Serial.print("[switchGameMode] Switching to gameMode: ");
	Serial.println(gameMode);
	
	// CRITICAL: Release ALL keys/buttons before switching
	releaseAllKeys();
	delay(100);
	
	// Always restart for clean BLE state
	Serial.println("[switchGameMode] Mode change requires restart...");
	saveGameMode(gameMode);
	
	// Flash LED to indicate restart
	for(int i = 0; i < 6; i++) {
		pixels.fill(layoutColors[gameMode]);
		pixels.show();
		delay(150);
		pixels.fill(0x000000);
		pixels.show();
		delay(150);
	}
	
	Serial.println("[switchGameMode] Restarting...");
	delay(100);
	ESP.restart();
}

void cycleGameMode() {
	GameMode nextGameMode = (GameMode)((currentGameMode + 1) % 3);
	Serial.print("[cycleGameMode] Current: ");
	Serial.print(currentGameMode);
	Serial.print(" -> Next: ");
	Serial.println(nextGameMode);
	switchGameMode(nextGameMode);
}

void cycleLEDMode() {
	LEDMode prevMode = currentLEDMode;
	currentLEDMode = (LEDMode)((currentLEDMode + 1) % 3);
	saveLEDMode(currentLEDMode);
	
	Serial.print("[cycleLEDMode] LED Mode changed from ");
	Serial.print(prevMode);
	Serial.print(" to ");
	Serial.println(currentLEDMode);
}

bool isConnected() {
	bool connected = false;
	if(isGamepadMode && gamepad != nullptr) {
		connected = gamepad->isConnected();
	} else if(!isGamepadMode && keyboard != nullptr) {
		connected = keyboard->isConnected();
	}
	
	// Log connection state changes
	if(connected != deviceConnected) {
		Serial.print("[isConnected] Connection state changed: ");
		Serial.print(deviceConnected);
		Serial.print(" -> ");
		Serial.println(connected);
	}
	
	// Update shared connection state with memory barrier
	__sync_synchronize();
	deviceConnected = connected;
	__sync_synchronize();
	return connected;
}

void updateLED() {
	if(deviceConnected) {
		// Solid color when connected
		pixels.fill(layoutColors[currentGameMode]);
		pixels.show();
	} else {
		// Blink when disconnected
		if(millis() - lastBlinkTime >= 500) {
			lastBlinkTime = millis();
			ledState = !ledState;
			
			if(ledState) {
				pixels.fill(layoutColors[currentGameMode]);
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
		strip.ClearTo(RgbColor(0));
		
		// Get current game mode for unique color
		GameMode gameMode = currentGameMode;
		// Set the current position and trailing LEDs with fading effect
		for (int i = 0; i < 3; i++) {  // 3 LED "tail"
			int pos = (chasePosition - i + NUM_STRIP_LEDS) % NUM_STRIP_LEDS;
			float brightness = 1.0f - (i * 0.333f);  // Fade the tail
			
			RgbColor color = ledLayoutColors[gameMode];
			color = RgbColor::LinearBlend(RgbColor(0), color, brightness);
				
			strip.SetPixelColor(pos, color);
		}
		
		strip.Show();
		
		// Move to next position
		chasePosition = (chasePosition + 1) % NUM_STRIP_LEDS;
	}
}

void updateSolidPattern() {
	strip.ClearTo(ledLayoutColors[currentGameMode]);
	strip.Show();
}

void updateRainbowPattern() {
	unsigned long currentTime = millis();
	
	if (currentTime - lastRainbowUpdate >= RAINBOW_SPEED) {
		lastRainbowUpdate = currentTime;
		
		// Convert hue to color
		HslColor hslColor(rainbowHue / 65535.0f, 1.0f, 0.5f);
		RgbColor rgbColor(hslColor);
		
		// Use fill instead of individual pixel sets
		strip.ClearTo(rgbColor);
		strip.Show();
		
		rainbowHue += 256;
		if(rainbowHue > 65535) {
			rainbowHue = 0;
		}
	}
}

void updateStripLEDs() {
	switch(currentLEDMode) {
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

	Serial.println("[setup] === Setup Starting ===");

	// CRITICAL: Ensure clean state on boot
	keyboard = nullptr;
	gamepad = nullptr;
	nudgeActive = false;
	activeNudgeKey = 0;
	areMotorsActive = false;
	deviceConnected = false;
	Serial.println("[setup] Global state initialized");
	
	// set neo pixel pins
	pinMode(NEOPIXEL_POWER, OUTPUT);
	digitalWrite(NEOPIXEL_POWER, HIGH);
	
	// Initialize NeoPixelBus
	strip.Begin();
	Serial.println("[setup] NeoPixel initialized");

	// Start accelerometer pins and init (chip is MPU6050)
	Wire.begin(ACCELEROMETER_SDA, ACCELEROMETER_SCL);
	mpu.initialize();

	// shine onboard light
	pixels.begin();
	pixels.setBrightness(20);
	pixels.fill(layoutColors[currentGameMode]);
	pixels.show();

	// Initialize LED strip
	strip.Show();  // Initialize all pixels to 'off'

	// Set pin modes for all buttons
	pinMode(BTN_RIGHT_FLIPPER, INPUT_PULLUP);
	pinMode(BTN_LEFT_FLIPPER, INPUT_PULLUP);
	pinMode(BTN_PLUNGER, INPUT_PULLUP);
	pinMode(BTN_SPECIAL, INPUT_PULLUP);
	pinMode(BTN_START_GAME, INPUT_PULLUP);
	pinMode(BTN_RMAGNASAVE, INPUT_PULLUP);
	pinMode(BTN_LMAGNASAVE, INPUT_PULLUP);
	Serial.println("[setup] Button pins configured");

	Serial.println("[setup] === Pinball Controller Starting ===");
	
	// Load and start with last used game mode
	GameMode savedGameMode = loadGameMode();
	initGameMode(savedGameMode);
	
	// Load LED mode
	currentLEDMode = loadLEDMode();
	
	Serial.println("[setup] === Pinball controller ready ===");
	Serial.println("[setup] Device should now be visible for pairing");

	// Try to configure accelerometer aka MPU6050
	if (mpu.testConnection()) {
		Serial.println("[setup] MPU6050 connected!");
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
		Serial.print("[setup] Accelerometer calibrated - baseX: ");
		Serial.print(baseX);
		Serial.print(" baseY: ");
		Serial.print(baseY);
		Serial.print(" baseZ: ");
		Serial.println(baseZ);
	} else {
		Serial.println("[setup] MPU6050 not found!");
		accelerometerEnabled = false;
	}

	accelerometerEnabled = false;

	// Create tasks
	// xTaskCreatePinnedToCore(
	// 	LEDTask,          // Task function
	// 	"LED Task",       // Task name
	// 	4096,            // Stack size
	// 	NULL,            // Parameters
	// 	1,               // Priority (1 = low priority)
	// 	&LEDTaskHandle,  // Task handle
	// 	0                // Core 0
	// );
	Serial.println("[setup] LED Task created on core 1");
}

void LEDTask(void *pvParameters) {
	(void) pvParameters;
	
	TickType_t xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(20);  // 50Hz update rate
	
	while(1) {
		updateLED();
		//updateStripLEDs();
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
	}
}

void startHaptic(int motorPin, unsigned long &startTime, bool &isActive) {
	// Non-blocking version - just start the motor
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

void handleQuestPinballMode(bool rightPressed, bool leftPressed, bool plungerPressed, bool specialPressed, 
													bool rMagnaSavePressed, bool lMagnaSavePressed) {
	if(keyboard == nullptr) {
		Serial.println("[handleQuestPinballMode] keyboard is null, returning");
		return;
	}
	
	// Right flipper
	if(rightPressed && lastRightState == HIGH) {
		//Serial.println("[handleQuestPinballMode] Right flipper PRESSED - sending '6'");
		keyboard->press('6');
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!rightPressed && lastRightState == LOW) {
		//Serial.println("[handleQuestPinballMode] Right flipper RELEASED - releasing '6'");
		keyboard->release('6');
	}
	lastRightState = rightPressed ? LOW : HIGH;
	
	// Left flipper
	if(leftPressed && lastLeftState == HIGH) {
		//Serial.println("[handleQuestPinballMode] Left flipper PRESSED - sending 'u'");
		keyboard->press('u');
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!leftPressed && lastLeftState == LOW) {
		//Serial.println("[handleQuestPinballMode] Left flipper RELEASED - releasing 'u'");
		keyboard->release('u');
	}
	lastLeftState = leftPressed ? LOW : HIGH;
	
	// Plunger
	if(plungerPressed && lastPlungerState == HIGH) {
		//Serial.println("[handleQuestPinballMode] Plunger PRESSED - sending '8'");
		keyboard->press('8');
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!plungerPressed && lastPlungerState == LOW) {
		//Serial.println("[handleQuestPinballMode] Plunger RELEASED - releasing '8'");
		keyboard->release('8');
	}
	lastPlungerState = plungerPressed ? LOW : HIGH;

	// Special
	if(specialPressed && lastSpecialState == HIGH) {
		//Serial.println("[handleQuestPinballMode] Special PRESSED - sending '5'");
		keyboard->press('5');
	} else if(!specialPressed && lastSpecialState == LOW) {
		//Serial.println("[handleQuestPinballMode] Special RELEASED - releasing '5'");
		keyboard->release('5');
	}
	lastSpecialState = specialPressed ? LOW : HIGH;

	// Right Nudge - D (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(rMagnaSavePressed && lastRightMagnaSave == HIGH) {
			Serial.print("[handleQuestPinballMode] Right MagnaSave PRESSED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastRightMagnaSave: ");
			Serial.print(lastRightMagnaSave);
			Serial.println(" - sending 'd'");
			keyboard->press('d');
			startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
		} else if(!rMagnaSavePressed && lastRightMagnaSave == LOW) {
			Serial.print("[handleQuestPinballMode] Right MagnaSave RELEASED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastRightMagnaSave: ");
			Serial.print(lastRightMagnaSave);
			Serial.println(" - releasing 'd'");
			keyboard->release('d');
		}
	} else {
		Serial.print("[handleQuestPinballMode] Right MagnaSave SKIPPED - nudgeActive: ");
		Serial.print(nudgeActive);
		Serial.print(" rMagnaSavePressed: ");
		Serial.print(rMagnaSavePressed);
		Serial.print(" lastRightMagnaSave: ");
		Serial.println(lastRightMagnaSave);
	}
	lastRightMagnaSave = rMagnaSavePressed ? LOW : HIGH;

	// Left Nudge - F (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(lMagnaSavePressed && lastLeftMagnaSave == HIGH) {
			Serial.print("[handleQuestPinballMode] Left MagnaSave PRESSED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastLeftMagnaSave: ");
			Serial.print(lastLeftMagnaSave);
			Serial.println(" - sending 'f'");
			keyboard->press('f');
			startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
		} else if(!lMagnaSavePressed && lastLeftMagnaSave == LOW) {
			Serial.print("[handleQuestPinballMode] Left MagnaSave RELEASED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastLeftMagnaSave: ");
			Serial.print(lastLeftMagnaSave);
			Serial.println(" - releasing 'f'");
			keyboard->release('f');
		}
	} else {
		Serial.print("[handleQuestPinballMode] Left MagnaSave SKIPPED - nudgeActive: ");
		Serial.print(nudgeActive);
		Serial.print(" lMagnaSavePressed: ");
		Serial.print(lMagnaSavePressed);
		Serial.print(" lastLeftMagnaSave: ");
		Serial.println(lastLeftMagnaSave);
	}
	lastLeftMagnaSave = lMagnaSavePressed ? LOW : HIGH;
}

void handlePCPinballMode(bool rightPressed, bool leftPressed, bool plungerPressed, bool specialPressed, 
													bool startPressed, bool rMagnaSavePressed, bool lMagnaSavePressed) {
	if(keyboard == nullptr) {
		Serial.println("[handlePCPinballMode] keyboard is null, returning");
		return;
	}
	
	// Right flipper - Right Shift
	if(rightPressed && lastRightState == HIGH) {
		Serial.println("[handlePCPinballMode] Right flipper PRESSED - sending RIGHT_SHIFT");
		keyboard->press(KEY_RIGHT_SHIFT);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!rightPressed && lastRightState == LOW) {
		Serial.println("[handlePCPinballMode] Right flipper RELEASED - releasing RIGHT_SHIFT");
		keyboard->release(KEY_RIGHT_SHIFT);
	}
	lastRightState = rightPressed ? LOW : HIGH;
	
	// Left flipper - Left Shift
	if(leftPressed && lastLeftState == HIGH) {
		Serial.println("[handlePCPinballMode] Left flipper PRESSED - sending LEFT_SHIFT");
		keyboard->press(KEY_LEFT_SHIFT);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!leftPressed && lastLeftState == LOW) {
		Serial.println("[handlePCPinballMode] Left flipper RELEASED - releasing LEFT_SHIFT");
		keyboard->release(KEY_LEFT_SHIFT);
	}
	lastLeftState = leftPressed ? LOW : HIGH;
	
	// Plunger - Enter
	if(plungerPressed && lastPlungerState == HIGH) {
		Serial.println("[handlePCPinballMode] Plunger PRESSED - sending RETURN");
		keyboard->press(KEY_RETURN);
	} else if(!plungerPressed && lastPlungerState == LOW) {
		Serial.println("[handlePCPinballMode] Plunger RELEASED - releasing RETURN");
		keyboard->release(KEY_RETURN);
		// going to try vibrating when the button is released
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	}
	lastPlungerState = plungerPressed ? LOW : HIGH;

	// Coin - 5
	if(specialPressed && lastSpecialState == HIGH) {
		Serial.println("[handlePCPinballMode] Special PRESSED - sending '5'");
		keyboard->press('5');
	} else if(!specialPressed && lastSpecialState == LOW) {
		Serial.println("[handlePCPinballMode] Special RELEASED - releasing '5'");
		keyboard->release('5');
	}
	lastSpecialState = specialPressed ? LOW : HIGH;

	// Start game - 1
	if(startPressed && lastStartState == HIGH) {
		Serial.println("[handlePCPinballMode] Start PRESSED - sending '1'");
		keyboard->press('1');
	} else if(!startPressed && lastStartState == LOW) {
		Serial.println("[handlePCPinballMode] Start RELEASED - releasing '1'");
		keyboard->release('1');
	}
	lastStartState = startPressed ? LOW : HIGH;

	// Right MagnaSave - Right Ctrl (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(rMagnaSavePressed && lastRightMagnaSave == HIGH) {
			Serial.print("[handlePCPinballMode] Right MagnaSave PRESSED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastRightMagnaSave: ");
			Serial.print(lastRightMagnaSave);
			Serial.println(" - sending RIGHT_CTRL");
			keyboard->press(KEY_RIGHT_CTRL);
			startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
		} else if(!rMagnaSavePressed && lastRightMagnaSave == LOW) {
			Serial.print("[handlePCPinballMode] Right MagnaSave RELEASED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastRightMagnaSave: ");
			Serial.print(lastRightMagnaSave);
			Serial.println(" - releasing RIGHT_CTRL");
			keyboard->release(KEY_RIGHT_CTRL);
		}
	} else {
		Serial.print("[handlePCPinballMode] Right MagnaSave SKIPPED - nudgeActive: ");
		Serial.print(nudgeActive);
		Serial.print(" rMagnaSavePressed: ");
		Serial.print(rMagnaSavePressed);
		Serial.print(" lastRightMagnaSave: ");
		Serial.println(lastRightMagnaSave);
	}
	lastRightMagnaSave = rMagnaSavePressed ? LOW : HIGH;

	// Left MagnaSave - Left Ctrl (but only if not nudging via accelerometer)
	if(!nudgeActive) {
		if(lMagnaSavePressed && lastLeftMagnaSave == HIGH) {
			Serial.print("[handlePCPinballMode] Left MagnaSave PRESSED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastLeftMagnaSave: ");
			Serial.print(lastLeftMagnaSave);
			Serial.println(" - sending LEFT_CTRL");
			keyboard->press(KEY_LEFT_CTRL);
			startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
		} else if(!lMagnaSavePressed && lastLeftMagnaSave == LOW) {
			Serial.print("[handlePCPinballMode] Left MagnaSave RELEASED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastLeftMagnaSave: ");
			Serial.print(lastLeftMagnaSave);
			Serial.println(" - releasing LEFT_CTRL");
			keyboard->release(KEY_LEFT_CTRL);
		}
	} else {
		Serial.print("[handlePCPinballMode] Left MagnaSave SKIPPED - nudgeActive: ");
		Serial.print(nudgeActive);
		Serial.print(" lMagnaSavePressed: ");
		Serial.print(lMagnaSavePressed);
		Serial.print(" lastLeftMagnaSave: ");
		Serial.println(lastLeftMagnaSave);
	}
	lastLeftMagnaSave = lMagnaSavePressed ? LOW : HIGH;
}

void handleGamepadMode(bool rightPressed, bool leftPressed, bool plungerPressed, bool specialPressed,
												bool startPressed, bool rMagnaSavePressed, bool lMagnaSavePressed) {
	if(gamepad == nullptr) {
		Serial.println("[handleGamepadMode] gamepad is null, returning");
		return;
	}

	// Right flipper (Right shoulder)
	if(rightPressed && lastRightState == HIGH) {
		Serial.println("[handleGamepadMode] Right flipper PRESSED - sending BUTTON_10");
		gamepad->press(BUTTON_10);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!rightPressed && lastRightState == LOW) {
		Serial.println("[handleGamepadMode] Right flipper RELEASED - releasing BUTTON_10");
		gamepad->release(BUTTON_10);
	}
	lastRightState = rightPressed ? LOW : HIGH;
	
	// Left flipper (Left shoulder)
	if(leftPressed && lastLeftState == HIGH) {
		Serial.println("[handleGamepadMode] Left flipper PRESSED - sending BUTTON_9");
		gamepad->press(BUTTON_9);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!leftPressed && lastLeftState == LOW) {
		Serial.println("[handleGamepadMode] Left flipper RELEASED - releasing BUTTON_9");
		gamepad->release(BUTTON_9);
	}
	lastLeftState = leftPressed ? LOW : HIGH;
	
	// Plunger (A button)
	if(plungerPressed && lastPlungerState == HIGH) {
		Serial.println("[handleGamepadMode] Plunger PRESSED - sending BUTTON_1");
		gamepad->press(BUTTON_1);
		startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
	} else if(!plungerPressed && lastPlungerState == LOW) {
		Serial.println("[handleGamepadMode] Plunger RELEASED - releasing BUTTON_1");
		gamepad->release(BUTTON_1);
	}
	lastPlungerState = plungerPressed ? LOW : HIGH;
	
	// Special (B button)
	if(specialPressed && lastSpecialState == HIGH) {
		Serial.println("[handleGamepadMode] Special PRESSED - sending BUTTON_2");
		gamepad->press(BUTTON_2);
	} else if(!specialPressed && lastSpecialState == LOW) {
		Serial.println("[handleGamepadMode] Special RELEASED - releasing BUTTON_2");
		gamepad->release(BUTTON_2);
	}
	lastSpecialState = specialPressed ? LOW : HIGH;
	
	// Switch Camera (Y button)
	if(startPressed && lastStartState == HIGH) {
		Serial.println("[handleGamepadMode] Start PRESSED - sending BUTTON_5");
		gamepad->press(BUTTON_5);
	} else if(!startPressed && lastStartState == LOW) {
		Serial.println("[handleGamepadMode] Start RELEASED - releasing BUTTON_5");
		gamepad->release(BUTTON_5);
	}
	// Note: lastStartState is updated in main loop

	// Right MagnaSave
	if(!nudgeActive) {
		if(rMagnaSavePressed && lastRightMagnaSave == HIGH) {
			Serial.print("[handleGamepadMode] Right MagnaSave PRESSED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastRightMagnaSave: ");
			Serial.print(lastRightMagnaSave);
			Serial.println(" - setting LeftThumb(32767, 0)");
			gamepad->setLeftThumb(32767, 0);
		} else if(!rMagnaSavePressed && lastRightMagnaSave == LOW) {
			Serial.print("[handleGamepadMode] Right MagnaSave RELEASED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastRightMagnaSave: ");
			Serial.print(lastRightMagnaSave);
			Serial.println(" - setting LeftThumb(0, 0)");
			gamepad->setLeftThumb(0, 0);
		}
	} else {
		Serial.print("[handleGamepadMode] Right MagnaSave SKIPPED - nudgeActive: ");
		Serial.print(nudgeActive);
		Serial.print(" rMagnaSavePressed: ");
		Serial.print(rMagnaSavePressed);
		Serial.print(" lastRightMagnaSave: ");
		Serial.println(lastRightMagnaSave);
	}
	lastRightMagnaSave = rMagnaSavePressed ? LOW : HIGH;

	// Left MagnaSave
	if(!nudgeActive) {
		if(lMagnaSavePressed && lastLeftMagnaSave == HIGH) {
			Serial.print("[handleGamepadMode] Left MagnaSave PRESSED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastLeftMagnaSave: ");
			Serial.print(lastLeftMagnaSave);
			Serial.println(" - setting LeftThumb(-32767, 0)");
			gamepad->setLeftThumb(-32767, 0);
		} else if(!lMagnaSavePressed && lastLeftMagnaSave == LOW) {
			Serial.print("[handleGamepadMode] Left MagnaSave RELEASED - nudgeActive: ");
			Serial.print(nudgeActive);
			Serial.print(" lastLeftMagnaSave: ");
			Serial.print(lastLeftMagnaSave);
			Serial.println(" - setting LeftThumb(0, 0)");
			gamepad->setLeftThumb(0, 0);
		}
	} else {
		Serial.print("[handleGamepadMode] Left MagnaSave SKIPPED - nudgeActive: ");
		Serial.print(nudgeActive);
		Serial.print(" lMagnaSavePressed: ");
		Serial.print(lMagnaSavePressed);
		Serial.print(" lastLeftMagnaSave: ");
		Serial.println(lastLeftMagnaSave);
	}
	lastLeftMagnaSave = lMagnaSavePressed ? LOW : HIGH;
}

// Non-blocking nudge check
void checkNudge() {
	if (!accelerometerEnabled) return;
	
	// Handle active nudge release
	if (nudgeActive && (millis() - nudgeStartTime >= NUDGE_PRESS_TIME)) {
		Serial.print("[checkNudge] Nudge release - activeNudgeKey: ");
		Serial.print((int)activeNudgeKey);
		Serial.print(" duration: ");
		Serial.println(millis() - nudgeStartTime);
		
		if (keyboard && activeNudgeKey != 0) {
			Serial.print("[checkNudge] Releasing keyboard key: ");
			Serial.println(activeNudgeKey);
			keyboard->release(activeNudgeKey);
		} else if (gamepad) {
			Serial.println("[checkNudge] Releasing gamepad thumb");
			gamepad->setLeftThumb(0, 0);
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
		case GAMEMODE_QUEST_PINBALL:
			// Quest uses A/S/D/F for 4-way nudge
			if (abs(deltaX) > NUDGE_THRESHOLD || abs(deltaY) > NUDGE_THRESHOLD) {
				Serial.print("[checkNudge] QUEST nudge triggered - deltaX: ");
				Serial.print(deltaX);
				Serial.print(" deltaY: ");
				Serial.println(deltaY);
				
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				// Determine primary axis
				if (abs(deltaX) > abs(deltaY)) {
					// X-axis dominates
					if (deltaX > 0) {
						activeNudgeKey = 'f';
						Serial.println("[checkNudge] QUEST nudge X+ - pressing 'f'");
						if (keyboard) keyboard->press('f');
					} else {
						activeNudgeKey = 'd';
						Serial.println("[checkNudge] QUEST nudge X- - pressing 'd'");
						if (keyboard) keyboard->press('d');
					}
				} else {
					// Y-axis dominates
					if (deltaY > 0) {
						activeNudgeKey = 'a';
						Serial.println("[checkNudge] QUEST nudge Y+ - pressing 'a'");
						if (keyboard) keyboard->press('a');
					} else {
						activeNudgeKey = 's';
						Serial.println("[checkNudge] QUEST nudge Y- - pressing 's'");
						if (keyboard) keyboard->press('s');
					}
				}
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
			break;
			
		case GAMEMODE_PC_PINBALL:
			// PC pinball uses Z/X/Space for nudge
			if (abs(deltaX) > NUDGE_THRESHOLD) {
				Serial.print("[checkNudge] PC nudge X triggered - deltaX: ");
				Serial.println(deltaX);
				
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				if (deltaX > 0) {
					activeNudgeKey = '/';  // Right
					Serial.println("[checkNudge] PC nudge X+ - pressing '/'");
					if (keyboard) keyboard->press('/');
				} else {
					activeNudgeKey = 'z';  // Left
					Serial.println("[checkNudge] PC nudge X- - pressing 'z'");
					if (keyboard) keyboard->press('z');
				}
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			} else if (abs(deltaY) > NUDGE_THRESHOLD) {
				Serial.print("[checkNudge] PC nudge Y triggered - deltaY: ");
				Serial.println(deltaY);
				
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				
				activeNudgeKey = ' ';  // Space for forward/back
				Serial.println("[checkNudge] PC nudge Y - pressing SPACE");
				if (keyboard) keyboard->press(' ');
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
			break;
			
		case GAMEMODE_GAMEPAD:
			// Gamepad uses analog stick
			if (abs(deltaX) > NUDGE_THRESHOLD || abs(deltaY) > NUDGE_THRESHOLD) {
				Serial.print("[checkNudge] GAMEPAD nudge triggered - deltaX: ");
				Serial.print(deltaX);
				Serial.print(" deltaY: ");
				Serial.println(deltaY);
				
				lastNudgeTime = millis();
				nudgeStartTime = millis();
				nudgeActive = true;
				activeNudgeKey = 123;  // Just a marker
				
				// Map accelerometer to analog stick
				int16_t stickX = 0, stickY = 0;
				
				if (abs(deltaX) > NUDGE_THRESHOLD) {
					stickX = (deltaX > 0) ? 32767 : -32767;
				}
				if (abs(deltaY) > NUDGE_THRESHOLD) {
					stickY = (deltaY > 0) ? 32767 : -32767;
				}
				
				Serial.print("[checkNudge] GAMEPAD nudge - stickX: ");
				Serial.print(stickX);
				Serial.print(" stickY: ");
				Serial.println(stickY);
				
				if (gamepad) gamepad->setLeftThumb(stickX, stickY);
				startHaptic(FLIPPER_MOTORS, motorsStartTime, areMotorsActive);
			}
			break;
	}
}

void loop() {
	// Update connection status
	bool connected = isConnected();
	
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
	
	// Handle game mode switching (works in all modes, connected or not)
	if(startPressed && lastStartState == HIGH) {
		startPressTime = millis();
		gameModeSwitchHandled = false;
		Serial.println("[loop] Start button pressed - timing for mode switch");
	}
	
	if(startPressed && !gameModeSwitchHandled) {
		if(millis() - startPressTime >= 3000) {
			Serial.println("[loop] Start held 3s - cycling game mode");
			cycleGameMode();
			gameModeSwitchHandled = true;
		}
	}
	
	if(!startPressed) {
		gameModeSwitchHandled = false;
	}
	
	// Check for LED mode switching combo
	// Must hold: left flipper + left magna save + right flipper + right magna save
	// Then press special to cycle
	bool ledModeCombo = leftPressed && lMagnaSavePressed && rightPressed && rMagnaSavePressed;
	
	if(ledModeCombo && specialPressed && !ledModeSwitchHandled) {
		Serial.println("[loop] LED mode combo detected - cycling LED mode");
		cycleLEDMode();
		ledModeSwitchHandled = true;
	}
	
	// Reset the LED mode switch handler when special is released or combo is broken
	if(!specialPressed || !ledModeCombo) {
		ledModeSwitchHandled = false;
	}
	
	// Only process button inputs if connected AND not in LED mode switching combo
	if(connected && !ledModeCombo) {
		// Route to appropriate game mode handler
		switch(currentGameMode) {
			case GAMEMODE_QUEST_PINBALL:
				handleQuestPinballMode(rightPressed, leftPressed, plungerPressed, specialPressed,
														rMagnaSavePressed, lMagnaSavePressed);
				break;
				
			case GAMEMODE_PC_PINBALL:
				handlePCPinballMode(rightPressed, leftPressed, plungerPressed, specialPressed,
													startPressed, rMagnaSavePressed, lMagnaSavePressed);
				break;
				
			case GAMEMODE_GAMEPAD:
				handleGamepadMode(rightPressed, leftPressed, plungerPressed, specialPressed,
														startPressed, rMagnaSavePressed, lMagnaSavePressed);
				break;
		}
	} else if(!connected) {
		// Periodic debug when disconnected
		static unsigned long lastDisconnectLog = 0;
		if(millis() - lastDisconnectLog > 5000) {
			lastDisconnectLog = millis();
			Serial.print("[loop] Not connected - gameMode: ");
			Serial.print(currentGameMode);
			Serial.print(" isGamepadMode: ");
			Serial.print(isGamepadMode);
			Serial.print(" keyboard: ");
			Serial.print(keyboard != nullptr ? "valid" : "null");
			Serial.print(" gamepad: ");
			Serial.println(gamepad != nullptr ? "valid" : "null");
		}
	}
	
	// Always update lastStartState at the end (after it's been used in handlers)
	lastStartState = startPressed ? LOW : HIGH;
	
	// Small delay to prevent watchdog issues
	//vTaskDelay(1 / portTICK_PERIOD_MS);
	//delay(10);
}