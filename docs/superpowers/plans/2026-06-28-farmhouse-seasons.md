# Farmhouse Seasons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `farmhouseSeasons` FastLED effect: a still 60×60 farmstead scene that plays one year on a ~4 min loop, the seasons driving colour/state change across layered elements.

**Architecture:** A layered compositor in a single self-contained header `farmhouseSeasons.h`, mirroring `oceanSunrise.h`. One entry `farmhouseSeasons()` computes the year phase from `millis()` and calls `fhRender(phase)`, which draws layers back-to-front into `leds[]` then `FastLED.show()`. Shapes are rasterised with sub-pixel **coverage anti-aliasing** (soft edges like the sun) via a small set of helpers. Pure helpers (`fhGround`, `fhSunY`, season blends, `fhSnowCover`) take the phase explicitly so any season can be pinned for inspection.

**Tech Stack:** C++ (Arduino/Teensy 4.0), FastLED 3.3.3 (`CRGB`, `XY()`, `FastLED.show()`), `math.h`. WASM sim preview via the repo's `fastled` toolchain + `.sim/run-viewer.ps1`.

## Global Constraints

- Canvas is **60×60** (`NUM_COLS == NUM_ROWS == 60`, `configuration.h`); address pixels via `XY(x, y)` from `XYMatrix.h`. `y = 0` is the top.
- Target board **Teensy 4.0** (FPU, ~1 MB RAM); `leds[]` is 3600 `CRGB`. No large heap allocations; module-static state only.
- **Emissive-palette rule:** never use true black for "dark" — lift to a visible tone (true black reads as "off").
- **Soft edges:** draw shapes with coverage AA (blend into `leds[]`), do not rely on hard pixel blocks. The physical diffuser blends neighbours further.
- **Keep root and `.sim` copies in sync** for every file touched: `AllEffects_FastLED/<f>` and `.sim/AllEffects_FastLED/<f>`.
- Effect entry must be named `farmhouseSeasons()` and end with `FastLED.show()`, matching the other effect headers.
- Header guard `#pragma once`; include `<FastLED.h>`, `<math.h>`, `"configuration.h"`, `"XYMatrix.h"`.
- Spec: `docs/superpowers/specs/2026-06-28-farmhouse-seasons-design.md`.

## Year-phase convention (used by every layer)

`yearPhase ∈ [0,1)`: **spring 0.00–0.25, summer 0.25–0.50, autumn 0.50–0.75, winter 0.75–1.00**. Season *centres* are 0.125 / 0.375 / 0.625 / 0.875. Steady-state seasonal values are keyframed at the centres and crossfaded between them (continuous morph). Discrete events use explicit sub-windows: **harvest** 0.62–0.68, **snow accumulation** 0.70–0.82, **melt** 0.95–1.00→0.00–0.05. These constants live as `#define`s in Task 1 and are reused everywhere.

## File Structure

- Create `AllEffects_FastLED/farmhouseSeasons.h` — the whole effect: constants, helpers, per-layer draw functions, `fhRender(float)`, `farmhouseSeasons()`. One responsibility: render this scene.
- Mirror `.sim/AllEffects_FastLED/farmhouseSeasons.h` — identical copy the WASM build compiles.
- Modify `AllEffects_FastLED/AllEffects_FastLED.ino` (+ `.sim` copy) — `#include` + `case 26` + bump sim slider range.
- Modify `AllEffects_FastLED/effectChanging.h` (+ `.sim` copy) — append effect id `26` to both matrix arrays before the trailing `13`; raise the notch cap to `21`.
- Modify `AllEffects_FastLED/viewer.html` (+ `.sim` copy) — add the `'farmhouse seasons'` label and raise the slider/`idx` cap.

## Verification model (read before starting)

This codebase has **no unit-test harness for effects**; effects are verified **visually in the WASM sim** (as `oceanSunrise` was). Each task therefore uses a build-and-look cycle, made deterministic by a compile-time phase override added in Task 1:

- `#define FH_DEBUG_PHASE 0.375f` pins the year to a chosen value (here, summer centre). Comment it out for the live millis-driven loop.
- Build the sim (from repo root, PowerShell): `& fastled .sim\AllEffects_FastLED --just-compile --no-interactive` (the "could not locate src/fastled/frontend" tail error is expected/harmless).
- Serve + view: run `.sim\run-viewer.ps1` (background) and open `http://127.0.0.1:8200/viewer.html`; drag the Pattern slider to the **farmhouse seasons** notch (last before "jusBlack (off)").
- To check a transition, set `FH_DEBUG_PHASE` to the sub-window value (e.g. `0.65f` for harvest) and rebuild, or set `#define FH_YEAR_MS 20000` to watch a fast 20 s year live.

Each task's "verify" step lists the exact `FH_DEBUG_PHASE` values to inspect and what you must see. Keep `FH_DEBUG_PHASE` commented out in the committed code unless a task says otherwise.

---

### Task 1: Scaffold, helpers, and wiring

**Files:**
- Create: `AllEffects_FastLED/farmhouseSeasons.h`
- Create: `.sim/AllEffects_FastLED/farmhouseSeasons.h` (copy)
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino:30` (includes), `:94-96` (switch), `:37` (sim slider)
- Modify: `AllEffects_FastLED/effectChanging.h:13-14,18-21`
- Modify: `AllEffects_FastLED/viewer.html:52-55,139` (+ slider `max` in the HTML above line 44)
- Mirror all four `.sim` copies.

**Interfaces:**
- Produces:
  - `CRGB fhMix(CRGB a, CRGB b, float t)` — clamped linear blend.
  - `float fhSmooth(float t)` — smoothstep, clamps 0..1.
  - `float fhClampf(float v, float lo, float hi)`.
  - `void fhPlot(int x, int y, CRGB c, float a)` — coverage blend `c` into `leds[XY(x,y)]` with alpha `a` (bounds-checked).
  - `void fhFillRow(int y, int x0, int x1, CRGB c)` — solid run.
  - `void fhDisc(float cx, float cy, float r, CRGB c)` — AA filled circle.
  - `float fhGround(float x)` — undulating hill/field boundary row.
  - `float fhYearPhase()` — current phase from `millis()`/`FH_DEBUG_PHASE`.
  - `void fhRender(float p)` and `void farmhouseSeasons()`.
  - Constants (`#define`): `FH_YEAR_MS`, season/event windows listed above.

- [ ] **Step 1: Create the header with constants, helpers, and a flat sky stub**

Create `AllEffects_FastLED/farmhouseSeasons.h`:

