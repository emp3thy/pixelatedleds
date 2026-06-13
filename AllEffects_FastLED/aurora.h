#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

// Aurora borealis: drifting vertical curtains of light on a dark sky, colour
// shifting with height — green up high fading to violet/magenta lower down —
// like real northern-lights photos. Sine-based so the curtains are smooth and
// reliably structured (each column has a coherent vertical ray).
void aurora() {
  uint16_t t = millis();
  for (uint8_t x = 0; x < NUM_COLS; x++) {
    // Ray strength for this column: two slow sine waves drifting sideways at
    // different rates → smooth vertical bands that wander over time.
    uint8_t rayv = (uint8_t)(((uint16_t)sin8(x * 14 + t / 15) +
                              (uint16_t)sin8(x * 8  - t / 22)) / 2);
    for (uint8_t y = 0; y < NUM_ROWS; y++) {
      // Shimmer travelling up the curtain (slowed).
      uint8_t shimmer = sin8(y * 5 + x * 3 + t / 8);
      uint8_t factor  = qadd8(120, shimmer >> 1);   // 120..183
      uint8_t b       = scale8(rayv, factor);       // ray dimmed by shimmer
      b               = qadd8(b, b >> 2);           // +25% light
      uint8_t bri     = (b < 45) ? 0 : b;           // dark sky between curtains
      // Hue: green (96) at top → violet/magenta (200) toward the bottom.
      uint8_t hue = 96 + (uint16_t)y * (200 - 96) / (NUM_ROWS - 1);
      leds[XY(x, y)] = CHSV(hue, 235, bri);
    }
  }
  // Sparse faint stars on the dark sky.
  if (random8() < 30) {
    leds[XY(random8(NUM_COLS), random8(NUM_ROWS))] += CRGB(30, 30, 45);
  }
  FastLED.show();
}
