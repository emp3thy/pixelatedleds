# PixelatedLights Generative Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add four new generative-art effects to the PixelatedLights sketch: **Mondrian** (recursive rectangle subdivision, primary palette, black borders, regen every 10s), **Plasma** (stateless sin-sum field), **Aurora** (top-rows green/blue/violet wave with sparse violet shimmers, dark base), **Voronoi** (N drifting seed points, each pixel coloured by nearest seed). Wire all four into the rectangular-matrix lookup table by expanding selector resolution from 14 slots to 18 slots.

**Architecture:** Each effect lives in its own `.h` header under `AllEffects_FastLED/`, follows the existing pattern of single-public-function-per-file (e.g. `make_fire`, `pacifica_loop`). The four new headers are `mondrian.h`, `plasma.h`, `aurora.h`, `voronoi.h`. The main `.ino` includes them and dispatches them from the `loop()` switch on appropriate `EVERY_N_MILLISECONDS` cadences. The `effectChanging.h` selector resolves analog input to the new 18-slot rectangular lookup.

**Tech Stack:** Arduino C++ (AVR Uno), FastLED 3.3.3 (vendored). `arduino-cli` for compile validation. Git for change tracking. GitHub for CI (`compile.yml` + `bugbot.yml` already wired).

**RAM budget**: Before this plan, the sketch uses 1911 / 2048 bytes (93%, 137 free). New effect estimated RAM cost:

| Effect | Bytes | Notes |
|---|---|---|
| Mondrian | ~60 | Leaf-rectangle list, max 12 leaves × 5 bytes (x, y, w, h, color) |
| Plasma | 0 | Stateless — recomputes from `millis()` |
| Aurora | 0 | Stateless — uses `beatsin8` / palette + offset |
| Voronoi | ~24 | 6 seeds × (2 byte x/y as packed + 1 byte hue + 1 byte direction) |
| **Total** | **~84** | Leaves ~53 RAM bytes after this plan, still some margin |

If the actual measured RAM bumps too tight after Task 1 (Mondrian), reduce `MONDRIAN_MAX_LEAVES` to 8 (saves 20 bytes) before continuing.

**Verification model:** Same as the quality-fix plan — `arduino-cli compile` is the gate per task, hardware visual verify is the final gate (Task 6).

---

## File Structure

```
AllEffects_FastLED/
├── AllEffects_FastLED.ino   (Task 1-4 — add #include + switch cases; Task 5 — expand selector divisor)
├── effectChanging.h         (Task 5 — expand lookup table from 14 to 18 slots)
├── mondrian.h               (Task 1 — new file)
├── plasma.h                 (Task 2 — new file)
├── aurora.h                 (Task 3 — new file)
├── voronoi.h                (Task 4 — new file)
└── ...existing headers unchanged
```

**Effect indices** (continuing the existing numbering 0-18):
- 19 = Mondrian
- 20 = Plasma
- 21 = Aurora
- 22 = Voronoi

---

## Task 1: Mondrian effect

**Goal:** Implement `mondrian()` — recursive horizontal/vertical splits producing a Mondrian-style composition of rectangles filled with white/red/yellow/blue/black, separated by 1-pixel black borders. Regenerate the composition every 10 seconds while the slot is selected. Wire into the main `.ino` loop with effect index 19.

**Files:**
- Create: `AllEffects_FastLED/mondrian.h`
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (add `#include "mondrian.h"` and add a `case 19:` to the appropriate `EVERY_N_MILLISECONDS` switch arm)

### Implementation notes (algorithm)

1. Maintain a small array of leaf rectangles `struct Leaf { uint8_t x, y, w, h, color; }` with capacity 12.
2. On regen: clear the list. Call a recursive `mondrianSplit(x, y, w, h, depth)` that:
   - If `w <= 4 && h <= 2` OR `depth >= 4` OR leaf count near capacity → emit this rectangle as a leaf with weighted-random colour (60% white, 13% red, 13% yellow, 13% blue, 1% black).
   - Otherwise: pick split axis (horizontal if `w >= 2*h`, vertical if `h >= 2*w`, else random). Pick split position `random(1, dim-1)`. Recurse on each half.