```cpp
#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Farmhouse seasons. Spec:
//   docs/superpowers/specs/2026-06-28-farmhouse-seasons-design.md
// Layered compositor, 60x60, one year on a ~4 min loop. y=0 top.
// ============================================================================
#define FH_YEAR_MS 240000UL        // full year (~4 min). Set 20000 to scrub fast.
// #define FH_DEBUG_PHASE 0.375f   // uncomment to pin the year (summer centre)

// season centres / boundaries
#define FH_SPRING 0.125f
#define FH_SUMMER 0.375f
#define FH_AUTUMN 0.625f
#define FH_WINTER 0.875f
// discrete event windows
#define FH_HARVEST0 0.62f
#define FH_HARVEST1 0.68f
#define FH_SNOW0    0.70f          // accumulation start
#define FH_SNOW1    0.82f          // fully covered
#define FH_MELT0    0.96f          // melt start (wraps through 0.00)
#define FH_MELT1    0.05f          // melt end

// ---- math helpers ----
static inline float fhClampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float fhSmooth(float t){ t=fhClampf(t,0,1); return t*t*(3.0f-2.0f*t); }
static inline uint8_t fhL8(uint8_t a,uint8_t b,float t){ return (uint8_t)(a+(b-a)*t+0.5f); }
static inline CRGB fhMix(CRGB a,CRGB b,float t){ t=fhClampf(t,0,1);
  return CRGB(fhL8(a.r,b.r,t),fhL8(a.g,b.g,t),fhL8(a.b,b.b,t)); }

// ---- raster helpers (coverage AA blends into leds[]) ----
static inline void fhPlot(int x,int y,CRGB c,float a){
  if(x<0||x>=NUM_COLS||y<0||y>=NUM_ROWS||a<=0.0f) return;
  uint16_t i=XY((uint8_t)x,(uint8_t)y);
  leds[i]= a>=1.0f ? c : fhMix(leds[i],c,a);
}
static inline void fhFillRow(int y,int x0,int x1,CRGB c){
  for(int x=x0;x<=x1;x++) fhPlot(x,y,c,1.0f);
}
static void fhDisc(float cx,float cy,float r,CRGB c){
  int y0=(int)floorf(cy-r-1), y1=(int)ceilf(cy+r+1);
  int x0=(int)floorf(cx-r-1), x1=(int)ceilf(cx+r+1);
  for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
    float d=sqrtf((x-cx)*(x-cx)+(y-cy)*(y-cy));
    float a=fhClampf(r+0.5f-d,0.0f,1.0f);   // 1px soft edge
    fhPlot(x,y,c,a);
  }
}

// undulating hill/field boundary (float row), matches the design mockups
static inline float fhGround(float x){
  return 24.0f + 1.8f*sinf(x*0.45f) + 1.3f*sinf(x*0.17f+2.0f);
}

// ---- phase ----
static inline float fhYearPhase(){
#ifdef FH_DEBUG_PHASE
  return FH_DEBUG_PHASE;
#else
  return (float)(millis()%FH_YEAR_MS)/(float)FH_YEAR_MS;
#endif
}

// ---- temporary flat sky stub (replaced in Task 2) ----
static void fhRender(float p){
  CRGB sky = fhMix(CRGB(0x7FC0EC), CRGB(0xCFE6F4), 0.5f);
  for(uint8_t y=0;y<NUM_ROWS;y++) for(uint8_t x=0;x<NUM_COLS;x++) leds[XY(x,y)]=sky;
}

void farmhouseSeasons(){
  fhRender(fhYearPhase());
  FastLED.show();
}
```

- [ ] **Step 2: Wire the effect into the sketch**

In `AllEffects_FastLED/AllEffects_FastLED.ino`, add the include after line 30 (`#include "oceanSunrise.h"`):

```cpp
#include "oceanSunrise.h"
#include "farmhouseSeasons.h"
```

Add a case in the 50 ms switch, right after the `case 25` block (after line 96):

```cpp
    case 25:
      oceanSunrise();
      break;
    case 26:
      farmhouseSeasons();
      break;
```

Bump the sim slider range on line 37 so a 22nd notch exists (21*57 = 1197):

```cpp
fl::UISlider effectSlider("Pattern (0-20)", 0, 0, 1197, 57);
```

- [ ] **Step 3: Register the effect id in the pattern tables**

In `AllEffects_FastLED/effectChanging.h`, append `26` before the trailing `13` in **both** matrix arrays (lines 13–14):

```cpp
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 13};
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 13};
```

Raise the notch cap (lines 18–21) from 20 to 21:

```cpp
  if (result > 21)
  {
    result = 21;
  }
```

- [ ] **Step 4: Add the viewer label and raise its cap**

In `AllEffects_FastLED/viewer.html`, insert `'farmhouse seasons'` before `'jusBlack (off)'` (lines 52–55):

```javascript
  const NAMES = ['pride','pacifica','metaBalls','fire','juggle','sinelon','rainbow','justWhite',
                 'palette fade','rain','confetti','bpm','rainbow+glitter',
                 'mondrian','plasma','aurora','voronoi',
                 'water lilies','kusama dots','ocean sunrise','farmhouse seasons','jusBlack (off)'];
```

Raise the index clamp on line 139 from 20 to 21:

```javascript
  const idx = v => Math.min(21, Math.round(v / 57));
```

Find the pattern slider `<input id="pattern" ... max="1140">` in the HTML head section (above line 44) and change `max="1140"` to `max="1197"`.

- [ ] **Step 5: Mirror all files into `.sim`**

Run (PowerShell, from repo root):

```powershell
Copy-Item AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h -Force
Copy-Item AllEffects_FastLED/AllEffects_FastLED.ino .sim/AllEffects_FastLED/AllEffects_FastLED.ino -Force
Copy-Item AllEffects_FastLED/effectChanging.h .sim/AllEffects_FastLED/effectChanging.h -Force
Copy-Item AllEffects_FastLED/viewer.html .sim/AllEffects_FastLED/viewer.html -Force
```

- [ ] **Step 6: Build the sim and verify the effect is selectable**

Run: `& fastled .sim\AllEffects_FastLED --just-compile --no-interactive`
Expected: compiles to `fastled_js/` (the trailing "could not locate src/fastled/frontend" line is expected). No C++ errors mentioning `farmhouseSeasons`.

Then run `.sim\run-viewer.ps1` (background) and open `http://127.0.0.1:8200/viewer.html`. Drag the Pattern slider to the last notch before "jusBlack (off)"; the name readout shows `21 · farmhouse seasons` and the panel fills with a flat pale-blue sky.

