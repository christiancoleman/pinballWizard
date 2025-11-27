#include "preferencesManager.hpp"

Preferences preferences;

uint8_t getControllerMode(){
	preferences.begin("pinballWizard.v1", true);
	uint8_t saved = preferences.getUChar("gMode", 0);
	preferences.end();
	if(saved > 2 || saved < 0) saved = 0;  // Safety check
	Serial.print("getControllerMode(): " + saved);
	return saved;
}

void saveControllerMode(int mode){
	preferences.begin("pinballWizard.v1", false);
	preferences.putUChar("gMode", (uint8_t)mode);
	preferences.end();
	Serial.print("saveControllerMode with " + mode);
}
