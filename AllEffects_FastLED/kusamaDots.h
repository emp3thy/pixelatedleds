#pragma once
#include <FastLED.h>
#include <math.h>
#include <string.h>
#include "configuration.h"
#include "XYMatrix.h"

// Yayoi Kusama — red dots ("Dots" series). Dense pack of red shapes in many
// random sizes on a warm-white ground: round dots and ellipses only.
// Largest land first, smaller fill the gaps, each kept apart by a 1px margin.
//
// Rendering is anti-aliased via a per-pixel coverage buffer: shape edges fade by
// sub-pixel coverage, so as a dot's fractional centre drifts the edge shifts
// gradually instead of snapping a whole pixel at a time (no more "stunted"
// motion). A final blur2d softens everything. All dots share one red and
// coverage is max-combined, so overlaps merge cleanly.
#define KUSAMA_MAX_DOTS 5200
#define KUSAMA_SPLIT_R   4      // dots with radius >= this fission into smaller ones
#define KUSAMA_TARGET    1100   // stop spawning new big dots past this many

enum KusamaShape : uint8_t { KS_ELLIPSE = 0, KS_DIAMOND = 1 };

struct KusamaDot {
  int16_t cx, cy;   // centre in 1/16-pixel fixed point (smooth slow drift)
  uint8_t rx, ry;   // half-extents in pixels
  uint8_t shape;    // KusamaShape
  int8_t  vx, vy;   // drift velocity in 1/16-pixel per update
};
static KusamaDot kusamaDotsArr[KUSAMA_MAX_DOTS];
static uint16_t kusamaDotCount = 0;
static bool kusamaInit = false;
static uint8_t kusamaCov[NUM_LEDS];

// Bounding-circle collision (uses each shape's larger half-extent). +1px gap.
static bool kusamaOverlaps(uint8_t x, uint8_t y, uint8_t bound) {
  for (uint16_t i = 0; i < kusamaDotCount; i++) {
    int16_t dx = (int16_t)x - (kusamaDotsArr[i].cx >> 4);
    int16_t dy = (int16_t)y - (kusamaDotsArr[i].cy >> 4);
    uint8_t ob = max(kusamaDotsArr[i].rx, kusamaDotsArr[i].ry);
    int16_t md = (int16_t)bound + ob + 1;   // 1px white gap (dense like the reference)
    if (dx * dx + dy * dy < md * md) return true;
  }
  return false;
}

static void kusamaInitDots() {
  kusamaDotCount = 0;
  // Big tiers first so they anchor the field; smaller tiers fill the gaps.
  // Per-tier attempt budget rises sharply as size drops → few big dots, lots of
  // small ones.
  // Hard per-tier COUNT caps (not just attempts): greedy packing fills big dots
  // first on an empty canvas, so attempts alone can't limit them. Few big dots,
  // lots of small ones.
  static const uint8_t  tiers[]    = { 8, 7, 6, 5, 4, 3, 2, 1 };
  static const uint16_t caps[]     = { 1, 2, 4, 7, 14, 30, 150, 250 };
  static const uint16_t attempts[] = { 200, 300, 400, 700, 1200, 2500, 4000, 6000 };
  for (uint8_t ti = 0; ti < sizeof(tiers); ti++) {
    uint8_t r = tiers[ti];
    uint16_t placed = 0;
    for (uint16_t a = 0; a < attempts[ti] && placed < caps[ti] && kusamaDotCount < KUSAMA_MAX_DOTS; a++) {
      // All round: circles and ellipses (no diamonds/squares — they read blocky).
      uint8_t shape = KS_ELLIPSE;
      uint8_t rx = r;
      uint8_t ry = (random8() < 110) ? (uint8_t)max(1, r - (int)random8(0, 3)) : r; // sometimes squashed
      uint8_t bound = max(rx, ry);
      uint8_t x = random8(bound, NUM_COLS - bound);
      uint8_t y = random8(bound, NUM_ROWS - bound);
      if (kusamaOverlaps(x, y, bound)) continue;
      KusamaDot &d = kusamaDotsArr[kusamaDotCount++];
      d.cx = (int16_t)x << 4;
      d.cy = (int16_t)y << 4;
      d.rx = rx; d.ry = ry; d.shape = shape;
      d.vx = (int8_t)random8(3) - 1;   // -1..+1 (1/16 px) → very slow, less merging
      d.vy = (int8_t)random8(3) - 1;
      if (d.vx == 0 && d.vy == 0) d.vx = 1; // never fully static
      placed++;
    }
  }
  kusamaInit = true;
}

// Set a fresh slow random velocity on a dot.
static void kusamaRandVel(KusamaDot &d) {
  d.vx = (int8_t)random8(3) - 1;   // -1..+1 (1/16 px per update) → slow
  d.vy = (int8_t)random8(3) - 1;
  if (d.vx == 0 && d.vy == 0) d.vx = 1;
}