- [ ] **Step 7: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h \
        AllEffects_FastLED/AllEffects_FastLED.ino .sim/AllEffects_FastLED/AllEffects_FastLED.ino \
        AllEffects_FastLED/effectChanging.h .sim/AllEffects_FastLED/effectChanging.h \
        AllEffects_FastLED/viewer.html .sim/AllEffects_FastLED/viewer.html
git commit -m "feat(farmhouse): scaffold farmhouseSeasons effect + wiring"
```

---

### Task 2: Sky layer — palette crossfade + sun arc

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhMix`, `fhSmooth`, `fhDisc`, season `#define`s.
- Produces:
  - `CRGB fhCycRGB(float p, const float* ph, const CRGB* col, int n)` — wrap-around keyframe interp on a cyclic phase.
  - `float fhCycF(float p, const float* ph, const float* val, int n)` — same for floats.
  - `float fhSunY(float p)` — sun centre row (height arc).
  - `void fhDrawSky(float p)` — sky gradient + sun, replaces the Task 1 stub body.

- [ ] **Step 1: Add cyclic keyframe interpolators and the sky/sun draw, call it from `fhRender`**

In `farmhouseSeasons.h`, replace the `// ---- temporary flat sky stub ----` block (the whole `fhRender` from Task 1) with:

```cpp
// ---- cyclic keyframe interpolation (phase wraps at 1.0) ----
static float fhCycF(float p,const float* ph,const float* val,int n){
  for(int i=0;i<n-1;i++){ if(p>=ph[i] && p<ph[i+1]){
    float t=(p-ph[i])/(ph[i+1]-ph[i]); return val[i]+(val[i+1]-val[i])*fhSmooth(t); } }
  return val[n-1];
}
static CRGB fhCycRGB(float p,const float* ph,const CRGB* col,int n){
  for(int i=0;i<n-1;i++){ if(p>=ph[i] && p<ph[i+1]){
    float t=fhSmooth((p-ph[i])/(ph[i+1]-ph[i])); return fhMix(col[i],col[i+1],t); } }
  return col[n-1];
}

// sky palettes per season centre (top, horizon). Endpoints duplicate spring for wrap.
static const float FH_SKY_PH[]      = {0.0f, FH_SPRING, FH_SUMMER, FH_AUTUMN, FH_WINTER, 1.0f};
static const CRGB  FH_SKY_TOP[]     = {CRGB(0x8FCBEE),CRGB(0x8FCBEE),CRGB(0x7FC0EC),CRGB(0x9FC2D8),CRGB(0xAEB9C4),CRGB(0x8FCBEE)};
static const CRGB  FH_SKY_BOT[]     = {CRGB(0xD6EEF8),CRGB(0xD6EEF8),CRGB(0xCFE6F4),CRGB(0xE6E3CF),CRGB(0xDBE1E6),CRGB(0xD6EEF8)};
static const CRGB  FH_SUN_COL[]     = {CRGB(0xFFE9A0),CRGB(0xFFE9A0),CRGB(0xFFE27A),CRGB(0xFFD27A),CRGB(0xF6EFD2),CRGB(0xFFE9A0)};

// sun height: highest at summer (y~6), lowest deep winter (y~12), rising late winter
static const float FH_SUNY_PH[]  = {0.0f, FH_SPRING, FH_SUMMER, FH_AUTUMN, 0.875f, 1.0f};
static const float FH_SUNY_VAL[] = {8.0f, 7.0f,      6.0f,      9.0f,      12.0f,   8.0f};
static const float FH_SUNR_PH[]  = {0.0f, FH_SUMMER, FH_WINTER, 1.0f};
static const float FH_SUNR_VAL[] = {4.0f, 4.0f,      3.0f,      4.0f};
static inline float fhSunY(float p){ return fhCycF(p,FH_SUNY_PH,FH_SUNY_VAL,6); }

static void fhDrawSky(float p){
  CRGB top=fhCycRGB(p,FH_SKY_PH,FH_SKY_TOP,6);
  CRGB bot=fhCycRGB(p,FH_SKY_PH,FH_SKY_BOT,6);
  for(uint8_t y=0;y<28;y++){
    CRGB c=fhMix(top,bot,(float)y/26.0f);
    for(uint8_t x=0;x<NUM_COLS;x++) leds[XY(x,y)]=c;
  }
  float r=fhCycF(p,FH_SUNR_PH,FH_SUNR_VAL,4);
  CRGB sc=fhCycRGB(p,FH_SKY_PH,FH_SUN_COL,6);
  fhDisc(50.0f, fhSunY(p), r, sc);
}

static void fhRender(float p){
  fhDrawSky(p);
}
```

- [ ] **Step 2: Mirror to `.sim` and build**

```powershell
Copy-Item AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h -Force
```
Run: `& fastled .sim\AllEffects_FastLED --just-compile --no-interactive`
Expected: clean compile.

- [ ] **Step 3: Verify the four skies + sun height**

Inspect by pinning `FH_DEBUG_PHASE` (rebuild each): `0.125f` spring, `0.375f` summer, `0.625f` autumn, `0.875f` winter. You must see: sky gradient shifting blue→hazier→grey across the seasons, and the **sun disc highest in summer (~row 6), lowest/palest/smaller in winter (~row 12)**. Re-comment `FH_DEBUG_PHASE`.

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): sky palette crossfade + sun height arc"
```

---

### Task 3: Hills + rolling ground + seasonal hill grass

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhGround`, `fhCycRGB`, season `#define`s, `fhPlot`.
- Produces:
  - `int fhDomeTop(int x)` — top row of the wooded hills at column `x`, or `-1`.
  - `CRGB fhHillGrass(float p)` — hill grass colour for the phase.
  - `void fhDrawHills(float p)`.

- [ ] **Step 1: Add the hill geometry + grass fill**

In `farmhouseSeasons.h`, before `fhRender`, add:

```cpp
// two overlapping hill domes (cx, rx, ry); base nominally at row 24
struct FhHill { float cx, rx, ry; };
static const FhHill FH_HILLS[2] = { {16,21,13}, {42,23,16} };

static int fhDomeTop(int x){
  int t=-1;
  for(int h=0;h<2;h++){
    float n=(x-FH_HILLS[h].cx)/FH_HILLS[h].rx; if(n<-1||n>1) continue;
    int top=24-(int)lroundf(FH_HILLS[h].ry*sqrtf(1.0f-n*n));
    if(t<0||top<t) t=top;
  }
  return t;
}

static const float FH_HILL_PH[]  = {0.0f, FH_SPRING, FH_SUMMER, FH_AUTUMN, FH_WINTER, 1.0f};
static const CRGB  FH_HILL_COL[] = {CRGB(0x7BB24A),CRGB(0x7BB24A),CRGB(0x6FA23F),CRGB(0xA8984E),CRGB(0xE8EEF2),CRGB(0x7BB24A)};
static inline CRGB fhHillGrass(float p){ return fhCycRGB(p,FH_HILL_PH,FH_HILL_COL,6); }

static void fhDrawHills(float p){
  CRGB g=fhHillGrass(p);
  for(int x=0;x<NUM_COLS;x++){
    int dt=fhDomeTop(x); if(dt<0) continue;
    int gb=(int)lroundf(fhGround(x));
    for(int y=dt;y<gb;y++) fhPlot(x,y,g,1.0f);
  }
}
```

Call it in `fhRender` after the sky:

```cpp
static void fhRender(float p){
  fhDrawSky(p);
  fhDrawHills(p);
}
```

- [ ] **Step 2: Mirror, build, verify**

Mirror to `.sim`, build (`& fastled .sim\AllEffects_FastLED --just-compile --no-interactive`). Pin `0.375f` (summer) and `0.875f` (winter): two green domes meeting the sky on a **wavy** (not flat) line in summer; the domes turn **snow-white** in winter. Re-comment debug.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): rolling hills with seasonal grass"
```

---

### Task 4: Hill wood — trees with leaf/turn/fall/bare lifecycle

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhDomeTop`, `fhGround`, `fhDisc`, `fhPlot`, `fhMix`, `fhSmooth`, season + event `#define`s, `fhSnowCover` is NOT yet available (snow handled in Task 9).
- Produces:
  - `CRGB fhFoliage(float p, uint8_t seed)` — a tree's canopy colour for the phase (green→turn→bare handled by caller), seed varies the shade/turn timing.
  - `float fhCanopyAmt(float p)` — 0 (bare) .. 1 (full canopy), drives leaf-out and leaf-fall.
  - `void fhDrawWood(float p)` — all hill trees.

- [ ] **Step 1: Add foliage colour, canopy amount, and the wood draw**

Add before `fhRender`:

```cpp
// summer green shades; autumn turn targets (per-seed variety)
static const CRGB FH_GREEN[7]={CRGB(0x347E3A),CRGB(0x2E7434),CRGB(0x46A24E),CRGB(0x3E9446),CRGB(0x52AE59),CRGB(0x43994A),CRGB(0x3C913F)};
static const CRGB FH_TURN[7] ={CRGB(0xE0892E),CRGB(0xD6552B),CRGB(0xE8B23A),CRGB(0xC24A2A),CRGB(0xB5862F),CRGB(0xE0A030),CRGB(0xD67A2A)};
static const CRGB FH_SPRGRN[7]={CRGB(0x86C552),CRGB(0xA6D86A),CRGB(0x6FB23E),CRGB(0x9ACB5A),CRGB(0x7CC04A),CRGB(0xA0D060),CRGB(0x8AC850)};

// canopy fullness: bare in winter, leafs out in spring, full summer, sheds in autumn
static float fhCanopyAmt(float p){
  if(p<FH_SPRING) return fhSmooth((p-0.02f)/(FH_SPRING-0.02f));   // leaf-out across early spring
  if(p<FH_AUTUMN) return 1.0f;                                    // full spring->autumn centre
  if(p<0.78f)     return 1.0f-fhSmooth((p-FH_AUTUMN)/(0.78f-FH_AUTUMN)); // shed late autumn
  return 0.0f;                                                    // bare winter
}
// canopy colour for the phase (spring fresh -> summer green -> autumn turn)
static CRGB fhFoliage(float p,uint8_t seed){
  CRGB summer=FH_GREEN[seed%7];
  if(p<FH_SPRING) return fhMix(FH_SPRGRN[seed%7],summer,fhSmooth(p/FH_SPRING));
  if(p<FH_AUTUMN) return summer;
  float t=fhSmooth((p-FH_AUTUMN)/(0.78f-FH_AUTUMN));              // turn during late autumn
  return fhMix(summer,FH_TURN[seed%7],t);
}

static void fhDrawWood(float p){
  float amt=fhCanopyAmt(p);
  for(int h=0;h<2;h++){
    const FhHill& H=FH_HILLS[h];
    for(int ax=(int)(H.cx-H.rx)+3; ax<=(int)(H.cx+H.rx)-3; ax+=3){
      float n=(ax-H.cx)/H.rx; if(n<-0.92f||n>0.92f) continue;
      int top=24-(int)lroundf(H.ry*sqrtf(1.0f-n*n));
      int bl=(int)lroundf(fhGround(ax));
      int start=top+2+(ax&1);
      for(int ty=start; ty+4<=bl; ty+=5){
        int tx=ax+((ty&1)?1:0);
        uint8_t seed=(uint8_t)(ax*3+ty);
        // trunk (always visible)
        fhPlot(tx,ty+1,CRGB(0x4A3318),1.0f);
        fhPlot(tx,ty+2,CRGB(0x4A3318),1.0f);
        // canopy scales with amt (bare in winter)
        if(amt>0.02f) fhDisc(tx,ty,2.3f*amt+0.3f, fhFoliage(p,seed));
      }
    }
  }
}
```

Call in `fhRender` after hills:

```cpp
  fhDrawHills(p);
  fhDrawWood(p);
```

- [ ] **Step 2: Mirror, build, verify the lifecycle**

Mirror + build. Pin and inspect: `0.10f` (leaf-out: small canopies growing on bare trunks), `0.375f` (full green), `0.70f` (autumn: yellow/orange/red, staggered shades, thinning), `0.875f` (bare trunks). Re-comment.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): hill wood with leaf/turn/fall/bare lifecycle"
```

---

### Task 5: Field — grass margin + wheat crop states + transitions

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhGround`, `fhMix`, `fhSmooth`, `fhPlot`, season + event `#define`s.
- Produces:
  - `CRGB fhCropColor(float p, int fx, int fy)` — crop cell colour (earth→green→gold→stubble) honouring the transitions.
  - `CRGB fhGrassMargin(float p, int y)` — the green margin colour (also seasonal).
  - `void fhDrawField(float p)` — margin + crop with anti-aliased top boundary.

