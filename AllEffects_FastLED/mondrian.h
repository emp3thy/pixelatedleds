#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

#define MONDRIAN_MAX_LEAVES 8

struct MondrianLeaf {
  uint8_t x, y, w, h;
  CRGB color;
};

static MondrianLeaf mondrianLeaves[MONDRIAN_MAX_LEAVES];
static uint8_t mondrianLeafCount = 0;
static unsigned long mondrianLastRegen = 0;

static CRGB mondrianPickColor() {
  uint8_t r = random8();
  if (r < 153) return CRGB(255, 255, 255); // 60% white
  if (r < 186) return CRGB(220, 0, 0);     // 13% red
  if (r < 219) return CRGB(240, 200, 0);   // 13% yellow
  if (r < 252) return CRGB(0, 0, 220);     // 13% blue
  return CRGB(0, 0, 0);                    // 4% black
}

static void mondrianSplit(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t depth) {
  if (mondrianLeafCount >= MONDRIAN_MAX_LEAVES ||
      depth >= 4 || (w <= 4 && h <= 2)) {
    mondrianLeaves[mondrianLeafCount++] = { x, y, w, h, mondrianPickColor() };
    return;
  }
  bool splitVert;
  if (w >= 2 * h) splitVert = true;
  else if (h >= 2 * w) splitVert = false;
  else splitVert = random8(2);

  if (splitVert && w >= 4) {
    uint8_t at = random8(2, w - 1);
    mondrianSplit(x, y, at, h, depth + 1);
    mondrianSplit(x + at, y, w - at, h, depth + 1);
  } else if (!splitVert && h >= 3) {
    uint8_t at = random8(1, h - 1);
    mondrianSplit(x, y, w, at, depth + 1);
    mondrianSplit(x, y + at, w, h - at, depth + 1);
  } else {
    mondrianLeaves[mondrianLeafCount++] = { x, y, w, h, mondrianPickColor() };
  }
}

static void mondrianRegen() {
  mondrianLeafCount = 0;
  mondrianSplit(0, 0, NUM_COLS, NUM_ROWS, 0);
  mondrianLastRegen = millis();
}

void mondrian() {
  if (mondrianLastRegen == 0 || millis() - mondrianLastRegen > 10000) {
    mondrianRegen();
  }
  for (uint8_t i = 0; i < mondrianLeafCount; i++) {
    MondrianLeaf &L = mondrianLeaves[i];
    for (uint8_t cx = L.x; cx < L.x + L.w; cx++) {
      for (uint8_t cy = L.y; cy < L.y + L.h; cy++) {
        bool border = (cx == L.x + L.w - 1 && L.x + L.w < NUM_COLS) ||
                      (cy == L.y + L.h - 1 && L.y + L.h < NUM_ROWS);
        leds[XY(cx, cy)] = border ? CRGB(0, 0, 0) : L.color;
      }
    }
  }
  FastLED.show();
}
