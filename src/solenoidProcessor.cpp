#include "solenoidProcessor.hpp"

void sendLeftFlipperDataHigh(){
	digitalWrite(LEFT_SOLENOID, HIGH);
}

void sendRightFlipperDataHigh(){
	digitalWrite(RIGHT_SOLENOID, HIGH);
}

void sendLeftFlipperDataLow(){
	digitalWrite(LEFT_SOLENOID, LOW);
}

void sendRightFlipperDataLow(){
	digitalWrite(RIGHT_SOLENOID, LOW);
}