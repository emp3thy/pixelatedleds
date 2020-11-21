#include <bitswap.h>
#include <chipsets.h>
#include <color.h>
#include <colorpalettes.h>
#include <colorutils.h>
#include <controller.h>
#include <cpp_compat.h>
#include <dmx.h>
#include <fastled_config.h>
#include <fastled_delay.h>
#include <fastled_progmem.h>
#include <FastLED.h>
#include <fastpin.h>
#include <fastspi_bitbang.h>
#include <fastspi_dma.h>
#include <fastspi_nop.h>
#include <fastspi_ref.h>
#include <fastspi_types.h>
#include <fastspi.h>
#include <hsv2rgb.h>
#include <led_sysdefs.h>
#include <lib8tion.h>
#include <noise.h>
#include <pixelset.h>
#include <pixeltypes.h>
#include <platforms.h>
#include <power_mgt.h>

#include <FastLED.h>
#include <avr/pgmspace.h>
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
  pinMode(2, INPUT_PULLUP); // internal pull-up resistor
  //attachInterrupt(digitalPinToInterrupt(BUTTON), changeEffect, CHANGE); // pressed
  FastLED.setBrightness(BRIGHTNESS);
  rainInit();
  selectedEffect = 0;
}

void loop()
{
  selectedEffect = convertToSelectedEffect(analogRead(BUTTON));

  EVERY_N_MILLISECONDS(50)
  {
    switch (selectedEffect)
    {
    case 6:
      rainbow();
      gHue++;
      break;
    default:
      break;
    }
  }
  EVERY_N_MILLISECONDS(30)
  {
    switch (selectedEffect)
    {
    case 9:
      changepattern();
      break;
    default:
      break;
    }
  }
EVERY_N_MILLISECONDS(150)
{
  switch (selectedEffect)
    {
      case 10:
      rainbowStripeNoise();
      break;
    case 13:
      jusBlack();
      break;
    default:
      break;
    }
}
  EVERY_N_MILLISECONDS(100)
  {
    switch (selectedEffect)
    {
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
    case 7:
      justWhite();
      break;
    case 8:
      partyNoise();
      break;
       default:
      break;
    }
  }

  EVERY_N_MILLISECONDS(20)
  {
    switch (selectedEffect)
    {
    case 0:
      pride();
      break;
    case 1:
      pacifica_loop();
      break;
    case 9:
      changepattern();
      break;
    case 11:
      bpm();
      gHue++;
      break;
    case 12:
      rainbowWithGlitter();
      gHue++;
      break;
    default:
      break;
    }
  }
}
