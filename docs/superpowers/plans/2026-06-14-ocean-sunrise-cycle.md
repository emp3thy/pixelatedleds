# Ocean Day/Night Cycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `oceanSunrise`, a self-contained FastLED effect that plays a continuous ~3.5-minute ocean day/night cycle (sunrise → day → fiery sunset → moonlit night) on the 60×60 matrix.

**Architecture:** A layered compositor in one header. Shared cycle state (phase, night-factor, sun altitude, cloud coverage, moon phase) is computed once per frame, then layers are drawn back-to-front: sky → clouds → stars → moon → sun → ocean → reflections. Each layer is an independent helper function.

**Tech Stack:** C++ / FastLED 3.10.3, `inoise8`/`sin8`/`blend`, `XY()` serpentine map, WASM sim (`fastled` CLI) + flat `viewer.html` for verification.

**Spec:** `docs/superpowers/specs/2026-06-14-ocean-sunrise-cycle-design.md`

---

## Conventions for every task

- **Two copies:** edit `.sim/AllEffects_FastLED/oceanSunrise.h`, then `cp` it to `AllEffects_FastLED/oceanSunrise.h`. Same for any other file. Both must stay identical.
- **Cache bust + build:** FastLED's build cache does NOT track header changes. After editing, run:
  ```bash
  touch .sim/AllEffects_FastLED/AllEffects_FastLED.ino
  ```
  then build:
  ```powershell
  $scripts = "$env:LOCALAPPDATA\Programs\Python\Python312\Scripts"; if (Test-Path $scripts) { $env:Path += ";$scripts" }; & fastled .sim\AllEffects_FastLED --just-compile --no-interactive 2>&1 | Select-Object -Last 4
  ```
  The build ALWAYS ends with `Native Rust WASM build failed: could not locate src/fastled/frontend` — this is EXPECTED and harmless; `fastled.wasm` is produced before that step. Success = you see `[WASM] Linking final WASM module...` just above it.
- **Server:** `node .sim/serve_coop.js .sim/AllEffects_FastLED 8200` serves `http://127.0.0.1:8200/viewer.html` (likely already running). The viewer auto-cache-busts; just reload.
- **Force a phase for verification:** the cycle is time-based. To inspect a specific moment without waiting, temporarily set `OCEAN_T_MS` to a small value (e.g. `20000`) OR add a temporary debug override of `ocean.phase`. Revert before committing. (Verification observations below assume you can watch a full short cycle.)
- **Effect index:** the new effect is case **25** in the loop switch; slider slot **20** (21 effects total, 0–20).

---

## File Structure

- **Create:** `AllEffects_FastLED/oceanSunrise.h` (+ `.sim` copy) — the whole effect: cycle-state helpers + all layer functions + `oceanSunrise()` entry point.
- **Modify:** `AllEffects_FastLED/AllEffects_FastLED.ino` (+ `.sim`) — include + loop `case 25`.
- **Modify:** `AllEffects_FastLED/effectChanging.h` (+ `.sim`) — add 25 to tables, bump clamp/length.
- **Modify:** `AllEffects_FastLED/viewer.html` (+ `.sim`) — add label, extend slider range/clamp.

---

## Task 1: Scaffold — cycle state, sky gradient, wiring

Gets a working effect on the slider that already cross-fades the sky through all four phases. Layer functions for the other elements are declared as stubs and filled in later tasks.

**Files:**
- Create: `AllEffects_FastLED/oceanSunrise.h` (+ `.sim/AllEffects_FastLED/oceanSunrise.h`)
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (+ `.sim`)
- Modify: `AllEffects_FastLED/effectChanging.h` (+ `.sim`)
- Modify: `AllEffects_FastLED/viewer.html` (+ `.sim`)

- [ ] **Step 1: Create `oceanSunrise.h` (sim copy) with state + sky + stubs**

Write `.sim/AllEffects_FastLED/oceanSunrise.h`:

```cpp
#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Ocean day/night cycle. Spec:
//   docs/superpowers/specs/2026-06-14-ocean-sunrise-cycle-design.md
// Layered compositor; sky=rows 0..44, ocean=rows 45..59 (bottom 1/4).
// ============================================================================
#define OCEAN_HORIZON   45        // first ocean row
#define OCEAN_PEAK_Y     8        // sun's high-noon row
#define OCEAN_SUN_X     30        // sun centre column
#define OCEAN_SUN_R      5        // sun disc radius
#define OCEAN_MOON_X    44        // moon centre column (right side)
#define OCEAN_MOON_Y    12        // moon centre row
#define OCEAN_MOON_R     3        // moon disc radius
#define OCEAN_T_MS  210000UL      // full cycle length (~3.5 min)

// sub-phase boundaries as a fraction of the cycle
#define OCEAN_SR_END  0.18f       // end of sunrise
#define OCEAN_DAY_END 0.40f       // end of day
#define OCEAN_SS_END  0.62f       // end of sunset (then night to 1.0)

// ---- math helpers ----
static inline float oceanSmooth(float t){ if(t<0)t=0; if(t>1)t=1; return t*t*(3.0f-2.0f*t); }
static inline uint8_t oceanLerp8(uint8_t a,uint8_t b,float t){ return (uint8_t)(a+(b-a)*t+0.5f); }
static inline CRGB oceanLerpRGB(const CRGB&a,const CRGB&b,float t){
  return CRGB(oceanLerp8(a.r,b.r,t), oceanLerp8(a.g,b.g,t), oceanLerp8(a.b,b.b,t));
}

// ---- sky palette keyframes (top colour, horizon colour) around the cycle ----
// night -> dawn -> day -> sunset -> night. Night top is a *visible* navy.
static const float OCEAN_KF_PHASE[]   = {0.00f, 0.09f, 0.29f, 0.51f, 0.62f, 1.01f};
static const CRGB  OCEAN_KF_TOP[]     = {CRGB(0x0A1430),CRGB(0x232A63),CRGB(0x2F7FD6),CRGB(0x2E2350),CRGB(0x0A1430),CRGB(0x0A1430)};
static const CRGB  OCEAN_KF_HORIZON[] = {CRGB(0x122044),CRGB(0xFFD98C),CRGB(0xD8F0FF),CRGB(0xFFCE5A),CRGB(0x122044),CRGB(0x122044)};

// ---- shared per-frame cycle state ----
struct OceanState {
  float    phase;        // 0..1 position in the cycle
  float    nf;           // night factor 0 (day) .. 1 (night)
  float    alt;          // sun altitude 0 (horizon) .. 1 (peak)
  bool     sunUp;        // is the sun above the horizon (drawn)?
  CRGB     skyTop, skyHorizon;
  float    cloudCov;     // rendered cloud coverage %
};
static OceanState ocean;

// ---- cloud coverage walk state ----
static float   oceanCloudPrev = 0.0f;   // coverage target at start of current stage
static float   oceanCloudNext = 0.0f;   // coverage target at end of current stage
static int8_t  oceanCloudDir  = 1;      // +1 rising, -1 falling
static int8_t  oceanLastStage = -1;     // last seen stage (0..3); -1 = uninitialised
static uint16_t oceanDayCount = 0;

static inline uint8_t oceanStage(float p){
  if(p<OCEAN_SR_END)  return 0;
  if(p<OCEAN_DAY_END) return 1;
  if(p<OCEAN_SS_END)  return 2;
  return 3;
}
static float oceanStageProgress(float p){
  if(p<OCEAN_SR_END)  return p/OCEAN_SR_END;
  if(p<OCEAN_DAY_END) return (p-OCEAN_SR_END)/(OCEAN_DAY_END-OCEAN_SR_END);
  if(p<OCEAN_SS_END)  return (p-OCEAN_DAY_END)/(OCEAN_SS_END-OCEAN_DAY_END);
  return (p-OCEAN_SS_END)/(1.0f-OCEAN_SS_END);
}
static float oceanNightFactor(float p){
  if(p<OCEAN_SR_END)  return 1.0f - oceanSmooth(p/OCEAN_SR_END);
  if(p<OCEAN_DAY_END) return 0.0f;
  if(p<OCEAN_SS_END)  return oceanSmooth((p-OCEAN_DAY_END)/(OCEAN_SS_END-OCEAN_DAY_END));
  return 1.0f;
}
static float oceanSunAlt(float p){
  if(p<OCEAN_SR_END)  return oceanSmooth(p/OCEAN_SR_END);
  if(p<OCEAN_DAY_END) return 1.0f;
  if(p<OCEAN_SS_END)  return 1.0f - oceanSmooth((p-OCEAN_DAY_END)/(OCEAN_SS_END-OCEAN_DAY_END));
  return 0.0f;
}
static void oceanSkyColors(float p, CRGB &top, CRGB &hor){
  uint8_t n = sizeof(OCEAN_KF_PHASE)/sizeof(float);
  for(uint8_t i=0;i<n-1;i++){
    if(p>=OCEAN_KF_PHASE[i] && p<OCEAN_KF_PHASE[i+1]){
      float t = oceanSmooth((p-OCEAN_KF_PHASE[i])/(OCEAN_KF_PHASE[i+1]-OCEAN_KF_PHASE[i]));
      top = oceanLerpRGB(OCEAN_KF_TOP[i],     OCEAN_KF_TOP[i+1],     t);
      hor = oceanLerpRGB(OCEAN_KF_HORIZON[i], OCEAN_KF_HORIZON[i+1], t);
      return;
    }
  }
  top = OCEAN_KF_TOP[0]; hor = OCEAN_KF_HORIZON[0];
}

// advance cloud + day counters once per stage transition
static void oceanUpdateStageState(float p){
  uint8_t st = oceanStage(p);
  if(st == oceanLastStage) return;
  // detect a wrap into stage 0 from stage 3 -> a new day
  if(st == 0 && oceanLastStage == 3) oceanDayCount++;
  oceanLastStage = st;
  // step cloud coverage 1..3% in the current direction, ping-pong at 0/25
  oceanCloudPrev = oceanCloudNext;
  uint8_t step = random8(1,4);            // 1..3
  oceanCloudNext += oceanCloudDir * (float)step;
  if(oceanCloudNext >= 25.0f){ oceanCloudNext = 25.0f; oceanCloudDir = -1; }
  if(oceanCloudNext <= 0.0f ){ oceanCloudNext = 0.0f;  oceanCloudDir = +1; }
}

// ---- layer stubs (filled in later tasks) ----
static void oceanDrawClouds()      {}
static void oceanDrawStars()       {}
static void oceanDrawMoon()        {}
static void oceanDrawSun()         {}
static void oceanDrawOcean()       {}
static void oceanDrawReflections() {}

// ---- sky layer: vertical gradient top->horizon over the sky rows ----
static void oceanDrawSky(){
  for(uint8_t y=0; y<OCEAN_HORIZON; y++){
    float t = (float)y/(float)(OCEAN_HORIZON-1);     // 0 top .. 1 horizon
    CRGB c = oceanLerpRGB(ocean.skyTop, ocean.skyHorizon, t);
    for(uint8_t x=0;x<NUM_COLS;x++) leds[XY(x,y)] = c;
  }
}

// ---- entry point ----
void oceanSunrise(){
  uint32_t ms = millis() % OCEAN_T_MS;
  ocean.phase = (float)ms / (float)OCEAN_T_MS;
  oceanUpdateStageState(ocean.phase);
  ocean.nf  = oceanNightFactor(ocean.phase);
  ocean.alt = oceanSunAlt(ocean.phase);
  ocean.sunUp = ocean.phase < OCEAN_SS_END;     // sun shown sunrise..sunset
  oceanSkyColors(ocean.phase, ocean.skyTop, ocean.skyHorizon);
  // rendered cloud coverage eases across the stage between prev/next targets
  ocean.cloudCov = oceanCloudPrev +
                   (oceanCloudNext - oceanCloudPrev) * oceanSmooth(oceanStageProgress(ocean.phase));

  oceanDrawSky();
  oceanDrawClouds();
  oceanDrawStars();
  oceanDrawMoon();
  oceanDrawSun();
  oceanDrawOcean();
  oceanDrawReflections();
  FastLED.show();
}
```

