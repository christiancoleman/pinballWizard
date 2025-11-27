#include "preferencesManager.hpp"

Preferences preferences;

int getControllerMode(){
	preferences.begin("pb", false);
	int wasSaved = preferences.getInt("mode");
	preferences.end();
	if(wasSaved > 2 || wasSaved < 0) wasSaved = 0;  // Safety check
	Serial.print("getControllerMode(): ");
	Serial.println(wasSaved);
	return wasSaved;
}

void saveControllerMode(int mode){
	preferences.begin("pb", false);
	preferences.putInt("mode", mode);
	preferences.end();
	Serial.print("saveControllerMode with ");
	Serial.println(mode);
}

void gotoNextMode(int mode){
	Serial.println("Calling gotoNextMode()");
	Serial.print("Saving the following as the next mode: ");
	Serial.println(mode);
	mode = (mode + 1) % 3;
	saveControllerMode(mode);
}

void setBLEMACAddress(int mode){
	uint8_t newMAC[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFF, 0x00};
	newMAC[5] = 0x10 + mode;  // Different last byte for each mode
	esp_base_mac_addr_set(newMAC);
}