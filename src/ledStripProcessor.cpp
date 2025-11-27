#include "ledStripProcessor.hpp"

Adafruit_NeoPixel pixels(NUM_STRIP_LEDS, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

void setLEDStrip(int mode){
	pixels.begin();
	pixels.setBrightness(100);
	pixels.fill(gameModeColors[mode]);
	pixels.show();
}