3. On every frame: paint `leds[]` from leaf list — for each cell `(x, y)` in the panel, find which leaf contains it, set `leds[XY(x, y)]` to the leaf's colour. Then redraw 1-pixel black borders by painting black on every cell where `x == leaf.x + leaf.w - 1` or `y == leaf.y + leaf.h - 1` (right and bottom edges).
4. Call `FastLED.show()`.

### Steps

- [ ] **Step 1: Create `AllEffects_FastLED/mondrian.h`**

```c
#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

#define MONDRIAN_MAX_LEAVES 12

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
```

- [ ] **Step 2: Update `AllEffects_FastLED/AllEffects_FastLED.ino`**

After the existing `#include "noisePatterns.h"` line (around line 52), add:

```c
#include "mondrian.h"
```

In `loop()`, add `case 19: mondrian(); break;` to the `EVERY_N_MILLISECONDS(150)` switch block (existing block already handles cases 10, 13, 17 at that cadence). 150ms cadence is fine — Mondrian re-paints from the leaf list, which is cheap, and the leaves only change every 10s anyway.

Use the Edit tool. Insert the case in alphanumeric position. The block currently reads:

```c
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
    default:
      break;
    }
  }
```

Add `case 19: mondrian(); break;` before `default:` so it becomes:

```c
    case 17:
      movePaletteToPalette();
      break;
    case 19:
      mondrian();
      break;
    default:
      break;
```

- [ ] **Step 3: Compile**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED
```

Expected: compiles cleanly. RAM usage rises by ~60 bytes (Mondrian leaf array). Confirm the result still leaves at least 60 bytes free for local variables — if not, reduce `MONDRIAN_MAX_LEAVES` to 8 and recompile.

- [ ] **Step 4: Commit**

```bash
cd C:\Users\gethi\source\pixelatedlights
git add -A
git commit -m "feat(mondrian): add recursive-subdivision generative effect (slot 19)"
```

---

## Task 2: Plasma effect

**Goal:** Implement stateless `plasma()` — every frame computes per-pixel palette index as `sin(x*a + t) + sin(y*b + t) + sin((x+y)*c + t)` and looks up palette colour. Smooth flowing colour waves.

**Files:**
- Create: `AllEffects_FastLED/plasma.h`
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (add include + `case 20:` dispatch)

### Steps

- [ ] **Step 1: Create `AllEffects_FastLED/plasma.h`**

```c
#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

void plasma() {
  uint16_t t = millis() / 16;
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      uint8_t v =
        sin8(x * 16 + t) +
        sin8(y * 32 + t * 2) +
        sin8((x + y) * 12 + t);
      // v is sum of three sin8 values, range [0, 765] — divide by 3 for [0..255]
      uint8_t idx = v / 3;
      leds[XY(x, y)] = ColorFromPalette(RainbowColors_p, idx, 255, LINEARBLEND);
    }
  }
  FastLED.show();
}
```

Notes:
- Three `sin8` summed gives a richly varying field.
- `LINEARBLEND` is smoother than `NOBLEND`.
- `RainbowColors_p` is a standard FastLED palette built-in; can swap for `PartyColors_p` etc later.

- [ ] **Step 2: Update `.ino` includes + dispatch**

Add `#include "plasma.h"` after `#include "mondrian.h"`.

Add `case 20: plasma(); break;` to the `EVERY_N_MILLISECONDS(50)` switch block (next to case 6 rainbow). 50ms = 20fps, smooth.

The block currently reads:

```c
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
```

Becomes:

```c
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
    default:
      break;
    }
  }
```

- [ ] **Step 3: Compile**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED
```

Expected: clean compile. RAM unchanged from Task 1 (plasma is stateless).

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(plasma): add stateless sin-sum field effect (slot 20)"
```

---

## Task 3: Aurora effect

**Goal:** Implement `aurora()` — green/blue/violet wave shimmer concentrated on top rows, dark base on bottom rows. Approximates northern lights.

**Files:**
- Create: `AllEffects_FastLED/aurora.h`
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (add include + `case 21:` dispatch)

### Implementation notes

- 6 rows: rows 0-1 darkest, rows 2-3 medium, rows 4-5 brightest aurora — but actually since panel orientation is unclear, do it inverted: rows 0-1 brightest, rows 4-5 darkest (caller can re-orient if needed).
- Per pixel: brightness modulated by row position (top brighter) AND by `beatsin8(x, ...)` so the band shimmers horizontally.
- Hue: drift through a custom 3-stop palette: green → cyan-blue → violet.
- Add sparse violet "shimmer" pixels using `random8() < threshold` for occasional bright pixel that decays.