- [ ] **Step 2: Wire include + loop case (sim copy of `AllEffects_FastLED.ino`)**

In `.sim/AllEffects_FastLED/AllEffects_FastLED.ino`, add the include after `#include "kusamaDots.h"`:

```cpp
#include "kusamaDots.h"
#include "oceanSunrise.h"
//end patterns
```

Add a `case 25` to the `EVERY_N_MILLISECONDS(50)` block (the one that already has `voronoi`/`waterLilies`/`kusamaDots`), right after `case 24`:

```cpp
    case 24:
      kusamaDots();
      break;
    case 25:
      oceanSunrise();
      break;
    default:
      break;
    }
  }
```

Also update the slider declaration name + range:

```cpp
fl::UISlider effectSlider("Pattern (0-20)", 0, 0, 1140, 57);
```

- [ ] **Step 3: Wire tables (sim copy of `effectChanging.h`)**

Insert `25` before the trailing `13` in BOTH matrix tables, append a `0` to the strip table, and bump the clamp to 20:

```cpp
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 13};
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 13};
  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13,  0,  0,  0,  0,  0,  0,  0};
```
```cpp
  byte result = resistence / 57;
  if (result > 20)
  {
    result = 20;
  }
```

- [ ] **Step 4: Wire viewer (sim copy of `viewer.html`)**

