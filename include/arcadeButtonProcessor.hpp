#pragma once

#include <BleKeyboard.h>
#include <BleGamepad.h>
#include <Arduino.h>
#include "preferencesManager.hpp"
#include "solenoidProcessor.hpp"

extern int currentGameMode;

// 74HC165 Shift Register (SR) Pins
#define SR_DATA           7    // QH - Serial data out
#define SR_CLK            14   // CLK - Clock pin
#define SR_LOAD           12   // SH/LD - Latch pin

// Button mappings from shift register (active LOW)
#define BTN_BIT_RMAGNASAVE    0  // A - Right MagnaSave
#define BTN_BIT_RFLIPPER      1  // B - Right Flipper
#define BTN_BIT_PLUNGER       2  // C - Plunger
#define BTN_BIT_START         3  // D - Start button
#define BTN_BIT_SPECIAL       4  // E - Special
#define BTN_BIT_NOTUSED       5  // F - nothing assigned
#define BTN_BIT_LMAGNASAVE    6  // G - Left MagnaSave
#define BTN_BIT_LFLIPPER      7  // H - Left Flipper

// Quest Pinball FX VR key mappings
#define KEY_RFLIPPER_QPVR      '6'
#define KEY_LFLIPPER_QPVR      'u'
#define KEY_PLUNGER_QPVR       '8'
#define KEY_SPECIAL_QPVR       '5'
#define KEY_RMAGNASAVE_QPVR    'd'
#define KEY_LMAGNASAVE_QPVR    'f'
#define KEY_NUDGE_UP_QPVR      'a'

// PC Visual Pinball key mappings
#define KEY_RFLIPPER_PCVP      KEY_RIGHT_SHIFT
#define KEY_LFLIPPER_PCVP      KEY_LEFT_SHIFT
#define KEY_PLUNGER_PCVP       KEY_RETURN
#define KEY_SPECIAL_PCVP       '5'   // Coin
#define KEY_START_PCVP         '1'
#define KEY_RMAGNASAVE_PCVP    KEY_RIGHT_CTRL
#define KEY_LMAGNASAVE_PCVP    KEY_LEFT_CTRL

struct ButtonMapping {
		uint8_t bit;
		char key;
};

const ButtonMapping questButtonMap[] = {
		{BTN_BIT_RMAGNASAVE, KEY_RMAGNASAVE_QPVR},
		{BTN_BIT_RFLIPPER,   KEY_RFLIPPER_QPVR},
		{BTN_BIT_PLUNGER,    KEY_PLUNGER_QPVR},
		{BTN_BIT_SPECIAL,    KEY_SPECIAL_QPVR},
		{BTN_BIT_LMAGNASAVE, KEY_LMAGNASAVE_QPVR},
		{BTN_BIT_LFLIPPER,   KEY_LFLIPPER_QPVR},
		{BTN_BIT_START,      KEY_NUDGE_UP_QPVR}
};

const ButtonMapping pcButtonMap[] = {
		{BTN_BIT_RMAGNASAVE, KEY_RMAGNASAVE_PCVP},
		{BTN_BIT_RFLIPPER,   KEY_RFLIPPER_PCVP},
		{BTN_BIT_PLUNGER,    KEY_PLUNGER_PCVP},
		{BTN_BIT_SPECIAL,    KEY_SPECIAL_PCVP},
		{BTN_BIT_START,      KEY_START_PCVP},
		{BTN_BIT_LMAGNASAVE, KEY_LMAGNASAVE_PCVP},
		{BTN_BIT_LFLIPPER,   KEY_LFLIPPER_PCVP}
};

const uint8_t QUEST_NUM_BUTTONS = sizeof(questButtonMap) / sizeof(ButtonMapping);
const uint8_t PC_NUM_BUTTONS = sizeof(pcButtonMap) / sizeof(ButtonMapping);

const unsigned long DEBOUNCE_MS = 5;  // try 5–10ms

void processKeyboardButtons(BleKeyboard* keyboard);

// Uncomment to enable Serial debug logs for button edges
//#define BUTTON_DEBUG