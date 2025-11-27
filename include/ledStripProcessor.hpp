#pragma once

#include <Adafruit_NeoPixel.h>

#define PIN_LED_STRIP          13
#define NUM_STRIP_LEDS         50

const uint32_t gameModeColors[] = {
	0x0000FF,  // Blue
	0xFF00FF   // Purple/Magenta
};

void setLEDStrip(int);
