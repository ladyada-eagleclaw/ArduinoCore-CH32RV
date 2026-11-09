#pragma once
#include <Arduino.h>
/** Write the Metro's single PC14 NeoPixel. Interrupts pause for about 30 us. */
void metroNeoPixelWrite(uint8_t red, uint8_t green, uint8_t blue);