// Append one new big dot at a free spot (so the field keeps churning).
static void kusamaSpawn(uint8_t r) {
  for (uint16_t a = 0; a < 200 && kusamaDotCount < KUSAMA_MAX_DOTS; a++) {
    uint8_t x = random8(r, NUM_COLS - r), y = random8(r, NUM_ROWS - r);
    if (kusamaOverlaps(x, y, r)) continue;
    KusamaDot &d = kusamaDotsArr[kusamaDotCount++];
    d.cx = (int16_t)x << 4; d.cy = (int16_t)y << 4;
    d.rx = d.ry = r; d.shape = KS_ELLIPSE;
    kusamaRandVel(d);
    return;
  }
}

void kusamaDots() {
  if (!kusamaInit) kusamaInitDots();

  // Fission: any dot at/above the threshold breaks into 3 smaller dots that
  // scatter with fresh random velocities. Cascades down to small over a couple
  // generations (e.g. r6 → r4 → r2). The parent becomes the first child.
  EVERY_N_MILLISECONDS(1300) {
    uint16_t n = kusamaDotCount;
    for (uint16_t i = 0; i < n; i++) {
      KusamaDot &d = kusamaDotsArr[i];
      if (d.rx < KUSAMA_SPLIT_R) continue;
      uint8_t nr = d.rx - 2;
      int16_t pcx = d.cx, pcy = d.cy;
      d.rx = d.ry = nr; kusamaRandVel(d);
      for (uint8_t k = 0; k < 2 && kusamaDotCount < KUSAMA_MAX_DOTS; k++) {
        KusamaDot &c = kusamaDotsArr[kusamaDotCount++];
        c.cx = pcx + ((int16_t)random8((nr + 2) * 16) - (nr + 1) * 16);
        c.cy = pcy + ((int16_t)random8((nr + 2) * 16) - (nr + 1) * 16);
        if (c.cx < 0) c.cx += NUM_COLS << 4; else if (c.cx >= (NUM_COLS << 4)) c.cx -= NUM_COLS << 4;
        if (c.cy < 0) c.cy += NUM_ROWS << 4; else if (c.cy >= (NUM_ROWS << 4)) c.cy -= NUM_ROWS << 4;
        c.rx = c.ry = nr; c.shape = KS_ELLIPSE;
        kusamaRandVel(c);
      }
    }
  }
  // Occasionally introduce a fresh big dot so there's always something breaking.
  EVERY_N_SECONDS(3) {
    if (kusamaDotCount < KUSAMA_TARGET) kusamaSpawn(random8(5, 8)); // r5..7
  }

  memset(kusamaCov, 0, sizeof(kusamaCov));

  for (uint16_t i = 0; i < kusamaDotCount; i++) {
    KusamaDot &d = kusamaDotsArr[i];
    // Independent slow random drift + toroidal wrap.
    d.cx += d.vx; d.cy += d.vy;
    if (d.cx < 0) d.cx += NUM_COLS << 4; else if (d.cx >= (NUM_COLS << 4)) d.cx -= NUM_COLS << 4;
    if (d.cy < 0) d.cy += NUM_ROWS << 4; else if (d.cy >= (NUM_ROWS << 4)) d.cy -= NUM_ROWS << 4;

    float cxf = d.cx / 16.0f, cyf = d.cy / 16.0f;
    int16_t bx = (int16_t)lroundf(cxf), by = (int16_t)lroundf(cyf);
    float rx = d.rx, ry = d.ry;
    float soft = (rx + ry) * 0.5f;              // crisp edge (narrow fade band)

    for (int16_t dy = -d.ry - 1; dy <= d.ry + 1; dy++) {
      for (int16_t dx = -d.rx - 1; dx <= d.rx + 1; dx++) {
        float ddx = (bx + dx) - cxf;
        float ddy = (by + dy) - cyf;
        float e;
        if (d.shape == KS_DIAMOND) e = fabsf(ddx) / rx + fabsf(ddy) / ry;       // 1 at rim
        else                       e = sqrtf((ddx*ddx)/(rx*rx) + (ddy*ddy)/(ry*ry));
        float cov = 0.5f + (1.0f - e) * soft; // sub-pixel coverage, fades at edge
        if (cov <= 0.0f) continue;
        if (cov > 1.0f) cov = 1.0f;
        uint8_t c = (uint8_t)(cov * 255.0f);
        int16_t x = bx + dx; if (x < 0) x += NUM_COLS; else if (x >= NUM_COLS) x -= NUM_COLS;
        int16_t y = by + dy; if (y < 0) y += NUM_ROWS; else if (y >= NUM_ROWS) y -= NUM_ROWS;
        uint16_t idx = XY(x, y);
        if (c > kusamaCov[idx]) kusamaCov[idx] = c;   // max-combine overlaps
      }
    }
  }

  // Composite: warm-white ground, blend toward red by coverage.
  const CRGB ground(235, 232, 222);
  const CRGB red(200, 12, 16);
  for (uint16_t i = 0; i < NUM_LEDS; i++)
    leds[i] = kusamaCov[i] ? blend(ground, red, kusamaCov[i]) : ground;

  blur2d(leds, NUM_COLS, NUM_ROWS, 28);   // gentle softening
  FastLED.show();
}
