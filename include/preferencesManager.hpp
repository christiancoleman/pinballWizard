#pragma once

#include <Preferences.h>

enum GAME_MODE : uint8_t {
	QUEST_PINBALL_FX_VR = 0,
	PC_VISUAL_PINBALL = 1,
	GAMPEPAD_STAR_WARS_VR = 2

};

uint8_t getControllerMode();

void saveControllerMode();