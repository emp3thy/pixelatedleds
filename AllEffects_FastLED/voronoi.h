#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

#define VORONOI_SEEDS 6

struct VoronoiSeed {
  float x, y;       // sub-pixel position → boundaries shift gradually
  float dx, dy;     // velocity in px per frame (small = languid)
  uint8_t hue;
};

static VoronoiSeed voronoiSeeds[VORONOI_SEEDS];
static bool voronoiInited = false;

static void voronoiInit() {
  for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
    voronoiSeeds[i].x = random8(NUM_COLS);
    voronoiSeeds[i].y = random8(NUM_ROWS);
    voronoiSeeds[i].hue = random8();
    do {
      voronoiSeeds[i].dx = ((int)random8(41) - 20) / 100.0f;  // -0.20..0.20 px/frame
      voronoiSeeds[i].dy = ((int)random8(41) - 20) / 100.0f;
    } while (fabsf(voronoiSeeds[i].dx) < 0.04f && fabsf(voronoiSeeds[i].dy) < 0.04f);
  }
  voronoiInited = true;
}

void voronoi() {
  if (!voronoiInited) voronoiInit();

  // Drift every frame in sub-pixel steps → smooth motion; bounce off edges.
  for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
    VoronoiSeed &s = voronoiSeeds[i];
    s.x += s.dx; s.y += s.dy;
    if (s.x < 0)            { s.x = 0;            s.dx = -s.dx; }
    if (s.x > NUM_COLS - 1) { s.x = NUM_COLS - 1; s.dx = -s.dx; }
    if (s.y < 0)            { s.y = 0;            s.dy = -s.dy; }
    if (s.y > NUM_ROWS - 1) { s.y = NUM_ROWS - 1; s.dy = -s.dy; }
  }
  EVERY_N_MILLISECONDS(120) {
    for (uint8_t i = 0; i < VORONOI_SEEDS; i++) voronoiSeeds[i].hue += 1;
  }

  // Per-pixel: nearest seed's hue (float distance for smoothly moving cells).
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      float bestD = 1e9f;
      uint8_t bestHue = 0;
      for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
        float ddx = (float)x - voronoiSeeds[i].x;
        float ddy = (float)y - voronoiSeeds[i].y;
        float d = ddx * ddx + ddy * ddy;
        if (d < bestD) { bestD = d; bestHue = voronoiSeeds[i].hue; }
      }
      leds[XY(x, y)] = CHSV(bestHue, 220, 255);
    }
  }

  blur2d(leds, NUM_COLS, NUM_ROWS, 40);   // soften the hard cell edges
  FastLED.show();
}
