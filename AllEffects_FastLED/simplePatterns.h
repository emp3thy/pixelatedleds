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

} // fadein()

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
  FastLED.show();
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void fill_grad() {
  
  uint8_t starthue = beatsin8(5, 0, 255);
  uint8_t endhue = beatsin8(7, 0, 255);
  
  if (starthue < endhue) {
    fill_gradient(leds, NUM_LEDS, CHSV(starthue,255,255), CHSV(endhue,255,255), FORWARD_HUES);    // If we don't have this, the colour fill will flip around. 
  } else {
    fill_gradient(leds, NUM_LEDS, CHSV(starthue,255,255), CHSV(endhue,255,255), BACKWARD_HUES);
  }
  
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
    leds[i] = CHSV(0, 0, BRIGHTNESS);
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
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS - 1 );
  leds[pos] += CHSV( gHue, 255, 192);
  FastLED.show();
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
  FastLED.show();
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16( i + 7, 0, NUM_LEDS - 1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  FastLED.show();
}
