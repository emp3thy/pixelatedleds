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

uint16_t timeDelay[] = {20, 20, 100, 
                        100, 50, 50, 
                        20, 100, 50,
                        30, 50, 20, 
                        20, 100};
void loop()
{
  if (selectedEffect > 10 || selectedEffect < 0)
  {
    selectedEffect = 0;
    EEPROM.write(1, selectedEffect);
  }
  
  EVERY_N_MILLISECONDS(timeDelay[selectedEffect])
  {
    switch(selectedEffect)
    {
      case 0:
      pride();
      break;
      case 1:
      pacifica_loop();
      break;
      case 2:
      metaBalls();
      break;
      case 3: 
      make_fire();
      break;
      case 4:
      lavaNoise();
      break;
      case 5:
      fireNoise();
      break;
      case 6:
      rainbow();
      gHue++;
      break;
      case 7:
      justWhite();
      break;
      case 8:
      partyNoise();
      break;
      case 9:
      changepattern();
      break;
      case 10:
      rainbowStripeNoise();
      break;
      case 11:
      bpm();
      gHue++;
      break;
      case 12:
      rainbowWithGlitter();
      gHue++;
      break;
      case 13:
      jusBlack();
      break;
    }
  }
}