Add the label before `'jusBlack (off)'`:

```javascript
  const NAMES = ['pride','pacifica','metaBalls','fire','juggle','sinelon','rainbow','justWhite',
                 'palette fade','rain','confetti','bpm','rainbow+glitter',
                 'mondrian','plasma','aurora','voronoi',
                 'water lilies','kusama dots','ocean sunrise','jusBlack (off)'];
```

Update slider range, range label, index clamp, and slider name:

```html
        <input type="range" id="pattern" min="0" max="1140" step="57" value="0">
        <div class="row"><span>0</span><span id="patVal">0</span><span>20</span></div>
```
```javascript
  const SLIDER_NAME      = 'Pattern (0-20)';   // UISlider name; UI manager matches by name
```
```javascript
  const idx = v => Math.min(20, Math.round(v / 57));
```

- [ ] **Step 5: Copy all four files to the root sketch**

```bash
cd "C:/Users/gethi/source/pixelatedlights"
for f in oceanSunrise.h AllEffects_FastLED.ino effectChanging.h viewer.html; do cp ".sim/AllEffects_FastLED/$f" "AllEffects_FastLED/$f"; done
for f in oceanSunrise.h AllEffects_FastLED.ino effectChanging.h viewer.html; do diff -q ".sim/AllEffects_FastLED/$f" "AllEffects_FastLED/$f" && echo "ok $f"; done
```
Expected: `ok` for all four.

- [ ] **Step 6: Build**

```bash
touch .sim/AllEffects_FastLED/AllEffects_FastLED.ino
```
then the PowerShell build command from Conventions.
Expected: ends with `[WASM] Linking final WASM module...` then the harmless frontend error.

- [ ] **Step 7: Verify in viewer**

Reload `http://127.0.0.1:8200/viewer.html`, set the slider to slot 20 (value `1140`). Confirm: label reads `20 · ocean sunrise`; the canvas shows a full-screen vertical sky gradient that **slowly cross-fades** dawn→day→sunset→night over the cycle (temporarily set `OCEAN_T_MS` to `20000` to watch it fast; revert after). No sun/ocean yet — that's expected.

- [ ] **Step 8: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h AllEffects_FastLED/AllEffects_FastLED.ino AllEffects_FastLED/effectChanging.h AllEffects_FastLED/viewer.html .sim/AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/AllEffects_FastLED.ino .sim/AllEffects_FastLED/effectChanging.h .sim/AllEffects_FastLED/viewer.html
git commit -m "feat(effect): ocean cycle scaffold — sky gradient + wiring"
```

---

## Task 2: Sun — disc, halo, sunburst rays, motion, colour

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Replace the `oceanDrawSun` stub with the full implementation**

```cpp
// Sun colour by altitude: deep orange-red low -> white-gold high.
static CRGB oceanSunColor(){
  CRGB low(0xFF,0x5A,0x2A), high(0xFF,0xF4,0xD0);
  return oceanLerpRGB(low, high, oceanSmooth(ocean.alt));
}

