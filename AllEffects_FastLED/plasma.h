#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

void plasma() {
  uint16_t t = millis() / 16;
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      uint8_t v =
        sin8(x * 16 + t) +
        sin8(y * 32 + t * 2) +
        sin8((x + y) * 12 + t);
      uint8_t idx = v / 3;
      leds[XY(x, y)] = ColorFromPalette(RainbowColors_p, idx, 255, LINEARBLEND);
    }
  }
  FastLED.show();
}
