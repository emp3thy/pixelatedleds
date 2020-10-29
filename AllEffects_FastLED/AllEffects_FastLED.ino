#include <FastLED.h>
#include <EEPROM.h>
#define NUM_LEDS 100
CRGB leds[NUM_LEDS];
#define PIN 5

#define BUTTON 2
#define BRIGHTNESS 254
#define NUM_ROWS 10
#define NUM_COLS 10

byte selectedEffect = -1;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

//Patterns
#include "effectChanging.h"
#include "XY.h"
#include "metaBalls.h"
#include "simplePatterns.h"
#include "pacifica.h"
#include "pride.h"
#include "rain.h"
#include "fire.h"
#include "noise.h"
//end patterns
typedef void (*SimplePatternList[])();
SimplePatternList patterns = {pride, pacifica_loop, metaBalls, 
                              make_fire, lavaNoise, fireNoise, 
                              rainbow, justWhite, partyNoise,
                              changepattern, rainbowStripeNoise, bpm, 
                              rainbowWithGlitter, jusBlack};

uint16_t timeDelay[] = {20, 20, 100, 
                        100, 50, 50, 
                        20, 100, 50,
                        30, 50, 20, 
                        20, 100};

void setup()
{
  delay(3000); // 3 second delay for boot recovery, and a moment of silence
  FastLED.addLeds<WS2811, PIN, GRB>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  pinMode(2, INPUT_PULLUP);                                             // internal pull-up resistor
  attachInterrupt(digitalPinToInterrupt(BUTTON), changeEffect, CHANGE); // pressed
  FastLED.setBrightness(BRIGHTNESS);
  rainInit();
  selectedEffect = EEPROM.read(1);
}

void loop()
{
  if (selectedEffect > 10 || selectedEffect < 0)
  {
    selectedEffect = 0;
    EEPROM.write(1, selectedEffect);
  }

  EVERY_N_MILLISECONDS(timeDelay[selectedEffect])
  {
    patterns[selectedEffect]();
  }
}