static void oceanDrawSun(){
  if(!ocean.sunUp) return;
  float sy = OCEAN_HORIZON - ocean.alt * (OCEAN_HORIZON - OCEAN_PEAK_Y); // disc centre row
  float sx = OCEAN_SUN_X;
  CRGB col = oceanSunColor();
  float lowness = 1.0f - oceanSmooth(ocean.alt);   // 1 at horizon, 0 at peak

  // halo + rays radius grows when low; rays only when low
  float haloR = OCEAN_SUN_R + 6.0f + lowness*8.0f;
  for(int16_t y=0; y<OCEAN_HORIZON; y++){
    for(int16_t x=0; x<NUM_COLS; x++){
      float dx = x - sx, dy = y - sy;
      float d  = sqrtf(dx*dx + dy*dy);
      if(d <= OCEAN_SUN_R){                           // solid disc
        leds[XY(x,y)] = col;
        continue;
      }
      float add = 0.0f;
      if(d < haloR){                                  // radial halo
        add = (1.0f - (d-OCEAN_SUN_R)/(haloR-OCEAN_SUN_R)) * 0.7f;
      }
      // sunburst rays: 8 spokes, stronger when low, reaching past the halo
      if(lowness > 0.05f){
        float ang = atan2f(dy,dx);
        float spoke = cosf(ang*8.0f);                 // 8 rays
        if(spoke > 0.6f && d < haloR*1.8f){
          float rayFade = (1.0f - d/(haloR*1.8f));
          add += (spoke-0.6f)/0.4f * rayFade * lowness * 0.6f;
        }
      }
      if(add > 0.0f){
        if(add>1.0f) add=1.0f;
        leds[XY(x,y)] += CRGB((uint8_t)(col.r*add),(uint8_t)(col.g*add),(uint8_t)(col.b*add));
      }
    }
  }
}
```

- [ ] **Step 2: Copy to root, build**

```bash
cp .sim/AllEffects_FastLED/oceanSunrise.h AllEffects_FastLED/oceanSunrise.h
touch .sim/AllEffects_FastLED/AllEffects_FastLED.ino
```
then the PowerShell build. Expected: links OK.

- [ ] **Step 3: Verify**

Reload viewer at slot 20 (with `OCEAN_T_MS=20000` to watch fast). Confirm: a glowing disc rises from horizon centre, deep orange near the waterline with visible sunburst rays, brightening to white-gold and losing its rays as it climbs and holds high, then descending and re-reddening at sunset; gone during night. Revert `OCEAN_T_MS` to `210000UL`.

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — sun disc, halo, rays, motion"
```

---

## Task 3: Ocean — base mirror, slow waves, foam

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Replace the `oceanDrawOcean` stub**

```cpp
static void oceanDrawOcean(){
  uint16_t t = millis();
  // base sea colour: darker mirror of the horizon sky, further darkened at night
  CRGB base = ocean.skyHorizon;
  base.nscale8_video(120);                       // ~47% brightness
  uint8_t nightCut = (uint8_t)(ocean.nf * 70);   // darker at night
  for(uint8_t y=OCEAN_HORIZON; y<NUM_ROWS; y++){
    // depth 0 at horizon -> 1 at bottom; deeper = a touch darker
    float depth = (float)(y-OCEAN_HORIZON)/(float)(NUM_ROWS-1-OCEAN_HORIZON);
    for(uint8_t x=0;x<NUM_COLS;x++){
      // slow horizontal wave bands
      uint8_t w = sin8(x*6 + y*10 + t/12);       // 0..255
      CRGB c = base;
      c.nscale8_video(200 - (uint8_t)(depth*40));
      // wave brightness ripple ±
      int16_t lift = ((int16_t)w - 128) / 6;     // about ±21
      c.r = qadd8(c.r, lift>0?lift:0); c.r = qsub8(c.r, lift<0?-lift:0);
      c.g = qadd8(c.g, lift>0?lift:0); c.g = qsub8(c.g, lift<0?-lift:0);
      c.b = qadd8(c.b, lift>0?lift:0); c.b = qsub8(c.b, lift<0?-lift:0);
      if(nightCut){ c.nscale8_video(255-nightCut); }
      leds[XY(x,y)] = c;
      // occasional white foam on crests
      if(w > 240 && random8() < 6){
        leds[XY(x,y)] += CRGB(60,60,70);
      }
    }
  }
}
```

