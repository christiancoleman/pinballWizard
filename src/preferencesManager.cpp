#include "preferencesManager.hpp"

Preferences preferences;

int getControllerMode(){
	preferences.begin("pb", false);
	int wasSaved = preferences.getInt("wiz");
	preferences.end();
	if(wasSaved > 1 || wasSaved < 0) {
		wasSaved = 0;  // Safety check
		saveControllerMode(0);
	}
	Serial.print("getControllerMode(): ");
	Serial.println(wasSaved);
	return wasSaved;
}

void saveControllerMode(int mode){
	preferences.begin("pb", false);
	preferences.putInt("wiz", mode);
	preferences.end();
	Serial.print("saveControllerMode with ");
	Serial.println(mode);
}

void gotoNextMode(int mode){
	Serial.println("Calling gotoNextMode()");
	Serial.print("Saving the following as the next mode: ");
	Serial.println(mode);
	if(mode == 0){
		mode = 1;
	} else if(mode == 1){
		mode = 0;
	} else {
		Serial.println("huh?");
	}
	saveControllerMode(mode);
}