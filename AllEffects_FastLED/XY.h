#include <FastLED.h>

uint16_t XY (uint8_t x, uint8_t y) {
  if ( (x % 2) == 0)
  {
    return x * 10 + y;
  }
  return x * 10 + (9 - y);
}