**Field rule:** crop tone must stay distinct from the grass margin every phase (margin is green; crop is earth/lighter-green/gold/stubble — all distinct). Winter snow handled in Task 9 (this task draws stubble; snow overlays later).

- [ ] **Step 1: Add the crop/margin colours and the field draw**

Add before `fhRender`:

```cpp
// crop palettes
static inline CRGB fhStripe(CRGB base,CRGB hi,CRGB lo,float v){
  return v>0 ? fhMix(base,hi,v*0.5f) : fhMix(base,lo,-v*0.5f);
}
static CRGB fhCropColor(float p,int fx,int fy){
  float v=sinf(fy*0.9f + 1.3f*sinf(fx*0.16f));   // rolling rows
  // season colour sets
  CRGB earth=fhStripe(CRGB(0x5A4028),CRGB(0x6E5234),CRGB(0x412C1A),v);
  CRGB green=fhStripe(CRGB(0xADCB55),CRGB(0xC6DC72),CRGB(0x93B845),v);
  CRGB gold =fhStripe(CRGB(0xE8B43A),CRGB(0xF8D758),CRGB(0xC6871E),v);
  CRGB stub =fhStripe(CRGB(0xE4D7A0),CRGB(0xF0E6BE),CRGB(0xCBBA7E),v);
  // spring: bare earth, then green grows in (speckled emergence) into summer
  if(p<FH_SPRING) return earth;
  if(p<FH_SUMMER){ float gp=(p-FH_SPRING)/(FH_SUMMER-FH_SPRING);
    float thr=((fx*7+fy*13)%100)/100.0f; float t=fhClampf((gp-thr)*3.0f,0,1);
    return fhMix(earth,green,t); }
  // summer -> autumn: ripen green to gold
  if(p<FH_HARVEST0){ float rp=(p-FH_SUMMER)/(FH_HARVEST0-FH_SUMMER);
    float off=((fx*3+fy*5)%10)/30.0f; float t=fhClampf((rp-off)*1.6f,0,1);
    return fhMix(green,gold,t); }
  // harvest: gold -> stubble, swept across
  if(p<FH_HARVEST1){ float hp=(p-FH_HARVEST0)/(FH_HARVEST1-FH_HARVEST0);
    float sm=fx/60.0f; float t=fhClampf((hp-sm)*6.0f,0,1);
    return fhMix(gold,stub,t); }
  // autumn->winter->spring hold stubble (snow overlays in Task 9); melt back to earth
  if(p<FH_MELT0) return stub;
  return earth;   // late winter reveal (melt handled visually by snow layer)
}

static const float FH_MARGIN_PH[]  = {0.0f, FH_SUMMER, FH_AUTUMN, 1.0f};
static const CRGB  FH_MARGIN_COL[] = {CRGB(0x7CB24A),CRGB(0x74A83F),CRGB(0x9AA24A),CRGB(0x7CB24A)};
static inline CRGB fhGrassMargin(float p,int y){
  CRGB c=fhCycRGB(p,FH_MARGIN_PH,FH_MARGIN_COL,4);
  return (y&1)? c : fhMix(c,CRGB(0xFFFFFF),0.04f);
}

static void fhDrawField(float p){
  for(int fx=0;fx<NUM_COLS;fx++){
    int g0=(int)lroundf(fhGround(fx));
    float g1f=fhGround(fx)+2.6f+1.2f*sinf(fx*0.32f);   // float wheat-top edge (smooth)
    for(int gy=g0; gy<(int)floorf(g1f); gy++) fhPlot(fx,gy,fhGrassMargin(p,gy),1.0f);
    for(int fy=(int)floorf(g1f); fy<46; fy++){
      CRGB col=fhCropColor(p,fx,fy);
      float cov=fhClampf((fy+1)-g1f,0,1);               // AA top boundary
      if(cov<1.0f) col=fhMix(fhGrassMargin(p,fy),col,cov);
      fhPlot(fx,fy,col,1.0f);
    }
  }
}
```

Call in `fhRender` after the wood:

```cpp
  fhDrawWood(p);
  fhDrawField(p);
```

- [ ] **Step 2: Mirror, build, verify each field state + transition**

Mirror + build. Pin and inspect: `0.125f` (dark plowed earth, furrow stripes), `0.30f` (green speckling in over earth), `0.375f` (green wheat, **lighter than the grass margin**, striped), `0.55f` (ripening toward gold), `0.65f` (harvest sweep gold→pale stubble across x), `0.875f` (stubble). Confirm the grass↔crop top edge is a soft wavy line, and crop ≠ margin in every state. Re-comment.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): wheat field states + grass margin + transitions"
```

---

### Task 6: Barn layer

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhPlot`, `fhFillRow`.
- Produces: `void fhDrawBarn(float p)` (a gambrel barn; snow cap added in Task 9).

- [ ] **Step 1: Add the barn draw and call it**

Add before `fhRender`:

```cpp
static void fhTri(int x0,int y0,int x1,int y1,int x2,int y2,CRGB c){
  int miny=min(y0,min(y1,y2)),maxy=max(y0,max(y1,y2));
  int minx=min(x0,min(x1,x2)),maxx=max(x0,max(x1,x2));
  for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){
    float d1=(x-x1)*(y0-y1)-(x0-x1)*(y-y1);
    float d2=(x-x2)*(y1-y2)-(x1-x2)*(y-y2);
    float d3=(x-x0)*(y2-y0)-(x2-x0)*(y-y0);
    bool neg=(d1<0)||(d2<0)||(d3<0), pos=(d1>0)||(d2>0)||(d3>0);
    if(!(neg&&pos)) fhPlot(x,y,c,1.0f);
  }
}
static void fhDrawBarn(float p){
  // gambrel roof (white): two triangles approximating ridge->knuckle->eave
  fhTri(42,31, 48,34, 36,34, CRGB(0xEDE8DC));
  fhTri(33,37, 51,37, 42,31, CRGB(0xEDE8DC));
  fhTri(33,37, 51,37, 48,34, CRGB(0xEDE8DC));
  // red body
  for(int y=37;y<46;y++) fhFillRow(y,34,49,CRGB(0xBE3B2C));
  for(int y=37;y<46;y++){ fhPlot(38,y,CRGB(0xB23528),1.0f); fhPlot(45,y,CRGB(0xB23528),1.0f); }
  // hayloft + open doorway
  for(int y=33;y<36;y++) fhFillRow(y,41,43,CRGB(0x2A1408));
  for(int y=40;y<46;y++) fhFillRow(y,40,46,CRGB(0x241006));
}
```

