#include <FastLED.h>
#include "configuration.h"

CRGB leds[NUM_LEDS];
#define PIN 5
#define BUTTON 2
#define BRIGHTNESS 64   // sim: lower than hardware 255 so dense matrix doesn't bloom to white



byte selectedEffect = 0;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

//Patterns
#include "effectChanging.h"
#include "XYMatrix.h"
#include "metaBalls.h"
#include "simplePatterns.h"
#include "pacifica.h"
#include "pride.h"
#include "rain.h"
#include "fire.h"
#include "noisePatterns.h"
#include "mondrian.h"
#include "plasma.h"
#include "aurora.h"
#include "voronoi.h"
#include "waterLilies.h"
#include "kusamaDots.h"
#include "oceanSunrise.h"
#include "farmhouseSeasons.h"
#include "lightCycleRace.h"
#include "fireworks.h"
//end patterns

// --- WASM-sim-only additions (not part of the Teensy/FastLED-3.3.3 build) ---
// On-screen slider replaces the hardware potentiometer. 0..1023 mirrors the
// analogRead() range; step 57 = one notch per pattern (convertToSelectedEffect
// divides by 57). Drag it in the browser to cycle all patterns.
fl::UISlider effectSlider("Pattern (0-23)", 0, 0, 1311, 57);

// WASM-sim-only export stub. The FastLED wasm linker is configured to export
// `sim_set_pattern`, so the symbol must exist for the sim to link; it's a
// vestigial hook (the viewer drives effects via the UISlider/processUiInput, so
// g_simPattern is intentionally unused). Guarded so the Teensy/AVR hardware
// build — which has no <emscripten.h> — still compiles.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern "C" {
  int g_simPattern = 0;
  EMSCRIPTEN_KEEPALIVE void sim_set_pattern(int v) { g_simPattern = v; }
}
#endif

// Adapter: the screenmap needs the XYFunction signature; forward to XY().
static uint16_t simXYFunc(uint16_t x, uint16_t y, uint16_t, uint16_t) { return XY(x, y); }

void setup()
{
  delay(3000); // 3 second delay for boot recoverye
  FastLED.addLeds<WS2811, PIN, GRB>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  // Describe the physical 44x73 grid to the WASM renderer (matches XY()).
  // Without this the renderer falls back to a degenerate linear map (divide by zero).
  FastLED[0].setScreenMap(
      XYMap::constructWithUserFunction(NUM_COLS, NUM_ROWS, simXYFunc), 1.0f);
  pinMode(2, INPUT_PULLUP); // internal pull-up resistor
  FastLED.setBrightness(BRIGHTNESS);
  rainInit();
  selectedEffect = 0;
}

void loop()
{
  selectedEffect = convertToSelectedEffect(effectSlider.as_int()); // viewer sets via processUiInput({"Pattern (0-19)":v})

  EVERY_N_MILLISECONDS(50)
  {
    switch (selectedEffect)
    {
    case 6:
      rainbow();
      gHue++;
      break;
    case 20:
      plasma();
      break;
    case 22:
      voronoi();
      break;
    case 23:
      waterLilies();
      break;
    case 24:
      kusamaDots();
      break;
    case 25:
      oceanSunrise();
      break;
    case 26:
      farmhouseSeasons();
      break;
    case 27:
      lightCycleRace();
      break;
    case 28:
      fireworks();
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
    case 3:
      make_fire();
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
    case 17:
      movePaletteToPalette();
      break;
    case 19:
      mondrian();
      break;
    default:
      break;
    }
  }
  EVERY_N_SECONDS(5)
  {
    switch (selectedEffect)
    {
    case 17:
      generateRandomTargetPalette();
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
    case 11:
      bpm();
      gHue++;
      break;
    case 12:
      rainbowWithGlitter();
      gHue++;
      break;
    case 14:
      juggle();
      break;
    case 15:
      sinelon();
      break;
    case 16:
      confetti();
      break;
      case 17:
      fadeIn();
      break;
      case 18:
      fill_grad();
      break;
    case 21:
      aurora();
      break;
    default:
      break;
    }
  }
}
