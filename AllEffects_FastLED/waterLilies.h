#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

// Claude Monet, "Water Lilies" (1897–99) — a light/pastel impression of the
// pond. Shimmering blue-green-lavender water comes from two layers of Perlin
// noise mapped through a soft Monet palette; a handful of green lily pads drift
// slowly on the surface, some carrying a pink/white flower dab with a warm
// yellow heart. "Light" = the palette is lifted toward pastels, no deep darks.

// 16-stop gradient: deep teal → blue-green → soft sage → pale lavender →
// dusty pink → cream. Brighter entries dominate so the water reads airy.
static const CRGBPalette16 waterLiliesPalette(
  CRGB(0x16465A), CRGB(0x1E5A6E), CRGB(0x2C7382), CRGB(0x3E8C8C),
  CRGB(0x5BA08C), CRGB(0x86B89A), CRGB(0xB0C9B4), CRGB(0xC9D2C6),
  CRGB(0xB9C0D6), CRGB(0xC9BFD8), CRGB(0xDCC3D2), CRGB(0xE6C6CE),
  CRGB(0xEAD2CE), CRGB(0xF0E2D2), CRGB(0xF4ECDC), CRGB(0xE9F0E6)
);

struct LilyPad {
  int16_t cx, cy;   // centre in 1/16-pixel fixed point (smooth slow drift)
  uint8_t rx, ry;   // ellipse radii in pixels
  int8_t  vx, vy;   // drift velocity in 1/16-pixel per update
  bool    flower;   // does this pad carry a flower?
  uint8_t fkind;    // 0 = pink, 1 = white
};

#define NUM_LILY_PADS 6
static LilyPad lilyPads[NUM_LILY_PADS];
static bool lilyPadsInit = false;

static void waterLiliesInitPads() {
  for (uint8_t i = 0; i < NUM_LILY_PADS; i++) {
    lilyPads[i].cx = random16(NUM_COLS * 16);
    lilyPads[i].cy = random16(NUM_ROWS * 16);
    lilyPads[i].rx = random8(5, 10);
    lilyPads[i].ry = random8(4, 8);
    lilyPads[i].vx = (int8_t)random8(3) - 1;   // -1..+1
    lilyPads[i].vy = (int8_t)random8(3) - 1;
    lilyPads[i].flower = random8() < 165;       // ~65% carry a flower
    lilyPads[i].fkind  = random8() < 130 ? 0 : 1;
  }
  lilyPadsInit = true;
}

static void waterLiliesDrawPads() {
  for (uint8_t i = 0; i < NUM_LILY_PADS; i++) {
    LilyPad &p = lilyPads[i];
    p.cx += p.vx; p.cy += p.vy;                 // drift + wrap around the canvas
    if (p.cx < 0) p.cx += NUM_COLS * 16; else if (p.cx >= NUM_COLS * 16) p.cx -= NUM_COLS * 16;
    if (p.cy < 0) p.cy += NUM_ROWS * 16; else if (p.cy >= NUM_ROWS * 16) p.cy -= NUM_ROWS * 16;

    int16_t ccx = p.cx >> 4, ccy = p.cy >> 4;
    CRGB pad = CRGB(64, 120, 70);              // soft lily-pad green

    for (int16_t dx = -(int16_t)p.rx; dx <= p.rx; dx++) {
      for (int16_t dy = -(int16_t)p.ry; dy <= p.ry; dy++) {
        int16_t x = ccx + dx, y = ccy + dy;
        if (x < 0 || x >= NUM_COLS || y < 0 || y >= NUM_ROWS) continue;
        // Normalised ellipse field: 0 at centre, ~100 at the rim.
        int32_t d = (int32_t)dx * dx * 100 / (p.rx * p.rx)
                  + (int32_t)dy * dy * 100 / (p.ry * p.ry);
        if (d > 110) continue;                  // just outside → skip (soft rim)
        uint8_t amt = (d >= 100) ? 70 : (uint8_t)(210 - d * 2); // softer toward edge
        nblend(leds[XY(x, y)], pad, amt);
      }
    }

    if (p.flower) {
      CRGB fl = (p.fkind == 0) ? CRGB(230, 150, 185) : CRGB(245, 238, 228);
      nblend(leds[XY(ccx, ccy)], fl, 235);
      if (ccx + 1 < NUM_COLS) nblend(leds[XY(ccx + 1, ccy)], fl, 150);
      if (ccx - 1 >= 0)       nblend(leds[XY(ccx - 1, ccy)], fl, 150);
      if (ccy + 1 < NUM_ROWS) nblend(leds[XY(ccx, ccy + 1)], fl, 150);
      if (ccy - 1 >= 0)       nblend(leds[XY(ccx, ccy - 1)], fl, 150);
      nblend(leds[XY(ccx, ccy)], CRGB(250, 225, 140), 110); // warm yellow heart
    }
  }
}

void waterLilies() {
  if (!lilyPadsInit) waterLiliesInitPads();

  uint16_t t  = millis();
  uint16_t tw = t / 6;     // slow water drift

  for (uint8_t x = 0; x < NUM_COLS; x++) {
    for (uint8_t y = 0; y < NUM_ROWS; y++) {
      // Two noise layers at different scales/directions → organic ripples.
      uint8_t n1 = inoise8((uint16_t)x * 22, (uint16_t)y * 22, tw);
      uint8_t n2 = inoise8((uint16_t)x * 11 + 1000, (uint16_t)y * 11 - tw, tw / 2);
      // Weighted blend biased bright so the pond stays light and airy.
      uint8_t v = qadd8(scale8(n1, 180), scale8(n2, 90));
      leds[XY(x, y)] = ColorFromPalette(waterLiliesPalette, v, 255, LINEARBLEND);
    }
  }

  waterLiliesDrawPads();
  FastLED.show();
}