- [ ] **Step 2: Copy to root, build** (as Task 2 Step 2).

- [ ] **Step 3: Verify**

Reload viewer. Confirm: bottom 15 rows are sea, coloured as a darker version of the current sky (fiery at sunset, dark blue at night), with slow horizontal brightness ripples drifting sideways and the occasional brief white foam fleck.

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — sea base, waves, foam"
```

---

## Task 4: Reflections — sun shimmer column on the water

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Replace the `oceanDrawReflections` stub**

Drawn after the ocean so it sits on top. Sun reflection only; moon reflection is added in Task 6.

```cpp
// additive shimmer column on the sea directly below a sky object at column cx.
static void oceanReflect(uint8_t cx, CRGB col, float strength){
  if(strength <= 0.0f) return;
  uint16_t t = millis();
  for(uint8_t y=OCEAN_HORIZON; y<NUM_ROWS; y++){
    float depth = (float)(y-OCEAN_HORIZON)/(float)(NUM_ROWS-OCEAN_HORIZON);
    float fade  = (1.0f - depth) * strength;                 // fades downward
    uint8_t wob = sin8(y*30 + t/9);                          // vertical wobble
    float width = 1.5f + depth*3.0f;                         // widens with depth
    for(int16_t x=cx-(int16_t)width; x<=cx+(int16_t)width; x++){
      if(x<0||x>=NUM_COLS) continue;
      float dxn = 1.0f - fabsf(x-(int)cx)/(width+0.5f);
      float a = fade * dxn * (0.5f + wob/512.0f);
      if(a<=0) continue; if(a>1) a=1;
      leds[XY(x,y)] += CRGB((uint8_t)(col.r*a),(uint8_t)(col.g*a),(uint8_t)(col.b*a));
    }
  }
}

static void oceanDrawReflections(){
  if(ocean.sunUp){
    float lowness = 1.0f - oceanSmooth(ocean.alt);          // strongest when low
    oceanReflect(OCEAN_SUN_X, oceanSunColor(), 0.25f + lowness*0.75f);
  }
}
```

- [ ] **Step 2: Copy to root, build.**

- [ ] **Step 3: Verify**

Reload viewer. Confirm: a shimmering vertical column of the sun's colour on the sea directly below the sun, brightest at sunrise/sunset when the sun is low, fading as the sun climbs, wobbling with the waves.

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — sun reflection column"
```

---

## Task 5: Stars — fixed twinkling points, night-only

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Add the star table + replace the `oceanDrawStars` stub**

Place the table and init near the other statics (above `oceanDrawSky`):

```cpp
#define OCEAN_NUM_STARS 30
struct OceanStar { uint8_t x, y, ph; };
static OceanStar oceanStars[OCEAN_NUM_STARS];
static bool oceanStarsInit = false;
static void oceanInitStars(){
  for(uint8_t i=0;i<OCEAN_NUM_STARS;i++){
    oceanStars[i].x = random8(NUM_COLS);
    oceanStars[i].y = random8(OCEAN_HORIZON-6);   // upper sky only
    oceanStars[i].ph = random8();
  }
  oceanStarsInit = true;
}
```

```cpp
static void oceanDrawStars(){
  if(ocean.nf <= 0.01f) return;                   // day: no stars
  if(!oceanStarsInit) oceanInitStars();
  uint16_t t = millis();
  for(uint8_t i=0;i<OCEAN_NUM_STARS;i++){
    OceanStar &s = oceanStars[i];
    uint8_t tw = sin8(s.ph + t/6);                // twinkle 0..255
    float a = ocean.nf * (0.35f + tw/400.0f);     // opacity follows night factor
    if(a>1) a=1;
    leds[XY(s.x,s.y)] += CRGB((uint8_t)(200*a),(uint8_t)(210*a),(uint8_t)(235*a));
  }
}
```

- [ ] **Step 2: Copy to root, build.**

- [ ] **Step 3: Verify**

