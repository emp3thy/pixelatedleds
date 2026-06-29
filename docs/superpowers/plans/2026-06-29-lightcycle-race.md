# Light-Cycle Race Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `lightCycleRace` FastLED effect (id 27): a Tron-style red-vs-blue AI light-cycle race on the 60×60 panel that loops new rounds forever.

**Architecture:** A single self-contained header `lightCycleRace.h` holding a stateful simulation — a `uint8_t` occupancy grid, two cycle structs, an xorshift PRNG, and a phase machine (RACING → FLASH → PAUSE → reset). Each call advances the race on its own millis timer and redraws the grid into `leds[]`. Same conventions as `oceanSunrise.h` / `farmhouseSeasons.h`.

**Tech Stack:** C++ (Arduino/Teensy 4.0), FastLED 3.3.3 (`CRGB`, `XY()`, `FastLED.show()`). WASM sim preview via the `fastled` toolchain + `.sim/run-viewer.ps1`.

## Global Constraints

- Canvas **60×60** (`NUM_COLS==NUM_ROWS==60`); draw via `leds[XY(x,y)]`; `(0,0)` top-left.
- Target **Teensy 4.0** (FPU, ~1 MB RAM); module-static state only; a 3.6 KB grid is fine.
- **Emissive rule:** background is lifted dark navy `#0A0E1A`, never pure black.
- Effect entry named `lightCycleRace()`, ending with `FastLED.show()`.
- Header guard `#pragma once`; include `<FastLED.h>`, `<math.h>`, `"configuration.h"`, `"XYMatrix.h"`.
- **Keep root and `.sim` copies in sync** for every file touched.
- **Selector wiring (PR #5 lesson): there are THREE selector arrays** in `effectChanging.h` — `listOfPatternsForRectangularMatrix`, `listOfPatternsForSquareMatrix`, **and** `listOfPatternsForSimpleLedStrip`. All three must be extended to the same length and the `if(result>N)` cap raised, or strip mode reads out of bounds.
- **`.ino` UISlider label string must equal viewer.html `SLIDER_NAME`** (the viewer matches the slider by name) — update both together.
- Spec: `docs/superpowers/specs/2026-06-29-lightcycle-race-design.md`.

## Verification model (read first)

Effects have **no unit tests**; they're verified **visually in the WASM sim** (like every other effect here). Build with PowerShell:
```
$env:Path += ";$env:LOCALAPPDATA\Programs\Python\Python312\Scripts"; Set-Location C:\Users\gethi\source\pixelatedlights; fastled .sim\AllEffects_FastLED --just-compile --no-interactive
```
The trailing `could not locate src/fastled/frontend` line is **expected/harmless**; success = no C++ `error:` lines. **Header-only edits are not picked up by the build cache — touch the `.ino` mtime first** (`(Get-Item .sim\AllEffects_FastLED\AllEffects_FastLED.ino).LastWriteTime = Get-Date`). Then serve via `.sim\run-viewer.ps1` (or an existing node server) and open `http://127.0.0.1:8200/viewer.html`, dragging the Pattern slider to **light cycle race** (notch 21). Each task lists what you must see.

## File Structure

- Create `AllEffects_FastLED/lightCycleRace.h` — the whole effect.
- Mirror `.sim/AllEffects_FastLED/lightCycleRace.h`.
- Modify `AllEffects_FastLED/AllEffects_FastLED.ino` (+ `.sim`) — include, `case 27`, slider max+label.
- Modify `AllEffects_FastLED/effectChanging.h` (+ `.sim`) — append `27` to all three arrays, raise cap.
- Modify `AllEffects_FastLED/viewer.html` (+ `.sim`) — NAMES label, `idx` cap, slider max, row label, `SLIDER_NAME`.

---

### Task 1: Scaffold, helpers, arena render, wiring

**Files:**
- Create: `AllEffects_FastLED/lightCycleRace.h` (+ `.sim` copy)
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (include after `farmhouseSeasons.h`; `case 27`; UISlider `1197→1254`, label `(0-21)→(0-22)`) (+ `.sim`)
- Modify: `AllEffects_FastLED/effectChanging.h` (append `27` to all three arrays before trailing `13`; cap `21→22`) (+ `.sim`)
- Modify: `AllEffects_FastLED/viewer.html` (NAMES + `idx` 22 + slider `1254` + row label `22` + `SLIDER_NAME '(0-22)'`) (+ `.sim`)

**Interfaces:**
- Produces: `enum {LC_EMPTY,LC_RED,LC_BLUE,LC_WALL}`, `enum {LC_RACING,LC_FLASH,LC_PAUSE}`; state `lcGrid[]`, `lcRed/lcBlue` (`LcCycle`), `lcPhase`, `lcRng`, `lcRound`, `lcWinner`, `lcInit`, `lcLastStep`, `lcPhaseStart`; helpers `lcRand()`, `lcGi(x,y)`, `lcClearTrails()`, `lcReset()`, `lcRender()`; entry `lightCycleRace()`.

- [ ] **Step 1: Create the header (state, helpers, arena render, entry)**

Create `AllEffects_FastLED/lightCycleRace.h`:

```cpp
#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Tron-style light-cycle race. Spec:
//   docs/superpowers/specs/2026-06-29-lightcycle-race-design.md
// Stateful sim: occupancy grid + two cycles + phase machine. 60x60.
// ============================================================================
#define LC_STEP_MS        70      // race step interval
#define LC_FLASH_MS       2000    // winner flash duration
#define LC_FLASH_BLINK_MS 200     // winner pulse period
#define LC_PAUSE_MS       500     // blank pause before next round
#define LC_INTERCEPT_K    6       // how far ahead of the opponent the AI aims

enum { LC_EMPTY=0, LC_RED=1, LC_BLUE=2, LC_WALL=3 };
enum { LC_RACING=0, LC_FLASH=1, LC_PAUSE=2 };

struct LcCycle { int8_t x, y, dx, dy; bool alive; };

static uint8_t  lcGrid[NUM_LEDS];          // one cell per LED, indexed by lcGi()
static LcCycle  lcRed, lcBlue;
static uint8_t  lcPhase      = LC_PAUSE;
static uint32_t lcPhaseStart = 0;
static uint32_t lcLastStep   = 0;
static uint32_t lcRng        = 0x1234567u;
static uint16_t lcRound      = 0;
static uint8_t  lcWinner     = 0;          // 0 none/draw, 1 red, 2 blue
static bool     lcInit       = false;

static inline uint32_t lcRand(){ lcRng^=lcRng<<13; lcRng^=lcRng>>17; lcRng^=lcRng<<5; return lcRng; }
static inline int lcGi(int x,int y){ return y*NUM_COLS + x; }

static void lcClearTrails(){
  for(int i=0;i<NUM_LEDS;i++) if(lcGrid[i]!=LC_WALL) lcGrid[i]=LC_EMPTY;
}

static void lcReset(){
  for(int i=0;i<NUM_LEDS;i++) lcGrid[i]=LC_EMPTY;
  for(int x=0;x<NUM_COLS;x++){ lcGrid[lcGi(x,0)]=LC_WALL; lcGrid[lcGi(x,NUM_ROWS-1)]=LC_WALL; }
  for(int y=0;y<NUM_ROWS;y++){ lcGrid[lcGi(0,y)]=LC_WALL; lcGrid[lcGi(NUM_COLS-1,y)]=LC_WALL; }
  lcRound++;
  lcRng = 0x9E3779B9u ^ ((uint32_t)lcRound*2654435761u); if(lcRng==0) lcRng=1;
  int rowR = 12 + (int)(lcRand()%36);                 // 12..47
  int rowB; do { rowB = 12 + (int)(lcRand()%36); } while(abs(rowB-rowR)<4);
  lcRed  = { 8,  (int8_t)rowR,  1, 0, true };
  lcBlue = { 51, (int8_t)rowB, -1, 0, true };
  lcGrid[lcGi(lcRed.x, lcRed.y)]   = LC_RED;
  lcGrid[lcGi(lcBlue.x,lcBlue.y)]  = LC_BLUE;
  lcWinner = 0;
}

static void lcRender(){
  for(int y=0;y<NUM_ROWS;y++) for(int x=0;x<NUM_COLS;x++){
    uint8_t g=lcGrid[lcGi(x,y)];
    CRGB c = CRGB(0x0A0E1A);
    if(g==LC_WALL)      c = CRGB(0x2A3040);
    else if(g==LC_RED)  c = CRGB(0xE03030);
    else if(g==LC_BLUE) c = CRGB(0x3060E0);
    leds[XY((uint8_t)x,(uint8_t)y)] = c;
  }
}

void lightCycleRace(){
  if(!lcInit){ lcInit=true; lcReset(); lcPhase=LC_RACING; lcLastStep=millis(); }
  lcRender();
  FastLED.show();
}
```

- [ ] **Step 2: Wire into the sketch**

In `AllEffects_FastLED/AllEffects_FastLED.ino` add the include after `farmhouseSeasons.h`:

```cpp
#include "farmhouseSeasons.h"
#include "lightCycleRace.h"
```

Add the case after `case 26` in the 50 ms switch:

```cpp
    case 26:
      farmhouseSeasons();
      break;
    case 27:
      lightCycleRace();
      break;
```

Bump the sim slider (22 notches → 22*57 = 1254) and relabel:

```cpp
fl::UISlider effectSlider("Pattern (0-22)", 0, 0, 1254, 57);
```

- [ ] **Step 3: Register the effect id in all three tables**

In `AllEffects_FastLED/effectChanging.h`, append `27` before the trailing `13` in **all three** arrays and raise the cap:

```cpp
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 27, 13};
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 27, 13};
  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13,  0,  0,  0,  0,  0,  0,  0,  0,  0};
```

Raise the cap:

```cpp
  if (result > 22)
  {
    result = 22;
  }
```

- [ ] **Step 4: Viewer label + caps**

In `AllEffects_FastLED/viewer.html`: add the label before `'jusBlack (off)'`:

```javascript
                 'water lilies','kusama dots','ocean sunrise','farmhouse seasons','light cycle race','jusBlack (off)'];
```

Raise the index clamp:

```javascript
  const idx = v => Math.min(22, Math.round(v / 57));
```

Update the `SLIDER_NAME` to match the `.ino` label:

```javascript
  const SLIDER_NAME      = 'Pattern (0-22)';   // UISlider name; UI manager matches by name
```

Change the pattern slider `max` and the row end-label:

```html
        <input type="range" id="pattern" min="0" max="1254" step="57" value="0">
        <div class="row"><span>0</span><span id="patVal">0</span><span>22</span></div>
```

- [ ] **Step 5: Mirror to `.sim`**

```powershell
Set-Location C:\Users\gethi\source\pixelatedlights
Copy-Item AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h -Force
Copy-Item AllEffects_FastLED/AllEffects_FastLED.ino .sim/AllEffects_FastLED/AllEffects_FastLED.ino -Force
Copy-Item AllEffects_FastLED/effectChanging.h .sim/AllEffects_FastLED/effectChanging.h -Force
Copy-Item AllEffects_FastLED/viewer.html .sim/AllEffects_FastLED/viewer.html -Force
```

- [ ] **Step 6: Build and verify the arena is selectable**

Run the build command from the Verification model. Expected: no `error:` lines. Open the viewer, select **light cycle race** (slider notch 21, value 1197): you should see a dark-navy arena with a grey 1-pixel border, plus one red pixel (left) and one blue pixel (right). (No movement yet.)

- [ ] **Step 7: Commit**

```bash
git add AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h \
        AllEffects_FastLED/AllEffects_FastLED.ino .sim/AllEffects_FastLED/AllEffects_FastLED.ino \
        AllEffects_FastLED/effectChanging.h .sim/AllEffects_FastLED/effectChanging.h \
        AllEffects_FastLED/viewer.html .sim/AllEffects_FastLED/viewer.html
git commit -m "feat(lightcycle): scaffold lightCycleRace effect + wiring"
```

---

### Task 2: Movement, trails, heads, wall-crash (straight only)

**Files:**
- Modify: `AllEffects_FastLED/lightCycleRace.h` (+ `.sim`)

**Interfaces:**
- Consumes: state + helpers from Task 1.
- Produces: `void lcStepStraight()` (advance both cycles straight, lay trails, crash on non-empty/out-of-bounds), head rendering in `lcRender()`, and the RACING step gate + crash→freeze in `lightCycleRace()`.

- [ ] **Step 1: Add straight-only stepping and crash, draw heads, gate the step**

In `lightCycleRace.h`, add before `lightCycleRace()`:

```cpp
// move a cycle straight; returns false (crash) if the next cell is blocked
static bool lcAdvance(LcCycle& c, uint8_t colour){
  int nx=c.x+c.dx, ny=c.y+c.dy;
  if(nx<0||nx>=NUM_COLS||ny<0||ny>=NUM_ROWS || lcGrid[lcGi(nx,ny)]!=LC_EMPTY){ c.alive=false; return false; }
  c.x=nx; c.y=ny; lcGrid[lcGi(nx,ny)]=colour; return true;
}
static void lcStepStraight(){
  bool r = lcAdvance(lcRed,  LC_RED);
  bool b = lcAdvance(lcBlue, LC_BLUE);
  if(!r || !b){ lcWinner = (!r&&!b)?0 : (!r?2:1); lcPhase=LC_FLASH; lcPhaseStart=millis(); }
}
```

Add head drawing at the end of `lcRender()` (before the closing brace it currently has — i.e. after the grid loop):

```cpp
  if(lcPhase==LC_RACING){
    if(lcRed.alive)  leds[XY((uint8_t)lcRed.x,(uint8_t)lcRed.y)]   = CRGB(0xFFE0D8);
    if(lcBlue.alive) leds[XY((uint8_t)lcBlue.x,(uint8_t)lcBlue.y)] = CRGB(0xD8E4FF);
  }
```

Replace the body of `lightCycleRace()` with the stepped version:

```cpp
void lightCycleRace(){
  uint32_t t=millis();
  if(!lcInit){ lcInit=true; lcReset(); lcPhase=LC_RACING; lcLastStep=t; }
  if(lcPhase==LC_RACING && t-lcLastStep >= LC_STEP_MS){ lcLastStep=t; lcStepStraight(); }
  lcRender();
  FastLED.show();
}
```

- [ ] **Step 2: Mirror, build, verify**

Mirror `lightCycleRace.h` to `.sim`, touch the `.ino` mtime, rebuild. In the viewer: red drives right and blue drives left, each leaving a solid trail and a bright head, then each stops dead when it reaches the far wall (the scene then freezes — phase machine comes in Task 4).

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h
git commit -m "feat(lightcycle): cycle movement, trails, heads, wall crash"
```

---

### Task 3: Aggressive cut-off AI

**Files:**
- Modify: `AllEffects_FastLED/lightCycleRace.h` (+ `.sim`)

**Interfaces:**
- Consumes: state + helpers.
- Produces: `bool lcChoose(const LcCycle& c, const LcCycle& opp, int8_t& outdx, int8_t& outdy)` and `void lcStep()` (replaces `lcStepStraight()` as the RACING stepper; handles both cycles deciding from the pre-step grid + head-on draws).

- [ ] **Step 1: Add the AI chooser and the real step**

Add before `lightCycleRace()` (you may leave `lcStepStraight`/`lcAdvance` in place, but `lightCycleRace` will now call `lcStep`):

```cpp
// pick a move (straight / left-turn / right-turn) that stays alive and cuts toward
// a point ahead of the opponent; returns false if no safe move (crash). y is down,
// so right-turn = (dx,dy)->(-dy,dx), left-turn = (dx,dy)->(dy,-dx).
static bool lcChoose(const LcCycle& c, const LcCycle& opp, int8_t& outdx, int8_t& outdy){
  int8_t cand[3][2] = { {c.dx,c.dy}, {(int8_t)c.dy,(int8_t)-c.dx}, {(int8_t)-c.dy,(int8_t)c.dx} };
  int ix = opp.x + opp.dx*LC_INTERCEPT_K; if(ix<1)ix=1; if(ix>NUM_COLS-2)ix=NUM_COLS-2;
  int iy = opp.y + opp.dy*LC_INTERCEPT_K; if(iy<1)iy=1; if(iy>NUM_ROWS-2)iy=NUM_ROWS-2;
  float best=-1e9f; bool found=false;
  for(int k=0;k<3;k++){
    int nx=c.x+cand[k][0], ny=c.y+cand[k][1];
    if(nx<0||nx>=NUM_COLS||ny<0||ny>=NUM_ROWS) continue;
    if(lcGrid[lcGi(nx,ny)]!=LC_EMPTY) continue;
    int open=0, ox=nx, oy=ny;                          // empty run straight ahead (anti dead-end)
    for(int s=0;s<8;s++){ ox+=cand[k][0]; oy+=cand[k][1];
      if(ox<0||ox>=NUM_COLS||oy<0||oy>=NUM_ROWS||lcGrid[lcGi(ox,oy)]!=LC_EMPTY) break; open++; }
    float score = -(float)(abs(nx-ix)+abs(ny-iy)) + open*0.6f + (float)(lcRand()%5)*0.1f;
    if(k==0) score += 0.2f;                            // mild straight preference
    if(score>best){ best=score; outdx=cand[k][0]; outdy=cand[k][1]; found=true; }
  }
  return found;
}
static void lcStep(){
  int8_t rdx=0,rdy=0,bdx=0,bdy=0;
  bool rOk = lcChoose(lcRed,  lcBlue, rdx,rdy);
  bool bOk = lcChoose(lcBlue, lcRed,  bdx,bdy);
  int rnx=lcRed.x+rdx,  rny=lcRed.y+rdy;
  int bnx=lcBlue.x+bdx, bny=lcBlue.y+bdy;
  bool rCrash=!rOk, bCrash=!bOk;
  if(rOk && bOk && rnx==bnx && rny==bny){ rCrash=true; bCrash=true; }   // head-on into same cell
  if(!rCrash){ lcRed.dx=rdx; lcRed.dy=rdy; lcRed.x=rnx; lcRed.y=rny; lcGrid[lcGi(rnx,rny)]=LC_RED; }
  else lcRed.alive=false;
  if(!bCrash){ lcBlue.dx=bdx; lcBlue.dy=bdy; lcBlue.x=bnx; lcBlue.y=bny; lcGrid[lcGi(bnx,bny)]=LC_BLUE; }
  else lcBlue.alive=false;
  if(rCrash||bCrash){ lcWinner = (rCrash&&bCrash)?0 : (rCrash?2:1); lcPhase=LC_FLASH; lcPhaseStart=millis(); }
}
```

Change the step call in `lightCycleRace()` from `lcStepStraight()` to `lcStep()`:

```cpp
  if(lcPhase==LC_RACING && t-lcLastStep >= LC_STEP_MS){ lcLastStep=t; lcStep(); }
```

- [ ] **Step 2: Mirror, build, verify**

Mirror + touch `.ino` + rebuild. The cycles now **turn**, curving toward each other and cutting the other off; a round ends when one drives into a wall/trail and the scene freezes. Watch a few selections (re-select the pattern to restart `lcInit`? no — it freezes; full looping is Task 4). Confirm both cycles steer and crash on trails, not just walls.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h
git commit -m "feat(lightcycle): aggressive cut-off AI steering"
```

---

### Task 4: Round phase machine — flash winner, pause, new round

**Files:**
- Modify: `AllEffects_FastLED/lightCycleRace.h` (+ `.sim`)

**Interfaces:**
- Consumes: `lcStep`, `lcReset`, `lcClearTrails`, `lcWinner`, phase state.
- Produces: FLASH/PAUSE handling in `lightCycleRace()`, and flash/dim rendering in `lcRender()`.

- [ ] **Step 1: Add flash rendering and the phase transitions**

In `lcRender()`, replace the trail colour assignment (the `else if(g==LC_RED)…/else if(g==LC_BLUE)…` lines) with flash-aware colouring:

```cpp
    uint8_t g=lcGrid[lcGi(x,y)];
    CRGB c = CRGB(0x0A0E1A);
    if(g==LC_WALL) c = CRGB(0x2A3040);
    else if(g==LC_RED || g==LC_BLUE){
      CRGB base = (g==LC_RED)?CRGB(0xE03030):CRGB(0x3060E0);
      if(lcPhase==LC_FLASH){
        bool blinkOn = ((millis()/LC_FLASH_BLINK_MS)&1)==0;
        if(lcWinner==0 || lcWinner==g){ c = blinkOn ? CRGB(0xE8ECF4) : base; }  // winner/draw pulses white
        else { c = base; c.nscale8(80); }                                       // loser dimmed ~31%
      } else c = base;
    }
    leds[XY((uint8_t)x,(uint8_t)y)] = c;
```

(The grid loop's `for` headers and the head-drawing block stay as they are.)

Update `lightCycleRace()` to the full phase machine:

```cpp
void lightCycleRace(){
  uint32_t t=millis();
  if(!lcInit){ lcInit=true; lcReset(); lcPhase=LC_RACING; lcLastStep=t; }
  if(lcPhase==LC_RACING){
    if(t-lcLastStep >= LC_STEP_MS){ lcLastStep=t; lcStep(); }
  } else if(lcPhase==LC_FLASH){
    if(t-lcPhaseStart >= LC_FLASH_MS){ lcClearTrails(); lcPhase=LC_PAUSE; lcPhaseStart=t; }
  } else { // LC_PAUSE
    if(t-lcPhaseStart >= LC_PAUSE_MS){ lcReset(); lcPhase=LC_RACING; lcLastStep=t; }
  }
  lcRender();
  FastLED.show();
}
```

- [ ] **Step 2: Mirror, build, verify the full loop**

Mirror + touch `.ino` + rebuild. Watch a full cycle: race → one crashes → **winner's trail pulses** (~2 s) while the loser dims → arena clears (~0.5 s blank) → **new round** starts with different start rows. Confirm it loops indefinitely and a draw (rare) pulses white.

- [ ] **Step 3: Commit**

```bash
git add AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h
git commit -m "feat(lightcycle): round phase machine - flash winner, pause, restart"
```

---

### Task 5: Head glow + tuning pass

**Files:**
- Modify: `AllEffects_FastLED/lightCycleRace.h` (+ `.sim`)

**Interfaces:**
- Consumes: everything. Produces: nicer heads + tuned constants. No new public functions.

- [ ] **Step 1: Add a soft glow around each head**

In `lcRender()`, replace the RACING head block with a core + 4-neighbour glow:

```cpp
  if(lcPhase==LC_RACING){
    const int8_t nb[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    if(lcRed.alive){
      for(int k=0;k<4;k++){ int gx=lcRed.x+nb[k][0], gy=lcRed.y+nb[k][1];
        if(gx>=0&&gx<NUM_COLS&&gy>=0&&gy<NUM_ROWS) leds[XY((uint8_t)gx,(uint8_t)gy)] += CRGB(0x401810); }
      leds[XY((uint8_t)lcRed.x,(uint8_t)lcRed.y)] = CRGB(0xFFE0D8);
    }
    if(lcBlue.alive){
      for(int k=0;k<4;k++){ int gx=lcBlue.x+nb[k][0], gy=lcBlue.y+nb[k][1];
        if(gx>=0&&gx<NUM_COLS&&gy>=0&&gy<NUM_ROWS) leds[XY((uint8_t)gx,(uint8_t)gy)] += CRGB(0x101840); }
      leds[XY((uint8_t)lcBlue.x,(uint8_t)lcBlue.y)] = CRGB(0xD8E4FF);
    }
  }
```

(`+=` is FastLED's saturating add, so the glow brightens trail/background without overflow.)

- [ ] **Step 2: Mirror, build, watch a few rounds; tune if needed**

Mirror + touch `.ino` + rebuild. Watch several rounds. The heads should read as glowing leaders. If races end too fast or the AI stalls/mirrors, adjust only the named constants: `LC_STEP_MS` (speed), `LC_INTERCEPT_K` (aggression aim), the `open*0.6f` weight (self-preservation), or the `(lcRand()%5)*0.1f` jitter (variety). Make only the changes your observation calls for; show each changed line in the commit.

- [ ] **Step 3: Confirm root/.sim parity and commit**

```bash
git diff --no-index AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h   # expect no diff
git add AllEffects_FastLED/lightCycleRace.h .sim/AllEffects_FastLED/lightCycleRace.h
git commit -m "feat(lightcycle): head glow + tuning"
```

---

## Per-Task Confidence (post-lift pass)

Confidence = likelihood the task lands correctly first pass. Lift pass: anything <90% gets a mitigation; riskiest flagged.

| Task | Confidence | Risk / mitigation |
|------|-----------|-------------------|
| T1 Scaffold + wiring | 95% | Wiring verified against the PR #5 lessons (3 selector arrays, cap, slider/label, `SLIDER_NAME` sync). Pure scaffolding. |
| T2 Movement/trails/heads | 94% | Simple grid stepping; `lcAdvance` crash check is straightforward. Visual check is unambiguous. |
| **T3 Aggressive-cut-off AI** ⚠️ | **88%** | **Riskiest.** Turn handedness (left/right rotation in y-down coords) and the both-decide-from-pre-step-grid + head-on rule are the error-prone parts. Mitigations: the turn vectors are written explicitly with the y-down note; head-on (same dest cell) handled in `lcStep`; AI *feel* (too aggressive / stalls) is pure constant-tuning in T5, not a structural risk. Worst case the AI is bland — still a valid race. |
| T4 Phase machine | 92% | Three-state millis machine; `lcClearTrails` keeps walls. Mild risk: pause showing stale frame — mitigated by clearing trails on FLASH→PAUSE. |
| T5 Head glow + tuning | 92% | `+=` saturating add is the right primitive; the rest is observation-driven constant tuning with named knobs. |

**Min confidence after lift: 88% (T3 AI).** The one flagged task degrades gracefully (a weak AI still produces a watchable race) and all its feel-risk is isolated to tunable constants in T5.

## Self-Review

**Spec coverage:** arena+border+bg → T1; grid/PRNG/state → T1; cycles+movement+trails+heads → T2; aggressive-cut-off AI (safety filter, intercept scoring, jitter, no-reverse) → T3; crash (border/own/opponent/head-on draw) → T2 (wall) + T3 (trails/head-on); round lifecycle (FLASH pulse, PAUSE blank, reset+reseed+new start rows) → T4; start positions (opposite sides, row-offset, RNG) → `lcReset` in T1; rendering/colours → T1/T2/T4/T5; wiring (id 27, three arrays, caps, viewer, `SLIDER_NAME`) → T1; timing constants → T1 `#define`s. ✓ No gaps.

**Placeholder scan:** none. The only observation-driven step is T5 Step 2 (constant tuning), constrained to named constants.

**Type consistency:** `lcGi/lcRand/lcReset/lcClearTrails/lcRender/lcAdvance/lcStepStraight/lcChoose/lcStep` and the `LcCycle` fields (`x,y,dx,dy,alive`) and globals (`lcGrid,lcRed,lcBlue,lcPhase,lcPhaseStart,lcLastStep,lcRng,lcRound,lcWinner,lcInit`) are used consistently across tasks. `lightCycleRace()` accretes calls; `lcStep` supersedes `lcStepStraight` as the RACING stepper in T3.
