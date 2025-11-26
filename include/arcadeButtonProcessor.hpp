#pragma once

#include <BleKeyboard.h>
#include <BleGamepad.h>
#include <Arduino.h>
#include "questPinballFXVR.hpp"
#include "solenoidProcessor.hpp"

// 74HC165 Shift Register (SR) Pins
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

const uint8_t buttonBits[] = {
	BTN_BIT_RMAGNASAVE,
	BTN_BIT_RFLIPPER,
	BTN_BIT_PLUNGER,
	BTN_BIT_SPECIAL,
	BTN_BIT_LMAGNASAVE,
	BTN_BIT_LFLIPPER
};

const uint8_t NUM_BUTTONS = sizeof(buttonBits);

const unsigned long DEBOUNCE_MS = 5;  // try 5–10ms

void processQuestButtons(BleKeyboard* keyboard);
void processVPinButtons(BleKeyboard* keyboard);
void processGamepadButtons(BleGamepad* gamepad);