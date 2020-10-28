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
//end patterns
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

  EVERY_N_MILLISECONDS(100)
  {
    switch (selectedEffect)
    {
    case 2:
      metaBalls();
      break;
    case 7:
      updaterain();
      break;
    case 3:
      make_fire();
      break;
    default:
      break;
    }
  }

  EVERY_N_MILLISECONDS(30)
  {
    switch (selectedEffect)
    {
    case 7:
      changepattern();
      break;
    default:
      break;
    }
  }
  EVERY_N_MILLISECONDS(20)
  {
    switch (selectedEffect)
    {
    case 6:
      justWhite();
      break;
    case 1:
      pacifica_loop();
      break;
    default:
      break;
    }
    FastLED.show();
  }
  if (selectedEffect > 10 || selectedEffect < 0)
  {
    selectedEffect = 0;
    EEPROM.write(1, selectedEffect);
  }
  EVERY_N_MILLISECONDS(50)
  {
    gHue++;
    switch (selectedEffect)
    {
    case 5:
      rainbow();
      break;
    case 10:
      rainbowWithGlitter();
      break;
    case 4:
      confetti();
      break;
    case 8:
      sinelon();
      break;
    case 9:
      bpm();
      break;
    default:
      break;
    }
  }
  if (selectedEffect == 0)
  {
    pride();
  }
}