Reload viewer (watch fast). Confirm: as sunset turns to night, ~30 pale twinkling stars fade in across the upper sky; they fade out again through sunrise; none visible during the day.

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — twinkling night stars"
```

---

## Task 6: Moon — disc, lunar phase, halo, reflection

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Replace the `oceanDrawMoon` stub**

The lunar phase is `(oceanDayCount % 30)/30` (0 = full). Shape via an offset mask: a second circle, filled with the *sky* colour, slides across the disc to carve the terminator.

```cpp
static CRGB oceanMoonColor(){ return CRGB(0xCF,0xDA,0xF2); } // pale blue-white

static void oceanDrawMoon(){
  if(ocean.nf <= 0.01f) return;
  float moonPhase = (float)(oceanDayCount % 30) / 30.0f;     // 0 full .. 0.5 new .. 1 full
  // mask offset: 0 at full, ±2*R at new. Sign gives waxing/waning side.
  float off = (moonPhase <= 0.5f ? moonPhase : moonPhase-1.0f) * 4.0f * OCEAN_MOON_R;
  CRGB col = oceanMoonColor();
  CRGB skyHere = oceanLerpRGB(ocean.skyTop, ocean.skyHorizon,
                              (float)OCEAN_MOON_Y/(float)(OCEAN_HORIZON-1));
  for(int16_t y=OCEAN_MOON_Y-OCEAN_MOON_R-3; y<=OCEAN_MOON_Y+OCEAN_MOON_R+3; y++){
    if(y<0||y>=OCEAN_HORIZON) continue;
    for(int16_t x=OCEAN_MOON_X-OCEAN_MOON_R-3; x<=OCEAN_MOON_X+OCEAN_MOON_R+3; x++){
      if(x<0||x>=NUM_COLS) continue;
      float dx=x-OCEAN_MOON_X, dy=y-OCEAN_MOON_Y;
      float d=sqrtf(dx*dx+dy*dy);
      if(d <= OCEAN_MOON_R){
        // inside disc: lit unless inside the offset mask circle
        float mdx = x-(OCEAN_MOON_X+off);
        bool masked = (mdx*mdx + dy*dy) <= (float)(OCEAN_MOON_R*OCEAN_MOON_R);
        if(masked == (off>=0)) {            // carve the correct limb
          // masked region -> sky (unlit)
          leds[XY(x,y)] = oceanLerpRGB(leds[XY(x,y)], skyHere, ocean.nf);
        } else {
          leds[XY(x,y)] = oceanLerpRGB(leds[XY(x,y)], col, ocean.nf);
        }
      } else if(d <= OCEAN_MOON_R+2.5f){    // soft halo
        float a = (1.0f-(d-OCEAN_MOON_R)/2.5f)*0.4f*ocean.nf;
        if(a>0) leds[XY(x,y)] += CRGB((uint8_t)(col.r*a),(uint8_t)(col.g*a),(uint8_t)(col.b*a));
      }
    }
  }
}
```

- [ ] **Step 2: Add the moon reflection to `oceanDrawReflections`**

Append inside `oceanDrawReflections()` after the sun block:

```cpp
  if(ocean.nf > 0.01f){
    oceanReflect(OCEAN_MOON_X, oceanMoonColor(), 0.30f * ocean.nf);
  }
```

- [ ] **Step 3: Copy to root, build.**

- [ ] **Step 4: Verify**

Reload viewer (watch fast). Confirm: at night a pale moon sits high on the right with a soft halo and a dim white reflection column on the sea. Temporarily set `oceanDayCount` to different values (e.g. add `oceanDayCount=7;` before the moon draw) to confirm the lit limb changes (full vs crescent); remove the override before committing.

- [ ] **Step 5: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — moon with lunar phase + reflection"
```

---

## Task 7: Clouds — stage-stepped coverage, noise patches, tint, drift

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Replace the `oceanDrawClouds` stub**

`ocean.cloudCov` (0..25, already eased) thresholds an `inoise8` field; the cloud tint comes from blending white toward the horizon colour by phase warmth, dimmed at night.