Call in `fhRender` after the field:

```cpp
  fhDrawField(p);
  fhDrawBarn(p);
```

- [ ] **Step 2: Mirror, build, verify**

Mirror + build. At any phase the barn shows a pale gambrel (two-slope) roof over a red body with a dark open doorway, right of centre. Re-comment any debug.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): gambrel barn layer"
```

---

### Task 7: Wall + gate + hero tree (spring blossom)

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhPlot`, `fhFillRow`, `fhDisc`, `fhFoliage`, `fhCanopyAmt`, `fhMix`, `fhSmooth`, season `#define`s.
- Produces:
  - `void fhDrawWall(float p)` — two-course wall + wooden gate.
  - `void fhDrawHeroTree(float p)` — hero tree with spring blossom then leaf-out.

- [ ] **Step 1: Add wall+gate and hero tree, call both**

Add before `fhRender`:

```cpp
static void fhDrawWall(float p){
  for(int x=0;x<NUM_COLS;x++){
    if(x>=27 && x<=32) continue;                 // gate gap
    fhPlot(x,46,(x%3==0)?CRGB(0x6E6C68):CRGB(0x94918C),1.0f);
    fhPlot(x,47,(x%3==0)?CRGB(0x6E6C68):CRGB(0x94918C),1.0f);
    fhPlot(x,48,((x+1)%3==0)?CRGB(0x6E6C68):CRGB(0x9C998F),1.0f);
    fhPlot(x,49,((x+1)%3==0)?CRGB(0x6E6C68):CRGB(0x9C998F),1.0f);
    fhPlot(x,46,CRGB(0xC2BFB8),1.0f);            // cap
    fhPlot(x,48,CRGB(0x6E6C68),1.0f);            // mortar line
  }
  // wooden gate
  for(int y=44;y<50;y++){ fhPlot(27,y,CRGB(0x5A3F22),1.0f); fhPlot(32,y,CRGB(0x5A3F22),1.0f); }
  for(int y=45;y<50;y++) fhFillRow(y,28,31,CRGB(0x8A6A3E));
  fhFillRow(46,28,31,CRGB(0xA6824E)); fhFillRow(48,28,31,CRGB(0xA6824E));
}

static void fhDrawHeroTree(float p){
  fhFillRow(46,12,13,CRGB(0x6B4A2B));            // trunk (rows 40..46)
  for(int y=40;y<47;y++){ fhPlot(12,y,CRGB(0x6B4A2B),1.0f); fhPlot(13,y,CRGB(0x6B4A2B),1.0f); }
  float amt=fhCanopyAmt(p);
  if(amt<=0.02f) return;                          // bare in winter
  // clump layout (cx,cy,r,seed)
  static const float CL[6][4]={{12,34,3.2f,0},{10,37,3.0f,1},{14,37,3.0f,2},{12,38,3.4f,3},{13,33,2.4f,4},{10,34,2.2f,5}};
  for(int i=0;i<6;i++){
    CRGB c=fhFoliage(p,(uint8_t)CL[i][3]);
    fhDisc(CL[i][0],CL[i][1],CL[i][2]*amt+0.3f,c);
  }
  // spring blossom: pink/white flush early spring, fading as leaves fill
  float bl=fhSmooth(1.0f-(p/FH_SPRING));          // 1 at p=0 -> 0 at spring end
  if(p<FH_SPRING && bl>0.05f){
    fhDisc(11,35,1.2f,fhMix(CRGB(0xF2B6C6),CRGB(0xFFFFFF),0.3f));
    fhDisc(14,36,1.0f,CRGB(0xF2B6C6));
    fhPlot(12,33,fhMix(leds[XY(12,33)],CRGB(0xFADCE6),bl),bl);
  }
}
```

Call in `fhRender` after the barn:

```cpp
  fhDrawBarn(p);
  fhDrawWall(p);
  fhDrawHeroTree(p);
```

- [ ] **Step 2: Mirror, build, verify**

Mirror + build. Inspect: any phase shows the two-course wall + wooden gate; hero tree is a multi-shade clump on a trunk that turns in autumn (`0.70f`), is bare in winter (`0.875f`), and shows **pink/white blossom in early spring** (`0.03f`) before leafing green (`0.20f`). Re-comment.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): wall, gate, hero tree with spring blossom"
```

---

### Task 8: Foreground — path, grass, flowers, wheat ears

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhPlot`, `fhFillRow`, `fhDisc`, `fhMix`, `fhSmooth`, season `#define`s.
- Produces:
  - `void fhDrawForeground(float p)` — fills foreground grass, draws the curvy path, seasonal flowers, seasonal wheat ears.
  - `float fhFlowerAmt(float p)` — 0..1 bloom level (spring→summer peak→autumn fade).

- [ ] **Step 1: Add the foreground layer and call it**

Add before `fhRender`:

