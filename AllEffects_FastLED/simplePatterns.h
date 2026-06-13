#pragma once

#include <FastLED.h>

CRGBPalette16 currentPalette = PartyColors_p;
CRGBPalette16 targetPalette;
TBlendType    currentBlending = LINEARBLEND;   

void movePaletteToPalette()
{
  uint8_t maxChanges = 24;
    nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);    // AWESOME palette blending capability.

}

void generateRandomTargetPalette()
{
   uint8_t baseC = random8(255);                                             // Use the built-in random number generator as we are re-initializing the FastLED one.
    targetPalette = CRGBPalette16(CHSV(baseC+random8(0,32), 255, random8(128, 255)), CHSV(baseC+random8(0,32), 255, random8(128, 255)), CHSV(baseC+random8(0,32), 192, random8(128, 255)), CHSV(baseC+random8(0,32), 255, random8(128, 255)));
}

void fadeIn() {

  random16_set_seed(535);                                                           // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

  for (int i = 0; i<NUM_LEDS; i++) {
    uint8_t fader = sin8(millis()/random8(10,20));                                  // The random number for each 'i' will be the same every time.
    leds[i] = ColorFromPalette(currentPalette, i*20, fader, currentBlending);       // Now, let's run it through the palette lookup.
  }

  random16_set_seed(millis());                                                      // Re-randomizing the random number seed for other routines.

  FastLED.show();

} // fadein()

void rainbow()
{
  // Diagonal spatial rainbow: hue varies with BOTH column and height so each
  // strip shows a moving gradient offset from its neighbours (flowing diagonal
  // bands). Mapped through XY() — never fill leds[] in raw array order, which is
  // lane-major + serpentine and renders as stripes.
  for (uint8_t x = 0; x < NUM_COLS; x++)
  {
    for (uint8_t y = 0; y < NUM_ROWS; y++)
    {
      uint8_t hue = gHue + x * 6 + y * 4; // x,y contributions; uint8_t wraps the wheel
      leds[XY(x, y)] = CHSV(hue, 255, 255);
    }
  }
  FastLED.show();
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void fill_grad() {

  uint8_t starthue = beatsin8(2, 0, 255);
  uint8_t endhue = beatsin8(15, 0, 255);

  // Vertical gradient: interpolate start->end hue over the height, same across
  // all columns. Mapped through XY() — fill_gradient on raw leds[] would stripe.
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    uint8_t hue = starthue + (uint8_t)(((int16_t)endhue - (int16_t)starthue) * y / (NUM_ROWS - 1));
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      leds[XY(x, y)] = CHSV(hue, 255, 255);
    }
  }
  FastLED.show();

}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
  FastLED.show();
}


void justWhite()
{
  for (int i = 0; i < NUM_LEDS; i++ ) {
    leds[i] = CHSV(0, 0, 255);   // full-white pixel; global setBrightness() dims it
  }
  FastLED.show();
}

void jusBlack()
{
  for (int i = 0; i < NUM_LEDS; i++ ) {
    leds[i] = CHSV(254, 254, 0);
  }
  FastLED.show();
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  FastLED.show();
}

void sinelon()
{
  // a colored vertical bar sweeping back and forth across the columns, trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  uint8_t x = beatsin16( 13, 0, NUM_COLS - 1 );
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    leds[XY(x, y)] += CHSV( gHue, 255, 192);
  }
  FastLED.show();
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  // Vertical palette gradient pulsing with the beat; identical across columns.
  for ( uint8_t y = 0; y < NUM_ROWS; y++) {
    CRGB c = ColorFromPalette(palette, gHue + (y * 2), beat - gHue + (y * 10));
    for ( uint8_t x = 0; x < NUM_COLS; x++) {
      leds[XY(x, y)] = c;
    }
  }
  FastLED.show();
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  // eight vertical bars sweeping across the columns at different rates
  for ( uint8_t i = 0; i < 8; i++) {
    uint8_t x = beatsin16( i + 7, 0, NUM_COLS - 1 );
    for (uint8_t y = 0; y < NUM_ROWS; y++) {
      leds[XY(x, y)] |= CHSV(dothue, 200, 255);
    }
    dothue += 32;
  }
  FastLED.show();
}
