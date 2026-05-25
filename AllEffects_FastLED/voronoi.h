#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

#define VORONOI_SEEDS 6

struct VoronoiSeed {
  uint8_t x, y;
  uint8_t hue;
  int8_t dx, dy;
};

static VoronoiSeed voronoiSeeds[VORONOI_SEEDS];
static bool voronoiInited = false;

static void voronoiInit() {
  for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
    voronoiSeeds[i].x = random8(NUM_COLS);
    voronoiSeeds[i].y = random8(NUM_ROWS);
    voronoiSeeds[i].hue = random8();
    voronoiSeeds[i].dx = (int8_t)random8(3) - 1;
    voronoiSeeds[i].dy = (int8_t)random8(3) - 1;
  }
  voronoiInited = true;
}

void voronoi() {
  if (!voronoiInited) voronoiInit();

  // drift seeds every 120ms
  EVERY_N_MILLISECONDS(120) {
    for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
      VoronoiSeed &s = voronoiSeeds[i];
      int16_t nx = (int16_t)s.x + s.dx;
      int16_t ny = (int16_t)s.y + s.dy;
      if (nx < 0 || nx >= NUM_COLS) { s.dx = -s.dx; nx = (int16_t)s.x + s.dx; }
      if (ny < 0 || ny >= NUM_ROWS) { s.dy = -s.dy; ny = (int16_t)s.y + s.dy; }
      s.x = (uint8_t)nx;
      s.y = (uint8_t)ny;
      s.hue += 1;
    }
  }

  // per-pixel: paint with nearest seed's hue
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      uint16_t bestD = 65535;
      uint8_t bestHue = 0;
      for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
        int16_t ddx = (int16_t)x - voronoiSeeds[i].x;
        int16_t ddy = (int16_t)y - voronoiSeeds[i].y;
        uint16_t d = (uint16_t)(ddx * ddx + ddy * ddy);
        if (d < bestD) {
          bestD = d;
          bestHue = voronoiSeeds[i].hue;
        }
      }
      leds[XY(x, y)] = CHSV(bestHue, 220, 255);
    }
  }
  FastLED.show();
}