### Steps

- [ ] **Step 1: Create `AllEffects_FastLED/aurora.h`**

```c
#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

// Aurora palette — classic green→cyan→blue→violet
DEFINE_GRADIENT_PALETTE(auroraPalette) {
    0,   0,   0,   8,   // very deep blue base
   64,   0, 180,  60,   // green
  128,   0, 200, 160,   // cyan
  192,  40,  80, 220,   // blue
  240, 120,   0, 220,   // violet
  255, 180,  40, 240    // pale violet shimmer
};

void aurora() {
  static CRGBPalette16 palette = auroraPalette;
  uint16_t t = millis() / 12;
  for (uint8_t y = 0; y < NUM_ROWS; y++) {
    // brightness falls off toward the bottom rows; row 0 brightest
    uint8_t rowBri = 255 - (y * 220 / (NUM_ROWS - 1));
    for (uint8_t x = 0; x < NUM_COLS; x++) {
      // horizontal shimmer band
      uint8_t band = sin8(x * 5 + t) / 2 +
                     sin8(x * 3 - t / 2) / 2;
      uint8_t idx = band + (y * 8);
      uint8_t bri = scale8(rowBri, qadd8(band, 40));
      leds[XY(x, y)] = ColorFromPalette(palette, idx, bri, LINEARBLEND);
    }
  }
  // sparse violet shimmer
  if (random8() < 18) {
    uint8_t sx = random8(NUM_COLS);
    uint8_t sy = random8(2); // shimmers on top 2 rows only
    leds[XY(sx, sy)] = CRGB(220, 100, 255);
  }
  FastLED.show();
}
```

- [ ] **Step 2: Update `.ino` includes + dispatch**

Add `#include "aurora.h"` after `#include "plasma.h"`.

Add `case 21: aurora(); break;` to the `EVERY_N_MILLISECONDS(20)` switch block (same cadence as `pride()`, `pacifica_loop()`, etc — smooth motion).

The block currently has many cases. Insert before `default:`:

```c
    case 21:
      aurora();
      break;
    default:
      break;
```

- [ ] **Step 3: Compile**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED
```

Expected: clean compile. RAM up by ~16 bytes (CRGBPalette16 — but `static` inside function means file-scope storage, accounted once). Gradient palette table lives in PROGMEM, not RAM.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(aurora): add green/blue/violet northern-lights effect (slot 21)"
```

---

## Task 4: Voronoi effect

**Goal:** Implement `voronoi()` — N drifting seed points; each pixel coloured by palette-indexed hue of its nearest seed. Stained-glass / leather appearance.

**Files:**
- Create: `AllEffects_FastLED/voronoi.h`
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (add include + `case 22:` dispatch)

### Implementation notes

- 6 seeds. Each seed has `(x, y, hue, dx, dy)`. Stored compactly.
- Each frame: drift each seed by `(dx, dy)` (small step). Bounce off panel edges.
- Per pixel: find nearest seed by squared-distance (no sqrt — comparison only). Set `leds[]` to `CHSV(seed.hue, 255, 255)`.

### Steps

- [ ] **Step 1: Create `AllEffects_FastLED/voronoi.h`**