```cpp
// flower bloom level: none winter, rise spring, peak summer, fade autumn
static float fhFlowerAmt(float p){
  if(p<0.04f) return 0.0f;
  if(p<FH_SUMMER) return fhSmooth((p-0.04f)/(FH_SUMMER-0.04f));
  if(p<FH_AUTUMN) return 1.0f-0.4f*fhSmooth((p-FH_SUMMER)/(FH_AUTUMN-FH_SUMMER));
  if(p<0.72f)     return 0.6f*(1.0f-fhSmooth((p-FH_AUTUMN)/(0.72f-FH_AUTUMN)));
  return 0.0f;
}
static inline float fhPathCenter(int gy){ float t=(gy-49)/11.0f; return 29.0f+7.0f*sinf(t*3.6f)*t; }
static inline float fhPathHalf(int gy){ float t=(gy-49)/11.0f; return 2.4f+t*4.2f; }

static void fhDrawForeground(float p){
  // foreground grass (seasonal: green summer/spring, drier autumn; winter snow in Task 9)
  CRGB fg = (p<FH_AUTUMN)? CRGB(0x6FA537) : fhMix(CRGB(0x6FA537),CRGB(0xB6A24C),fhSmooth((p-FH_AUTUMN)/0.13f));
  for(int y=50;y<60;y++) fhFillRow(y,0,59,fg);

  // curvy path with soft (coverage) edges, always visible
  CRGB ptop=CRGB(0xC8B07A), pbot=CRGB(0xB49A63);
  for(int y=49;y<60;y++){
    float c=fhPathCenter(y), hh=fhPathHalf(y), tt=(y-49)/11.0f;
    CRGB pc=fhMix(ptop,pbot,tt);
    float L=c-hh, R=c+hh;
    for(int x=(int)floorf(L)-1;x<=(int)ceilf(R);x++){
      float cov=fhClampf(min((float)x+1,R)-max((float)x,L),0,1);
      if(cov>0) fhPlot(x,y,pc,cov);
    }
  }

  // flowers in the FOREGROUND grass only (white/yellow/pink), bloom-scaled
  float fa=fhFlowerAmt(p);
  if(fa>0.05f){
    static const int FX[8]={5,11,18,24,40,46,52,57};
    static const int FYr[8]={54,57,52,58,53,56,51,55};
    static const CRGB FC[3]={CRGB(0xF2F0E2),CRGB(0xF2D24A),CRGB(0xE68FB0)};
    for(int i=0;i<8;i++){
      float thr=(i+1)/9.0f;                       // flowers open as bloom rises
      if(fa<thr) continue;
      // keep off the path
      float c=fhPathCenter(FYr[i]), hh=fhPathHalf(FYr[i]);
      if(FX[i]>c-hh-1 && FX[i]<c+hh+1) continue;
      fhPlot(FX[i],FYr[i],FC[i%3],fhClampf((fa-thr)*4,0,1));
    }
  }

  // foreground wheat ears: green in summer, gold in autumn; none spring/winter
  bool ears = (p>=FH_SUMMER && p<FH_HARVEST0);
  if(ears){
    float et=fhClampf((p-FH_SUMMER)/(FH_HARVEST0-FH_SUMMER),0,1);
    CRGB stalk=fhMix(CRGB(0x7FA840),CRGB(0xC9A23A),et), head=fhMix(CRGB(0xA6C85A),CRGB(0xF2C648),et);
    static const int EX[8]={3,6,9,2,57,54,51,58};
    static const int EY[8]={57,55,58,53,57,55,58,53};
    for(int i=0;i<8;i++){
      for(int k=0;k<6;k++) fhPlot(EX[i],EY[i]+k,stalk,1.0f);
      fhDisc(EX[i],EY[i]-1,1.6f,head);
      fhPlot(EX[i],EY[i]-3,head,1.0f);
    }
  }
}
```

Call in `fhRender` after the hero tree:

```cpp
  fhDrawHeroTree(p);
  fhDrawForeground(p);
```

- [ ] **Step 2: Mirror, build, verify**

Mirror + build. Inspect: `0.20f` (spring: flowers beginning, no ears), `0.375f` (summer: most flowers brightest, green ears, path clear), `0.625f` (autumn: gold ears, flowers fading), `0.875f` (winter: no flowers/ears, path still visible). Confirm flowers avoid the path. Re-comment.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): foreground path, grass, flowers, wheat ears"
```

---

### Task 9: Snow model — accumulation, melt, path-clear

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhPlot`, `fhMix`, `fhClampf`, `fhSmooth`, `fhPathCenter`, `fhPathHalf`, event `#define`s.
- Produces:
  - `float fhSnowAmt(float p)` — global accumulation 0..1 (ramps up FH_SNOW0→FH_SNOW1, holds, melts FH_MELT0→FH_MELT1 across the wrap).
  - `float fhSnowCover(int x, int y, float p)` — per-cell snow coverage 0..1 (excludes the path; furrow-shadow texture over the field so snowy field ≠ snowy grass).
  - `void fhDrawSnow(float p)` — composites snow over the scene as the final ground/object layer (before weather particles).

- [ ] **Step 1: Add the snow model and composite it**

Add before `fhRender`:

```cpp
static float fhSnowAmt(float p){
  if(p>=FH_SNOW0 && p<FH_SNOW1) return fhSmooth((p-FH_SNOW0)/(FH_SNOW1-FH_SNOW0)); // accumulate
  if(p>=FH_SNOW1 && p<FH_MELT0) return 1.0f;                                       // hold
  if(p>=FH_MELT0) return 1.0f-fhSmooth((p-FH_MELT0)/((1.0f-FH_MELT0)+FH_MELT1));   // melt (wraps)
  if(p<FH_MELT1)  return 1.0f-fhSmooth(((1.0f-FH_MELT0)+p)/((1.0f-FH_MELT0)+FH_MELT1));
  return 0.0f;
}
static float fhSnowCover(int x,int y,float p){
  float amt=fhSnowAmt(p); if(amt<=0.0f || y<14) return 0.0f;
  // path stays clear
  if(y>=49){ float c=fhPathCenter(y),hh=fhPathHalf(y); if(x>c-hh-1 && x<c+hh+1) return 0.0f; }
  // soft front + tiny per-cell jitter so the edge isn't a hard line
  float j=((x*7+y*13)%5)/40.0f;
  return fhClampf((amt-j)*3.0f,0,1);
}
static void fhDrawSnow(float p){
  if(fhSnowAmt(p)<=0.0f) return;
  for(int y=14;y<NUM_ROWS;y++) for(int x=0;x<NUM_COLS;x++){
    float cov=fhSnowCover(x,y,p); if(cov<=0) continue;
    // field band (rows ~ground..46) keeps faint furrow shadow so it stays distinct from grass
    CRGB snow = (y>=24 && y<46 && ((x+ (int)(y))%4==0)) ? CRGB(0xDCE6EC) : CRGB(0xEAF1F5);
    fhPlot(x,y,snow,cov*0.92f);
  }
}
```

Call in `fhRender` after the foreground (snow sits over the ground/objects, under weather particles added in Task 10):

```cpp
  fhDrawForeground(p);
  fhDrawSnow(p);
```

- [ ] **Step 2: Mirror, build, verify accumulate → hold → melt**

Mirror + build. Inspect: `0.69f` (no snow yet, stubble field), `0.76f` (snow accumulating, partial), `0.875f` (full white cover, **path still clear**, field shows faint furrow shadow vs cleaner grass/roofs), `0.99f` (melting back), `0.03f` (nearly gone, earth/green returning). Re-comment.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): snow accumulation, melt, path-clear"
```

---

### Task 10: Weather particles — leaves, snow, blossom

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: `fhPlot`, `fhMix`, `fhClampf`, season + event `#define`s, `FH_TURN`.
- Produces:
  - A fixed particle pool + `void fhDrawWeather(float p)` — falling leaves (late autumn), falling snow (winter), drifting blossom (spring). Particles are deterministic functions of `(index, millis)` so no RNG/state churn.

- [ ] **Step 1: Add the particle layer and call it last**

Add before `fhRender`:

