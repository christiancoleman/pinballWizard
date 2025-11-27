#pragma once

#include <Preferences.h>

#define BOOT_BUTTON 0 // using the Boot button to switch modes

// define game modes
#define MODE_QUEST_PINBALLFXVR         0
#define MODE_PC_VISUALPINBALL          1
#define MODE_GAMEPAD_STARWARSVR        2

enum GAME_MODE {
	QUEST_PINBALL_FX_VR = 0,
	PC_VISUAL_PINBALL = 1,
	GAMPEPAD_STAR_WARS_VR = 2

};

int getControllerMode();

void saveControllerMode(int);

void gotoNextMode(int);

void setBLEMACAddress(int);