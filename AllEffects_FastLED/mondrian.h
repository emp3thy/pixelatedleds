#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

// Mondrian: recursive-rectangle subdivision with white grid lines.
// Teensy 4.0 has ~1MB RAM, so the old AVR Uno leaf/depth caps are gone.
#define MONDRIAN_MAX_LEAVES 24   // hard ceiling on rectangles
#define MONDRIAN_MIN_W      5    // min rect width  (incl. its white border)
#define MONDRIAN_MIN_H      5    // min rect height (incl. its white border)

struct MondrianLeaf {
  uint8_t x, y, w, h;
  CRGB color;
};

static MondrianLeaf mondrianLeaves[MONDRIAN_MAX_LEAVES];
static uint8_t mondrianLeafCount = 0;
static unsigned long mondrianLastRegen = 0;

// Block palette as indexed colours so the colourer can avoid same-colour
// neighbours. Grid lines are white; no black (per request).
static const CRGB MONDRIAN_PALETTE[3] = {
  CRGB(220, 0, 0),     // red
  CRGB(240, 200, 0),   // yellow
  CRGB(0, 40, 220),    // blue
};

// Do two leaves share an edge? Rects tile the grid with no gaps, so two are
// "adjacent" when their bounds touch along a horizontal or vertical seam.
static bool mondrianAdjacent(const MondrianLeaf &a, const MondrianLeaf &b) {
  bool xOverlap = (a.x < b.x + b.w) && (b.x < a.x + a.w);
  bool yOverlap = (a.y < b.y + b.h) && (b.y < a.y + a.h);
  bool vTouch = (a.y + a.h == b.y) || (b.y + b.h == a.y); // stacked
  bool hTouch = (a.x + a.w == b.x) || (b.x + b.w == a.x); // side by side
  return (xOverlap && vTouch) || (yOverlap && hTouch);
}

// Subdivide into a random target count of rectangles, then colour them.
// No recursion — everything lives in the fixed mondrianLeaves array.
static void mondrianRegen() {
  mondrianLeafCount = 1;
  mondrianLeaves[0] = { 0, 0, NUM_COLS, NUM_ROWS, CRGB::Black };

  uint8_t target = random8(14, MONDRIAN_MAX_LEAVES + 1); // 14..24 blocks

  uint16_t guard = 0;
  while (mondrianLeafCount < target && guard++ < 600) {
    // Area-weighted pick among splittable rects: big rects split more often,
    // but small ones still get chosen — yields a mix of block sizes instead of
    // everything trending toward equal.
    uint32_t totalArea = 0;
    for (uint8_t i = 0; i < mondrianLeafCount; i++) {
      MondrianLeaf &L = mondrianLeaves[i];
      if (L.w >= 2 * MONDRIAN_MIN_W || L.h >= 2 * MONDRIAN_MIN_H)
        totalArea += (uint32_t)L.w * L.h;
    }
    if (totalArea == 0) break; // nothing left big enough to split

    uint32_t pickT = random16() % totalArea;
    int best = -1;
    for (uint8_t i = 0; i < mondrianLeafCount; i++) {
      MondrianLeaf &L = mondrianLeaves[i];
      if (!(L.w >= 2 * MONDRIAN_MIN_W || L.h >= 2 * MONDRIAN_MIN_H)) continue;
      uint32_t area = (uint32_t)L.w * L.h;
      if (pickT < area) { best = i; break; }
      pickT -= area;
    }
    if (best < 0) break;

    MondrianLeaf L = mondrianLeaves[best];
    bool canV = L.w >= 2 * MONDRIAN_MIN_W;
    bool canH = L.h >= 2 * MONDRIAN_MIN_H;
    // 50/50 axis when both possible → more varied aspect ratios.
    bool splitVert = canV && (!canH || random8(2));

    if (splitVert) {
      uint8_t at = random8(MONDRIAN_MIN_W, L.w - MONDRIAN_MIN_W + 1); // [MIN_W, w-MIN_W]
      mondrianLeaves[best] = { L.x, L.y, at, L.h, CRGB::Black };
      mondrianLeaves[mondrianLeafCount++] =
          { (uint8_t)(L.x + at), L.y, (uint8_t)(L.w - at), L.h, CRGB::Black };
    } else {
      uint8_t at = random8(MONDRIAN_MIN_H, L.h - MONDRIAN_MIN_H + 1); // [MIN_H, h-MIN_H]
      mondrianLeaves[best] = { L.x, L.y, L.w, at, CRGB::Black };
      mondrianLeaves[mondrianLeafCount++] =
          { L.x, (uint8_t)(L.y + at), L.w, (uint8_t)(L.h - at), CRGB::Black };
    }
  }

  // Greedy colouring: each block avoids the colours of its already-coloured
  // neighbours, so reds/yellows/blues stay interspersed instead of pooling.
  uint8_t colorIdx[MONDRIAN_MAX_LEAVES];
  for (uint8_t i = 0; i < mondrianLeafCount; i++) {
    bool blocked[3] = { false, false, false };
    for (uint8_t j = 0; j < i; j++)
      if (mondrianAdjacent(mondrianLeaves[i], mondrianLeaves[j]))
        blocked[colorIdx[j]] = true;
    uint8_t allowed[3], n = 0;
    for (uint8_t c = 0; c < 3; c++) if (!blocked[c]) allowed[n++] = c;
    uint8_t pick = (n > 0) ? allowed[random8(n)] : random8(3); // fallback if hemmed in
    colorIdx[i] = pick;
    mondrianLeaves[i].color = MONDRIAN_PALETTE[pick];
  }
}

// Paint the current layout. amt==255 → hard set; else blend each cell toward it.
static void mondrianPaint(uint8_t amt) {
  for (uint8_t i = 0; i < mondrianLeafCount; i++) {
    MondrianLeaf &L = mondrianLeaves[i];
    for (uint8_t cx = L.x; cx < L.x + L.w; cx++) {
      for (uint8_t cy = L.y; cy < L.y + L.h; cy++) {
        // 1px white outline on all four sides of every block.
        bool edge = (cx == L.x) || (cx == L.x + L.w - 1) ||
                    (cy == L.y) || (cy == L.y + L.h - 1);
        CRGB target = edge ? CRGB(255, 255, 255) : L.color;
        if (amt >= 255) leds[XY(cx, cy)] = target;
        else            nblend(leds[XY(cx, cy)], target, amt);
      }
    }
  }
}

void mondrian() {
  static uint8_t phase = 0;   // 0 = steady, 1 = fade OUT to white, 2 = fade IN to new
  static uint8_t step = 0;
  unsigned long now = millis();

  if (mondrianLastRegen == 0) {
    mondrianRegen(); mondrianLastRegen = now; mondrianPaint(255); FastLED.show(); return;
  }

  if (phase == 0) {
    if (now - mondrianLastRegen > 10000) { phase = 1; step = 0; } // time to cycle
    else { mondrianPaint(255); FastLED.show(); return; }
  }
  if (phase == 1) {                         // fade OUT to white
    for (uint16_t i = 0; i < NUM_LEDS; i++) nblend(leds[i], CRGB::White, 50);
    FastLED.show();
    if (++step >= 10) { mondrianRegen(); phase = 2; step = 0; } // new layout ready
    return;
  }
  if (phase == 2) {                         // fade IN from white to the new layout
    mondrianPaint(45);
    FastLED.show();
    if (++step >= 10) { mondrianPaint(255); FastLED.show(); mondrianLastRegen = now; phase = 0; }
    return;
  }
}
