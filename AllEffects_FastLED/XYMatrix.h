#pragma once
#include <FastLED.h>
#include "configuration.h"

uint16_t XY(uint8_t x, uint8_t y)
{
  if (ISMATRIX)
  {
    if ((x % 2) == 0)
    {
      return x * NUM_ROWS + y;
    }
    return x * NUM_ROWS + (NUM_ROWS - 1 - y);
  }
  return x * 10 + y;
}