```cpp
static void oceanDrawClouds(){
  float cov = ocean.cloudCov;
  if(cov < 5.0f) return;                          // <5% reads as clear
  if(cov > 25.0f) cov = 25.0f;
  // higher coverage -> lower noise threshold -> more cloud pixels
  uint8_t thresh = (uint8_t)(255 - (cov/25.0f)*120.0f);   // 255(min) .. 135(max)
  uint16_t t = millis();
  // cloud tint: white by day, warmed toward horizon colour, dimmed at night
  CRGB warm = ocean.skyHorizon;
  CRGB tint = oceanLerpRGB(CRGB(235,238,245), warm, 0.45f);
  tint = oceanLerpRGB(tint, CRGB(60,66,86), ocean.nf*0.7f);
  for(uint8_t y=0; y<OCEAN_HORIZON-4; y++){       // sky only, leave a clear strip at horizon
    for(uint8_t x=0;x<NUM_COLS;x++){
      uint8_t n = inoise8(x*28 + t/20, y*28);     // drifting sideways
      if(n <= thresh) continue;
      float a = (n-thresh)/(float)(255-thresh);   // soft cloud edge
      // fade clouds out near the top and the horizon strip
      a *= 1.0f - fabsf((float)y/(OCEAN_HORIZON-4) - 0.5f)*0.6f;
      if(a<=0) continue; if(a>1) a=1;
      leds[XY(x,y)] = oceanLerpRGB(leds[XY(x,y)], tint, a);
    }
  }
}
```

- [ ] **Step 2: Copy to root, build.**

- [ ] **Step 3: Verify**

Reload viewer. To force visible clouds quickly, temporarily set `oceanCloudNext = 25.0f; oceanCloudPrev = 25.0f;` at the top of `oceanSunrise()` (so `cloudCov` ≈ 25). Confirm: soft cloud patches drift sideways across the sky only (never over the sea), tinted grey-white by day, picking up pink/gold near sunset, dim by night. Remove the override; confirm over a few cycles that coverage builds and clears gradually (use small `OCEAN_T_MS` to speed stage transitions).

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — drifting tinted clouds"
```

---

## Task 8: Integration tuning pass

**Files:**
- Modify: `.sim/AllEffects_FastLED/oceanSunrise.h` then copy to root.

- [ ] **Step 1: Watch a full real-time cycle**

Ensure `OCEAN_T_MS` is `210000UL`. Reload viewer at slot 20 and watch (or scrub via temporary small `OCEAN_T_MS`) a complete sunrise → day → sunset → night → sunrise. Note any rough transitions (e.g. sun popping in/out at the horizon, palette banding, foam too sparse/busy, clouds too faint).

- [ ] **Step 2: Apply targeted tweaks**

Adjust only the tunables (no structural change): palette keyframe colours/positions (`OCEAN_KF_*`), `OCEAN_PEAK_Y`, sun/halo radii, wave speed (`t/12`) and foam probability (`random8() < 6`), cloud `thresh` curve, star count. Make minimal edits to smooth anything flagged in Step 1. Rebuild and re-verify after each change.

- [ ] **Step 3: Confirm black stays last & all labels correct**

Slide through all 21 slots; confirm slot 20 = `ocean sunrise` renders the scene and slot 21-position (value 1140 is slot 20; the final slot) — verify `jusBlack (off)` is the last entry and shows black. Confirm no other effect's label/behaviour shifted.

- [ ] **Step 4: Final commit**

```bash
git add AllEffects_FastLED/oceanSunrise.h .sim/AllEffects_FastLED/oceanSunrise.h
git commit -m "feat(effect): ocean cycle — integration tuning"
```

---

## Self-Review notes

- **Spec coverage:** sky gradient/palettes (T1), sun disc+halo+rays+motion+colour (T2), ocean base+waves+foam (T3), sun reflection (T4), stars+night-factor (T5), moon phase+halo+reflection (T6), stage-stepped clouds+tint+drift (T7), horizon at 45 / moon right / 210s / flat horizon assumptions (constants in T1). Wiring as effect 25 / slot 20 with black last (T1 steps 2-4). All spec sections map to a task.
- **Type consistency:** `oceanSunColor()` defined in T2 and reused in T4; `oceanMoonColor()` defined in T6 and used in its reflection; `oceanReflect()` defined in T4 and extended in T6; `ocean` state fields set in T1 entry point and read by all layers; layer function names match the stubs declared in T1.
- **No placeholders:** every step has complete code or an exact command + expected observation.
```