```cpp
#define FH_NPART 40
// particle i position is a deterministic function of time; mode picks colour/behaviour
static void fhDrawWeather(float p){
  // which particle type is active, and how strongly
  float leaves = (p>=FH_AUTUMN && p<FH_SNOW0)? fhSmooth((p-FH_AUTUMN)/(FH_SNOW0-FH_AUTUMN)) : 0.0f;
  float snow   = (p>=FH_SNOW0 && p<FH_MELT0)? 1.0f : 0.0f;
  float petals = (p<FH_SPRING)? fhSmooth(1.0f-(p/FH_SPRING)) : 0.0f;
  uint32_t t=millis();
  for(int i=0;i<FH_NPART;i++){
    float col = (float)((i*37)%60);
    float speed = 6.0f + (i%5);                // px/sec
    float sway = sinf((t*0.001f)+(i*1.3f))*2.0f;
    float fall = fmodf((t*0.001f*speed)+(i*7), 64.0f);
    int x=(int)(col+sway), y=(int)(fall-4);
    if(y<0||y>=NUM_ROWS||x<0||x>=NUM_COLS) continue;
    if(snow>0)        fhPlot(x,y,CRGB(0xF2F6F8),0.9f);
    else if(leaves>0) fhPlot(x,y,FH_TURN[i%7],0.85f*leaves);
    else if(petals>0) fhPlot(x,y,CRGB(0xF7CBD9),0.8f*petals);
  }
}
```

Call in `fhRender` as the very last layer:

```cpp
  fhDrawSnow(p);
  fhDrawWeather(p);
```

- [ ] **Step 2: Mirror, build, verify**

Mirror + build. Set `#define FH_YEAR_MS 20000` (fast live year, `FH_DEBUG_PHASE` commented) and watch: drifting pink petals in spring, falling coloured leaves in late autumn, falling white snow through winter, nothing in summer. Restore `FH_YEAR_MS 240000`.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): weather particles (leaves/snow/blossom)"
```

---

### Task 11: Full-year integration, performance, polish

**Files:**
- Modify: `AllEffects_FastLED/farmhouseSeasons.h` (+ `.sim` copy)

**Interfaces:**
- Consumes: everything. Produces: a verified, tuned full-year loop. No new public functions required.

- [ ] **Step 1: Watch a full fast year and note glitches**

Set `#define FH_YEAR_MS 30000` (debug commented), build, and watch 2–3 full loops in the viewer. Write down any hard pops at season boundaries, any layer that flickers, and the sim FPS (top of viewer). Expected baseline: 30+ fps (in line with voronoi/aurora/ocean).

- [ ] **Step 2: Fix the boundary pops found in Step 1**

For any discontinuity, widen/adjust the relevant crossfade window constant (the `FH_*` `#define`s) so the transition is continuous — e.g. if green→gold snaps, lower the `*1.6f` rate in `fhCropColor` or move `FH_HARVEST0`. Make only the edits your Step 1 notes call for; show each changed line in the commit. (No code shown here because the specific edits depend on what you observe — but each must be a concrete constant/colour change, not a structural rewrite.)

- [ ] **Step 3: Restore production constants and confirm**

Ensure `FH_YEAR_MS` is back to `240000UL` and `FH_DEBUG_PHASE` is commented out. Build once more; the live effect runs a smooth ~4 min year.

- [ ] **Step 4: Confirm root/.sim parity**

Run: `git diff --no-index AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h`
Expected: no differences. (If any, re-copy root → `.sim` and rebuild.)

- [ ] **Step 5: Commit**

```bash
git add AllEffects_FastLED/farmhouseSeasons.h .sim/AllEffects_FastLED/farmhouseSeasons.h
git commit -m "feat(farmhouse): full-year integration + transition polish"
```

---

## Self-Review

**Spec coverage** (each spec section → task):
- Scene composition / geometry → Tasks 2–8 (every element placed per the mockup coordinates).
- Layer architecture (Sky, Hills+wood, Field, Barn, Wall+hero, Foreground, Weather) → Tasks 2,3,4,5,6,7,8,10; barn its own layer (6). ✓
- Cycle/timing (~4 min, 4 seasons, crossfade) → Task 1 constants + Task 11 integration. ✓
- Sun height arc / sky crossfade → Task 2. ✓
- Hill wood lifecycle (leaf/turn/fall/bare, no blossom) → Task 4. ✓
- Hero tree spring blossom → Task 7. ✓
- Field states + transitions + always-distinct-from-grass + winter furrow-shadow snow → Tasks 5 & 9. ✓
- Barn (constant + snow cap) → Task 6 + snow via Task 9. ✓
- Wall + gate (+ snow cap) → Task 7 + Task 9. ✓
- Path always clear in winter → Task 9 (`fhSnowCover` path exclusion). ✓
- Foreground flowers (foreground-only, bloom curve) + wheat ears → Task 8. ✓
- Snowfall triggers winter + accumulation + melt → Tasks 9 & 10. ✓
- Weather particles (leaves/snow/blossom) → Task 10. ✓
- Vector-shape coverage AA rendering → Task 1 helpers (`fhDisc`, `fhPlot`, AA path/field boundaries). ✓
- Firmware wiring (`.h`, index 26, `.sim` mirror, viewer label) → Task 1. ✓
- Performance → Task 11. ✓

**Placeholder scan:** No "TBD/TODO". The only deliberately observation-driven step is Task 11 Step 2 (boundary-pop fixes), which is constrained to "edit a named constant/colour you identified", not a vague placeholder — its inputs come from Step 1's concrete findings.

**Type consistency:** Names are stable across tasks — `fhMix/fhSmooth/fhClampf/fhPlot/fhFillRow/fhDisc/fhGround` (T1); `fhCycRGB/fhCycF/fhSunY/fhDrawSky` (T2); `fhDomeTop/fhHillGrass/fhDrawHills` (T3); `fhCanopyAmt/fhFoliage/fhDrawWood` (T4); `fhCropColor/fhGrassMargin/fhDrawField/fhStripe` (T5); `fhDrawBarn/fhTri` (T6); `fhDrawWall/fhDrawHeroTree` (T7); `fhFlowerAmt/fhPathCenter/fhPathHalf/fhDrawForeground` (T8); `fhSnowAmt/fhSnowCover/fhDrawSnow` (T9); `fhDrawWeather` (T10). `fhRender` accumulates calls in order each task; `FH_GREEN/FH_TURN/FH_SPRGRN` defined in T4 and reused in T7/T10. No signature drift.
