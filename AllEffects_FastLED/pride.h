#pragma once

#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"
//*** - Pride rainbows
void pride()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 72, 168);   // lower depth -> higher floor -> brighter
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  // Traverse spatially (column-major, bottom-up) and write through XY() so the
  // pride flow is smooth on the tower. Writing leds[] in raw array order scrambles
  // under the lane-major/serpentine layout.
  for ( uint8_t x = 0; x < NUM_COLS; x++) {
    for ( uint8_t y = 0; y < NUM_ROWS; y++) {
      hue16 += hueinc16;
      uint8_t hue8 = hue16 / 256;

      brightnesstheta16  += brightnessthetainc16;
      uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

      uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
      uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
      bri8 += (255 - brightdepth);

      CRGB newcolor = CHSV( hue8, sat8, bri8);

      nblend( leds[XY(x, y)], newcolor, 96);
    }
  }
  blur2d(leds, NUM_COLS, NUM_ROWS, 32);   // soften the rainbow bands
  FastLED.show();
}