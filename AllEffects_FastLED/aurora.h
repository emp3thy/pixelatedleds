#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

void aurora() {
  uint16_t t = millis() / 12;
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    // row 0 brightest, falls off toward bottom row
    uint8_t rowBri = 255 - ((uint16_t)y * 220 / (NUM_ROWS - 1));
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      // horizontal shimmer band — two sines summed and scaled
      uint16_t bandSum = (uint16_t)sin8(x * 5 + t) + (uint16_t)sin8(x * 3 - t / 2);
      uint8_t band = bandSum / 2;
      // hue: saturating add keeps aurora in green→violet range, no red wrap
      uint8_t hue = qadd8(96, (band >> 1) + (y * 8));
      // brightness: combine row falloff with shimmer
      uint8_t bri = scale8(rowBri, qadd8(band, 40));
      leds[XY(x, y)] = CHSV(hue, 220, bri);
    }
  }
  // sparse violet shimmer on top two rows
  if (random8() < 18) {
    uint8_t sx = random8(NUM_COLS);
    uint8_t sy = random8(2);
    leds[XY(sx, sy)] = CRGB(220, 100, 255);
  }
  FastLED.show();
}
