#include <FastLED.h>

uint16_t XY(uint8_t x, uint8_t y)
{
  if (ISMATRIX)
  {
    if ((x % 2) == 0)
    {
      return x * NUM_ROWS + y;
    }
    return x * NUM_ROWS + (COL_OFFSET - y);
  }
  return x * 10 + y;
}