```c
#pragma once
#include <FastLED.h>
#include "configuration.h"
#include "XYMatrix.h"

#define VORONOI_SEEDS 6

struct VoronoiSeed {
  uint8_t x, y;     // position in panel coords (NUM_COLS, NUM_ROWS)
  uint8_t hue;
  int8_t dx, dy;    // velocity, small (-1, 0, +1)
};

static VoronoiSeed voronoiSeeds[VORONOI_SEEDS];
static bool voronoiInited = false;

static void voronoiInit() {
  for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
    voronoiSeeds[i].x = random8(NUM_COLS);
    voronoiSeeds[i].y = random8(NUM_ROWS);
    voronoiSeeds[i].hue = random8();
    voronoiSeeds[i].dx = (int8_t)random8(3) - 1; // -1, 0, +1
    voronoiSeeds[i].dy = (int8_t)random8(3) - 1;
  }
  voronoiInited = true;
}

void voronoi() {
  if (!voronoiInited) voronoiInit();

  // drift seeds every N frames
  EVERY_N_MILLISECONDS(120) {
    for (uint8_t i = 0; i < VORONOI_SEEDS; i++) {
      VoronoiSeed &s = voronoiSeeds[i];
      int16_t nx = (int16_t)s.x + s.dx;
      int16_t ny = (int16_t)s.y + s.dy;
      if (nx < 0 || nx >= NUM_COLS) { s.dx = -s.dx; nx = s.x + s.dx; }
      if (ny < 0 || ny >= NUM_ROWS) { s.dy = -s.dy; ny = s.y + s.dy; }
      s.x = (uint8_t)nx;
      s.y = (uint8_t)ny;
      s.hue += 1; // slow palette drift
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
        uint16_t d = ddx * ddx + ddy * ddy;
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
```

- [ ] **Step 2: Update `.ino` includes + dispatch**

Add `#include "voronoi.h"` after `#include "aurora.h"`.

Add `case 22: voronoi(); break;` to the `EVERY_N_MILLISECONDS(100)` switch block (10fps — Voronoi compute is heaviest of the four; 10fps is enough for a slowly evolving pattern).

The block currently has cases 2/3/4/5/7/8. Insert:

```c
    case 22:
      voronoi();
      break;
    default:
      break;
```

- [ ] **Step 3: Compile**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED
```

Expected: clean compile. RAM up by ~30 bytes (6 seeds × 5 bytes). Confirm at least 30 bytes free for locals — if not, drop `VORONOI_SEEDS` to 4.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "feat(voronoi): add drifting-seed Voronoi cell effect (slot 22)"
```

---

## Task 5: Expand selector to 18 slots

**Goal:** Change the analog-to-slot mapping in `effectChanging.h` from 14 slots to 18 slots, and append the four new effect indices to the rectangular-matrix lookup table. The square-matrix and simple-strip tables can either replicate the new patterns or leave the new slots filled with existing effects — to keep behaviour compatible we'll pad them with effects 0 (pride), so any non-rectangular config sees no surprise change.

**File:** `AllEffects_FastLED/effectChanging.h`

### Steps

- [ ] **Step 1: Update the divisor and cap**

Current code:

```c
  int difference = resistence - lastSetResistence;
  byte result = resistence / 78;
  if (result > 13)
  {
    result = 13;
  }
```

The analog range is 0..1023 mapped to 14 slots via `/78` (rounds slightly tight — 1023/78 = 13.1). Change to 18 slots via `/57` (1023/57 ≈ 17.9):

```c
  int difference = resistence - lastSetResistence;
  byte result = resistence / 57;
  if (result > 17)
  {
    result = 17;
  }
```

Use the Edit tool with the multi-line `old_string` to make the change atomic.

- [ ] **Step 2: Extend the rectangular-matrix lookup table**

Current:

```c
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13};
```

Change to:

```c
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13, 19, 20, 21, 22};
```

That adds the four new effect indices (19=Mondrian, 20=Plasma, 21=Aurora, 22=Voronoi) at the right side of the analog scale.

- [ ] **Step 3: Pad the other two tables to 18 slots**

Current:

```c
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3,  4,  5, 6, 7,  8, 9, 10, 11, 12, 13};
  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13};
```

Change to (pad with existing effects so they remain working — repeating effect 0 is a safe filler):

```c
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3,  4,  5, 6, 7,  8, 9, 10, 11, 12, 13,  0,  0,  0,  0};
  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13,  0,  0,  0,  0};
```

If you'd rather have the new effects also available on the square / strip configs, replace some of those filler `0`s with the new indices — but that's optional and not strictly in scope. For now, padding with `0` preserves existing behaviour on those configs.

- [ ] **Step 4: Compile**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED
```

Expected: clean compile.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "feat(selector): expand to 18 slots, wire new effects into rectangular table"
```

---

## Task 6: Final compile log + PR

**Goal:** Capture the post-feature compile log, push the branch, open a PR.

### Steps

- [ ] **Step 1: Create branch from main**

Before starting Task 1, you should already be on a feature branch. If not:

```bash
cd C:\Users\gethi\source\pixelatedlights
git checkout main
git pull origin main
git checkout -b feat/generative-effects
```

(All Tasks 1-5 commits land on `feat/generative-effects`.)

- [ ] **Step 2: Capture final compile log**

```bash
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --fqbn arduino:avr:uno --warnings all --libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\libraries C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED 2>&1 | tee C:\Users\gethi\source\pixelatedlights\docs\superpowers\plans\generative-compile.log
```

Verify the bottom of the log shows the sketch fits in flash and RAM. If RAM exceeds the 2048 byte limit, the build fails — reduce `MONDRIAN_MAX_LEAVES` or `VORONOI_SEEDS` and recompile.

- [ ] **Step 3: Commit the log**

```bash
git add docs/superpowers/plans/generative-compile.log
git commit -m "chore: capture post-generative-effects compile log"
```

- [ ] **Step 4: Push and open PR**

```bash
git push -u origin feat/generative-effects
gh pr create --base main --head feat/generative-effects \
  --title "Add 4 generative-art effects: Mondrian, Plasma, Aurora, Voronoi" \
  --body "$(cat <<'EOF'
## Summary

Adds four new generative-art effect slots and expands the selector resolution from 14 to 18 positions:

- **Slot 19 — Mondrian:** recursive horizontal/vertical subdivision producing primary-colour rectangles with black borders. Regenerates every 10 seconds.
- **Slot 20 — Plasma:** stateless sin-sum field, smooth flowing colour waves.
- **Slot 21 — Aurora:** green/blue/violet northern-lights palette concentrated on top rows with sparse violet shimmer.
- **Slot 22 — Voronoi:** 6 drifting seed points, each pixel coloured by nearest seed (stained-glass look).

## Architecture

Each effect is a single new header file (`mondrian.h`, `plasma.h`, `aurora.h`, `voronoi.h`) following the existing one-public-function-per-file pattern. The selector lookup table `effectChanging.h` gains four entries at the rectangular-matrix tail. The square-matrix and simple-strip lookup tables are padded with effect 0 in the new slots so their behaviour is preserved.

## RAM impact

Pre-feature: 1911 / 2048 bytes (93%, 137 free). Post-feature: ~+90 bytes (Mondrian leaf list + Voronoi seed list). Plasma and Aurora are stateless. Build remains within RAM limit; see `docs/superpowers/plans/generative-compile.log`.

## Verification

- `arduino-cli compile --fqbn arduino:avr:uno --warnings all` — passes
- Hardware visual verification deferred to Task 6 of plan

## Test plan

- [ ] Flash and turn potentiometer through all 18 selector positions
- [ ] Confirm slots 0-13 (existing effects) still work
- [ ] Slot 14 (Mondrian) — see different composition every ~10s, primary colours + white with black borders
- [ ] Slot 15 (Plasma) — smooth flowing colour
- [ ] Slot 16 (Aurora) — green/blue/violet, brighter at top, occasional violet shimmer
- [ ] Slot 17 (Voronoi) — distinct colour cells, drifting boundaries

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

- [ ] **Step 5: Wait for CI**

`compile.yml` and `bugbot.yml` both run. Confirm both green.

- [ ] **Step 6: Hardware verify**

Flash and visually confirm each effect renders correctly per the test plan.

---

## Coverage Self-Check

| Feature requested | Task |
|---|---|
| Mondrian effect | Task 1 |
| Mondrian regen every 10s | Task 1 (mondrian.h logic) |
| Plasma effect | Task 2 |
| Aurora effect with green/blue/violet | Task 3 |
| Voronoi cells | Task 4 |
| Wire all four into selector | Task 5 |
| CI verification + PR | Task 6 |

All requested features mapped. Effect numbering follows the existing convention (continues 19, 20, 21, 22).

---

## Execution Notes

- One commit per task — same cadence as the quality-fixes plan.
- If RAM tightens to <40 bytes free after Task 1 or Task 4, reduce `MONDRIAN_MAX_LEAVES` (12 → 8) or `VORONOI_SEEDS` (6 → 4) before continuing.
- Mondrian regen randomises per call; if a particularly boring composition appears, the next regen (within 10s) reshuffles.
- The four new effects are independent — if you want to ship only a subset, drop the corresponding tasks and trim the selector table accordingly.
