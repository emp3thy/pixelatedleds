#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

#define MONDRIAN_MAX_LEAVES 8
#define MONDRIAN_MAX_DEPTH  2
#define MONDRIAN_WORK_STACK 3

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
  return CRGB(0, 0, 0);                    // ~1.5% black
}

static void mondrianEmitLeaf(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
  if (mondrianLeafCount >= MONDRIAN_MAX_LEAVES) return;
  mondrianLeaves[mondrianLeafCount++] = { x, y, w, h, mondrianPickColor() };
}

// Iterative subdivision via a small explicit work stack — avoids deep recursion
// that would overflow the AVR Uno's tiny SRAM stack budget (25 bytes free).
static void mondrianRegen() {
  struct WorkItem { uint8_t x, y, w, h, depth; };
  WorkItem stack[MONDRIAN_WORK_STACK];
  uint8_t sp = 0;

  mondrianLeafCount = 0;
  stack[sp++] = { 0, 0, NUM_COLS, NUM_ROWS, 0 };

  while (sp > 0 && mondrianLeafCount < MONDRIAN_MAX_LEAVES) {
    WorkItem cur = stack[--sp];

    if (cur.depth >= MONDRIAN_MAX_DEPTH || (cur.w <= 4 && cur.h <= 2)) {
      mondrianEmitLeaf(cur.x, cur.y, cur.w, cur.h);
      continue;
    }

    bool splitVert;
    if (cur.w >= 2 * cur.h) splitVert = true;
    else if (cur.h >= 2 * cur.w) splitVert = false;
    else splitVert = random8(2);

    bool didSplit = false;
    if (splitVert && cur.w >= 4 && sp + 2 <= MONDRIAN_WORK_STACK) {
      uint8_t at = random8(2, cur.w - 1);
      stack[sp++] = { cur.x, cur.y, at, cur.h, (uint8_t)(cur.depth + 1) };
      stack[sp++] = { (uint8_t)(cur.x + at), cur.y, (uint8_t)(cur.w - at), cur.h, (uint8_t)(cur.depth + 1) };
      didSplit = true;
    } else if (!splitVert && cur.h >= 3 && sp + 2 <= MONDRIAN_WORK_STACK) {
      uint8_t at = random8(1, cur.h - 1);
      stack[sp++] = { cur.x, cur.y, cur.w, at, (uint8_t)(cur.depth + 1) };
      stack[sp++] = { cur.x, (uint8_t)(cur.y + at), cur.w, (uint8_t)(cur.h - at), (uint8_t)(cur.depth + 1) };
      didSplit = true;
    }

    if (!didSplit) {
      mondrianEmitLeaf(cur.x, cur.y, cur.w, cur.h);
    }
  }
}

void mondrian() {
  if (mondrianLastRegen == 0 || millis() - mondrianLastRegen > 10000) {
    mondrianRegen();
    mondrianLastRegen = millis();
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
