#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// Aurora borealis: several independent ribbon "curtains" of light on a dark sky.
// Each curtain has its own width, vertical extent, sideways bend (sway), hue and
// a languid wandering drift — so they all look different and wander randomly
// rather than scrolling together. Colour shifts green→violet down the ribbon.
#define AURORA_CURTAINS 7

struct AuroraCurtain {
  float   cx;        // horizontal centre (drifts)
  float   vx;        // drift velocity (sign = direction)
  float   halfw;     // half-width → fatter/thinner ribbons
  float   swayAmp;   // how far the ribbon bends side to side
  float   swayFreq;  // vertical wave density of the bend
  float   swayPhase; // animates the bend
  float   swaySpeed; // bend animation rate
  float   yTop, yBot;// vertical extent → tall/short ribbons
  uint8_t hueBase;   // base colour
};

static AuroraCurtain auroraCurtains[AURORA_CURTAINS];
static bool auroraInited = false;

static void auroraInitCurtains() {
  for (uint8_t i = 0; i < AURORA_CURTAINS; i++) {
    AuroraCurtain &c = auroraCurtains[i];
    c.cx        = random8(NUM_COLS);
    c.vx        = ((int)random8(33) - 16) / 200.0f;        // ~-0.08..0.08 px/frame
    if (fabsf(c.vx) < 0.015f) c.vx = 0.03f;
    c.halfw     = 6.0f + random8(130) / 10.0f;             // 6..19 (fatter ribbons)
    c.swayAmp   = 2.0f + random8(70) / 10.0f;              // 2..9
    c.swayFreq  = 0.05f + random8(14) / 100.0f;            // 0.05..0.19
    c.swayPhase = random8() / 40.0f;
    c.swaySpeed = 0.008f + random8(22) / 1000.0f;          // 0.008..0.030
    c.yTop      = random8((uint8_t)(NUM_ROWS * 0.18f));    // taller — less black
    c.yBot      = NUM_ROWS - 1 - random8((uint8_t)(NUM_ROWS * 0.18f));
    if (c.yBot - c.yTop < 14) c.yBot = c.yTop + 14;        // keep a usable height
    c.hueBase   = 80 + random8(130);                       // green..violet/magenta
  }
  auroraInited = true;
}

void aurora() {
  if (!auroraInited) auroraInitCurtains();

  // Languid drift + animate the bends; occasionally wander a new direction.
  for (uint8_t i = 0; i < AURORA_CURTAINS; i++) {
    AuroraCurtain &c = auroraCurtains[i];
    c.cx += c.vx;
    if (c.cx < 0)            { c.cx = 0;            c.vx = -c.vx; }
    if (c.cx > NUM_COLS - 1) { c.cx = NUM_COLS - 1; c.vx = -c.vx; }
    c.swayPhase += c.swaySpeed;
  }
  EVERY_N_SECONDS(9) {
    for (uint8_t i = 0; i < AURORA_CURTAINS; i++)
      auroraCurtains[i].vx = (random8(20) + 6) / 200.0f * (random8(2) ? 1 : -1);
  }

  uint16_t t = millis();
  float    center[AURORA_CURTAINS], vEnv[AURORA_CURTAINS];

  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    // Per-row: each curtain's swaying centre and vertical brightness envelope.
    for (uint8_t i = 0; i < AURORA_CURTAINS; i++) {
      AuroraCurtain &c = auroraCurtains[i];
      center[i] = c.cx + c.swayAmp * sinf(y * c.swayFreq + c.swayPhase);
      if (y < c.yTop || y > c.yBot) { vEnv[i] = 0.0f; continue; }
      float vpos = (y - c.yTop) / (c.yBot - c.yTop);        // 0..1
      vEnv[i] = 0.45f + 0.55f * sinf(vpos * 3.14159f);      // raised floor → fuller body
    }
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      uint8_t shimmer = sin8(y * 6 + x * 3 + t / 18);       // languid shimmer
      float   sh = 0.6f + (shimmer / 255.0f) * 0.4f;
      // Additive accumulation across curtains → thicker, brighter, colours mix.
      uint16_t r = 4, g = 14, b = 12;                       // faint ambient glow (no pure black)
      for (uint8_t i = 0; i < AURORA_CURTAINS; i++) {
        if (vEnv[i] <= 0.0f) continue;
        float dxc = fabsf(x - center[i]);
        if (dxc >= auroraCurtains[i].halfw) continue;
        float horiz = 1.0f - dxc / auroraCurtains[i].halfw; // linear → fat, not pinched
        float bb = horiz * vEnv[i] * sh;                    // 0..1
        float vpos = (y - auroraCurtains[i].yTop) /
                     (auroraCurtains[i].yBot - auroraCurtains[i].yTop);
        uint8_t hue = auroraCurtains[i].hueBase + (uint8_t)(vpos * 60.0f);
        CRGB cc = CHSV(hue, 225, (uint8_t)(bb > 1.0f ? 255 : bb * 255.0f));
        r += cc.r; g += cc.g; b += cc.b;
      }
      leds[XY(x, y)] = CRGB(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
    }
  }

  // Sparse faint stars on the dark sky.
  if (random8() < 30) {
    leds[XY(random8(NUM_COLS), random8(NUM_ROWS))] += CRGB(30, 30, 45);
  }
  FastLED.show();
}
