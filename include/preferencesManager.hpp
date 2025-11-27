#pragma once

#include <Preferences.h>

#define BOOT_BUTTON 0 // using the Boot button to switch modes

// define game modes
#define MODE_QUEST_PINBALLFXVR         0
#define MODE_PC_VISUALPINBALL          1

enum GAME_MODE {
	QUEST_PINBALL_FX_VR = 0,
	PC_VISUAL_PINBALL = 1
};

int getControllerMode();

void saveControllerMode(int);

void gotoNextMode(int);