#pragma once
#include <FastLED.h>

byte dist (uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)  {
  int a = y2 - y1;
  int b = x2 - x1;
  a *= a;
  b *= b;
  uint8_t r = sqrt16(a + b);
  // r==0 when the pixel sits exactly on the ball centre. 160/0 is undefined on
  // AVR and traps ("divide by zero") under WASM/emscripten — clamp to max here.
  // Numerator lowered (was 220) so more balls don't saturate the field to white.
  byte dist = r ? (160 / r) : 160;
  return dist;
}

// More balls, slower drift. beatsin8 BPMs are low (5..11) so the blobs ooze.
#define NUM_METABALLS 9

void metaBalls()
{
  static const uint8_t bpmX[NUM_METABALLS]  = { 7, 9, 6, 8, 5, 10, 7, 9, 6 };
  static const uint8_t bpmY[NUM_METABALLS]  = { 8, 6, 9, 5, 11, 7, 6, 8, 9 };
  static const uint8_t phase[NUM_METABALLS] = { 0, 4, 8, 16, 22, 30, 40, 50, 60 };

  uint8_t bx[NUM_METABALLS], by[NUM_METABALLS];
  for (uint8_t k = 0; k < NUM_METABALLS; k++) {
    bx[k] = beatsin8(bpmX[k], 0, NUM_COLS - 1, 0, phase[k]);
    by[k] = beatsin8(bpmY[k], 0, NUM_ROWS - 1, 0, (uint8_t)(phase[k] + 7));
  }

  for (int i = 0; i < NUM_COLS; i++)    {
    for (int j = 0; j < NUM_ROWS; j++) {
      byte sum = 0;
      for (uint8_t k = 0; k < NUM_METABALLS; k++)
        sum = qadd8(sum, dist(i, j, bx[k], by[k]));

      // Use the field value directly (small lift) as the heat index — keeps
      // saturated reds/oranges with bright cores instead of washed-out white.
      leds[XY (i, j)] = ColorFromPalette(HeatColors_p, qadd8(sum, 40), 255);
    }
  }

  blur2d(leds, NUM_COLS, NUM_ROWS, 20 );
  FastLED.show();
}
