#pragma once
#include <FastLED.h>

byte dist (uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)  {
  int a = y2 - y1;
  int b = x2 - x1;
  a *= a;
  b *= b;
  uint8_t r = sqrt16(a + b);
  // r==0 when the pixel sits exactly on the ball centre. 220/0 is undefined on
  // AVR and traps ("divide by zero") under WASM/emscripten — clamp to max here.
  byte dist = r ? (220 / r) : 220;
  return dist;
}
void metaBalls()
{
  uint8_t bx1 = beatsin8(15, 0, NUM_COLS - 1, 0, 0);
  uint8_t by1 = beatsin8(18, 0, NUM_ROWS - 1, 0, 0);
  uint8_t bx2 = beatsin8(28, 0, NUM_COLS - 1, 0, 4);
  uint8_t by2 = beatsin8(23, 0, NUM_ROWS - 1, 0, 4);
  uint8_t bx3 = beatsin8(30, 0, NUM_COLS - 1, 0, 8);
  uint8_t by3 = beatsin8(24, 0, NUM_ROWS - 1, 0, 8);
  uint8_t bx4 = beatsin8(17, 0, NUM_COLS - 1, 0, 16);
  uint8_t by4 = beatsin8(25, 0, NUM_ROWS - 1, 0, 16);
  uint8_t bx5 = beatsin8(19, 0, NUM_COLS - 1, 0, 22);
  uint8_t by5 = beatsin8(21, 0, NUM_ROWS - 1, 0, 22);

  for (int i = 0; i < NUM_COLS; i++)    {
    for (int j = 0; j < NUM_ROWS; j++) {

      byte  sum =  dist(i, j, bx1, by1);
      sum = qadd8(sum, dist(i, j, bx2, by2));
      sum = qadd8(sum, dist(i, j, bx3, by3));
      sum = qadd8(sum, dist(i, j, bx4, by4));
      sum = qadd8(sum, dist(i, j, bx5, by5));

      // Use the field value directly (small lift) as the heat index. The old
      // "+165" bias shoved everything into the pale white top of HeatColors,
      // which read as washed-out; this keeps saturated reds/oranges with bright cores.
      leds[XY (i, j)] =  ColorFromPalette(HeatColors_p, qadd8(sum, 40), 255);
    }
  }

  blur2d(leds, NUM_COLS, NUM_ROWS, 20 );
  FastLED.show();
}
