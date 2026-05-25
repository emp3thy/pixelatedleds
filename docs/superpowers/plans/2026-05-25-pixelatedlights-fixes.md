# PixelatedLights Quality Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 19 quality issues in the `AllEffects_FastLED` Arduino sketch — 8 are real bugs (wrong serpentine math, dead noise effects, fire that never reheats, integer overflow, missing `FastLED.show()`, undefined-behaviour return statements, Arduino IDE folder/file mismatch), 11 are hygiene issues (no header guards, global naming collisions, typos, redundant code).

**Architecture:** Single Arduino sketch targeting Uno/Nano-class AVR boards driving WS2811 LEDs via FastLED 3.3.3. All effects share globals (`leds[]`, `gHue`, palettes); each effect lives in one `.h` file `#include`d once from the main `.ino`. Fixes are edits to existing files plus two file renames. No new modules, no architectural rework.

**Tech Stack:** Arduino C++ (AVR), FastLED 3.3.3 (vendored in `AllEffects_FastLED/libraries/`), `arduino-cli` for compile validation, Git for change tracking.

**Verification model:** Embedded sketch — no host-side test framework. The "tests pass" gate for each task is **`arduino-cli compile` succeeds without warnings or errors against the unchanged baseline**. Visual confirmation on hardware is the final acceptance gate but is performed once at the end (Task 11) rather than after every step.

**Toolchain prerequisites (Task 0 sets these up):**
- `git` (already installed)
- `arduino-cli` (not currently installed — Task 0 installs it)
- Arduino AVR core (`arduino:avr`) installed via `arduino-cli core install arduino:avr`

If the engineer cannot install `arduino-cli`, the compile-verify steps can be performed manually inside the Arduino IDE (`Sketch → Verify`); the plan calls this out per task.

---

## File Structure

All edits are within `C:\Users\gethi\source\pixelatedlights\AllEffects_FastLED\`. The `build/` folder at the project root is stale Arduino IDE output and is deleted in Task 0.

```
pixelatedlights/
├── .gitignore                  (Task 0 — new)
├── AllEffects_FastLED/
│   ├── AllEffects_FastLED.ino  (Task 1 — renamed from SquareMatrix.ino)
│   ├── configuration.h         (Task 1 — renamed from configuation.h)
│   ├── XYMatrix.h              (Task 2 — fix serpentine; Task 9 — header guard)
│   ├── effectChanging.h        (Task 9 — header guard; Task 10 — static const arrays)
│   ├── noisePatterns.h         (Task 8 — renamed from noise.h; fix bounds + XY args + return types; Task 9 — header guard)
│   ├── fire.h                  (Task 4 — reheat fix; Task 9 — header guard + rename `rows`/`cols`/`t`)
│   ├── rain.h                  (Task 5 — speed overflow; Task 9 — header guard)
│   ├── simplePatterns.h        (Task 6 — fadeIn show; Task 9 — header guard)
│   ├── pacifica.h              (Task 9 — header guard)
│   ├── pride.h                 (Task 9 — header guard; Task 10 — drop pixelnumber reorder)
│   ├── metaBalls.h             (Task 9 — header guard; Task 10 — drop gHue clobber)
│   └── libraries/              (unchanged — vendored FastLED + MultiButton)
└── docs/superpowers/plans/2026-05-25-pixelatedlights-fixes.md
```

---

## Task 0: Toolchain & Repo Setup

**Files:**
- Create: `C:\Users\gethi\source\pixelatedlights\.gitignore`
- Delete: `C:\Users\gethi\source\pixelatedlights\build\` (stale Arduino IDE output)

- [ ] **Step 1: Initialise Git repo**

```bash
cd /c/Users/gethi/source/pixelatedlights
git init
git config user.email "emp3thy@googlemail.com"
git config user.name "gethi"
```

- [ ] **Step 2: Add `.gitignore`**

Create `C:\Users\gethi\source\pixelatedlights\.gitignore` with:

```
build/
*.elf
*.hex
*.eep
*.bin
*.o
*.d
*.with_bootloader.bin
*.with_bootloader.hex
.vscode/
.idea/
```

- [ ] **Step 3: Delete stale build artefacts**

```bash
rm -rf /c/Users/gethi/source/pixelatedlights/build
```

- [ ] **Step 4: Install `arduino-cli`**

Run (PowerShell):

```powershell
winget install --id ArduinoSA.CLI -e
```

Verify:

```powershell
arduino-cli version
```

Expected: prints a version (e.g. `arduino-cli Version: 1.x.x`).

If `winget` is unavailable, download the Windows ZIP from <https://github.com/arduino/arduino-cli/releases> and put `arduino-cli.exe` on `PATH`.

- [ ] **Step 5: Install AVR core**

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
```

Verify:

```bash
arduino-cli core list
```

Expected output contains a line beginning with `arduino:avr`.

- [ ] **Step 6: Baseline compile (capture pre-existing warnings before any edit)**

```bash
arduino-cli compile --fqbn arduino:avr:uno --warnings all --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED 2>&1 | tee /c/Users/gethi/source/pixelatedlights/docs/superpowers/plans/baseline-compile.log
```

Expected: compile fails OR succeeds-with-warnings. Either way, the log is the baseline for diffing later. Note the sketch may currently fail to compile because the folder name (`AllEffects_FastLED`) does not match the `.ino` filename (`SquareMatrix.ino`); that is exactly the bug Task 1 fixes.

- [ ] **Step 7: Baseline commit**

```bash
git add -A
git commit -m "chore: import pixelatedlights baseline"
```

Expected: one commit with all source files staged.

---

## Task 1: Fix Arduino sketch folder/file naming + `configuation.h` typo

**Why:** Arduino IDE / `arduino-cli` require the primary `.ino` filename to match its containing folder name. Folder is `AllEffects_FastLED`; current `.ino` is `SquareMatrix.ino` → sketch won't open / won't compile. Also `configuation.h` is a typo of `configuration.h`.

**Files:**
- Rename: `AllEffects_FastLED/SquareMatrix.ino` → `AllEffects_FastLED/AllEffects_FastLED.ino`
- Rename: `AllEffects_FastLED/configuation.h` → `AllEffects_FastLED/configuration.h`
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (line 31 — change `#include "configuation.h"` to `#include "configuration.h"`)

- [ ] **Step 1: Rename `.ino` to match folder**

```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
git mv SquareMatrix.ino AllEffects_FastLED.ino
```

- [ ] **Step 2: Rename config header**

```bash
git mv configuation.h configuration.h
```

- [ ] **Step 3: Update `#include` to new config name**

Edit `AllEffects_FastLED/AllEffects_FastLED.ino` line 31:

```diff
- #include "configuation.h"
+ #include "configuration.h"
```

- [ ] **Step 4: Compile to verify rename works**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compile no longer fails on the folder/filename mismatch. Other pre-existing issues may still report warnings; that is fine.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "fix: rename sketch file to match folder and fix configuration.h typo"
```

---

## Task 2: Fix XYMatrix serpentine formula

**Why:** `XYMatrix.h:11` computes `x * NUM_ROWS + (COL_OFFSET - y)` with `COL_OFFSET = 35`, `NUM_ROWS = 6`. For odd column `x = 1`, `y = 0`, this returns `1*6 + 35 - 0 = 41`, but the correct serpentine index for that cell is `1*6 + 5 - 0 = 11`. The constant should be `NUM_ROWS - 1` (=5), not 35. Comments in `configuration.h` corroborate (`COL_OFFSET 9 for 10×10`, `COL_OFFSET 5 for 5×20`). Either fix the constant or, cleaner, drop `COL_OFFSET` and inline `NUM_ROWS - 1`.

**Files:**
- Modify: `AllEffects_FastLED/XYMatrix.h` (lines 5-13 — inline `NUM_ROWS - 1`)
- Modify: `AllEffects_FastLED/configuration.h` (remove `COL_OFFSET` lines)

- [ ] **Step 1: Replace `XY()` body with correct serpentine math**

Replace the entire body of `AllEffects_FastLED/XYMatrix.h` with:

```c
#pragma once
#include <FastLED.h>
#include "configuration.h"

uint16_t XY(uint8_t x, uint8_t y)
{
  if (ISMATRIX)
  {
    if ((x % 2) == 0)
    {
      return x * NUM_ROWS + y;
    }
    return x * NUM_ROWS + (NUM_ROWS - 1 - y);
  }
  return x * 10 + y;
}
```

- [ ] **Step 2: Remove `COL_OFFSET` from `configuration.h`**

Replace the entire body of `AllEffects_FastLED/configuration.h` with:

```c
#pragma once

// for differentiating between doing a line and a matrix
#define ISMATRIX true
#define NUM_LEDS 216
#define NUM_ROWS 6
#define NUM_COLS 36
```

- [ ] **Step 3: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly (no new errors introduced; warnings same as baseline or fewer).

- [ ] **Step 4: Sanity-check the math by hand**

For `NUM_ROWS = 6`, `NUM_COLS = 36`:
- `XY(0, 0)` → `0` (col 0, row 0)
- `XY(0, 5)` → `5` (col 0, row 5)
- `XY(1, 0)` → `1*6 + (6-1-0) = 11` (col 1, row 0 — last LED of col 1, serpentine)
- `XY(1, 5)` → `1*6 + (6-1-5) = 6` (col 1, row 5 — first LED of col 1)
- `XY(35, 5)` → `35*6 + (6-1-5) = 210`
- `XY(35, 0)` → `35*6 + 5 = 215` = `NUM_LEDS - 1` ✓

All indices in `[0, NUM_LEDS - 1]`. No out-of-bounds.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "fix: correct XYMatrix serpentine offset (NUM_ROWS-1, not COL_OFFSET)"
```

---

## Task 3: Fix noise.h XY argument swap + bounds check

**Why:** `noise.h` lines 167-172 contain two bugs:

```c
uint16_t n = XY(i, j);              // BUG: i is row, j is col — XY expects (x=col, y=row), so args are swapped
if (n > -1 && n < NUM_ROWS)         // BUG: n is uint16_t so > -1 is always true; and bound is NUM_ROWS (=6) instead of NUM_LEDS (=216)
{
  leds[n] = color;
}
```

The bounds bug means noise effects only write LEDs 0..5 — most of the panel stays black during cases 4, 5, 8, 10. The argument-swap bug means even those 6 LEDs map to the wrong physical cells.

**Files:**
- Modify: `AllEffects_FastLED/noise.h` (lines 167-172) — note that this file is renamed to `noisePatterns.h` in Task 8, but is edited under the old name here

- [ ] **Step 1: Edit `mapNoiseToLEDsUsingPalette` to use correct XY args + correct bound**

In `AllEffects_FastLED/noise.h`, replace lines 167-172:

```diff
-      CRGB color = ColorFromPalette(palette, index, bri);
-      uint16_t n = XY(i, j);
-
-      if (n > -1 && n < NUM_ROWS)
-      {
-        leds[n] = color;
-      }
+      CRGB color = ColorFromPalette(palette, index, bri);
+      uint16_t n = XY(j, i);
+
+      if (n < NUM_LEDS)
+      {
+        leds[n] = color;
+      }
```

- [ ] **Step 2: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly. (Visual confirmation that noise effects now light the whole panel happens at Task 11.)

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "fix(noise): correct XY argument order and use NUM_LEDS bound"
```

---

## Task 4: Fix fire.h bottom-row reheat

**Why:** `fire.h:165-172` reheats the bottom row only when `pix[0][j] > 0`. `pix[][]` is zero-initialised, so cold cells never reignite — the fire effect runs cold and only flares (Step `newflare()`) ever inject heat. Intent of "Heat the bottom row" is to always seed the floor regardless of prior value.

**Files:**
- Modify: `AllEffects_FastLED/fire.h` (lines 164-172)

- [ ] **Step 1: Always reseed bottom row**

In `AllEffects_FastLED/fire.h`, replace lines 164-172:

```diff
-  // Heat the bottom row
-  for (j = 0; j < cols; ++j)
-  {
-    i = pix[0][j];
-    if (i > 0)
-    {
-      pix[0][j] = random(NCOLORS - 6, NCOLORS - 2);
-    }
-  }
+  // Heat the bottom row — always reseed so cold cells reignite
+  for (j = 0; j < cols; ++j)
+  {
+    pix[0][j] = random(NCOLORS - 6, NCOLORS - 2);
+  }
```

- [ ] **Step 2: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "fix(fire): always reseed bottom row so cold cells reignite"
```

---

## Task 5: Fix rain.h `speed` overflow

**Why:** `rain.h:4` declares `int speed = 2;` and `rain.h:18` does `speed++` each frame. Signed int overflow after ~32 767 frames is undefined behaviour. The value is only ever used inside `(j + speed + random8(2) + NUM_ROWS) % NUM_ROWS` so a `byte` modulo `NUM_ROWS` suffices.

**Files:**
- Modify: `AllEffects_FastLED/rain.h` (lines 4, 18)

- [ ] **Step 1: Change `speed` to `byte`, wrap modulo `NUM_ROWS`**

In `AllEffects_FastLED/rain.h`, replace line 4:

```diff
-int speed = 2;
+byte speed = 2;
```

And line 18 (inside `updaterain`):

```diff
-  speed ++;
+  speed = (speed + 1) % NUM_ROWS;
```

- [ ] **Step 2: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "fix(rain): wrap speed modulo NUM_ROWS to avoid signed overflow UB"
```

---

## Task 6: Fix `fadeIn` missing `FastLED.show()`

**Why:** `simplePatterns.h:20-31` `fadeIn()` writes to `leds[]` but never calls `FastLED.show()`. Effect 17 in `loop()` dispatches `fadeIn()` (every 20 ms), `movePaletteToPalette()` (every 150 ms — also no `show`), and `generateRandomTargetPalette()` (every 5 s — no `show`). Net result: effect 17 never pushes pixels until some other effect runs.

**Files:**
- Modify: `AllEffects_FastLED/simplePatterns.h` (lines 20-31)

- [ ] **Step 1: Add `FastLED.show()` to `fadeIn`**

In `AllEffects_FastLED/simplePatterns.h`, replace lines 20-31:

```diff
 void fadeIn() {

   random16_set_seed(535);                                                           // The randomizer needs to be re-set each time through the loop in order for the 'random' numbers to be the same each time through.

   for (int i = 0; i<NUM_LEDS; i++) {
     uint8_t fader = sin8(millis()/random8(10,20));                                  // The random number for each 'i' will be the same every time.
     leds[i] = ColorFromPalette(currentPalette, i*20, fader, currentBlending);       // Now, let's run it through the palette lookup.
   }

   random16_set_seed(millis());                                                      // Re-randomizing the random number seed for other routines.
+
+  FastLED.show();

 } // fadein()
```

- [ ] **Step 2: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "fix(simplePatterns): call FastLED.show() in fadeIn so effect 17 renders"
```

---

## Task 7: Fix noise.h `uint16_t`-returning functions with no return statement

**Why:** `rainbowNoise`, `rainbowStripeNoise`, `partyNoise`, `forestNoise`, `cloudNoise`, `lavaNoise`, `oceanNoise`, `blackAndWhiteNoise`, `blackAndBlueNoise` are all declared `uint16_t` but contain no `return` statement. Falling off a non-void function is undefined behaviour in C++. They are also only ever called for their side effects (drawing noise), so the return type should be `void`. `fireNoise` is already `void`.

**Files:**
- Modify: `AllEffects_FastLED/noise.h` (lines 189, 199, 209, 219, 229, 249, 259, 269, 280)

- [ ] **Step 1: Change all 9 noise-effect wrappers from `uint16_t` to `void`**

In `AllEffects_FastLED/noise.h`, change each of the following function signatures (lines 189, 199, 209, 219, 229, 249, 259, 269, 280) from `uint16_t` to `void`:

```diff
-uint16_t rainbowNoise()
+void rainbowNoise()
```

```diff
-uint16_t rainbowStripeNoise()
+void rainbowStripeNoise()
```

```diff
-uint16_t partyNoise()
+void partyNoise()
```

```diff
-uint16_t forestNoise()
+void forestNoise()
```

```diff
-uint16_t cloudNoise()
+void cloudNoise()
```

```diff
-uint16_t lavaNoise()
+void lavaNoise()
```

```diff
-uint16_t oceanNoise()
+void oceanNoise()
```

```diff
-uint16_t blackAndWhiteNoise()
+void blackAndWhiteNoise()
```

```diff
-uint16_t blackAndBlueNoise()
+void blackAndBlueNoise()
```

(`fireNoise` on line 239 is already `void` — do not touch it.)

- [ ] **Step 2: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly. The `-Wreturn-type` warning that the baseline log captured should now be gone for these functions.

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "fix(noise): change side-effecting wrappers from uint16_t to void"
```

---

## Task 8: Rename `noise.h` → `noisePatterns.h`

**Why:** Local `noise.h` collides with FastLED's `<noise.h>`. The main `.ino` `#include`s both with different quote styles (`<noise.h>` line 23 picks FastLED's, `"noise.h"` line 52 picks the local one), which works today but is fragile — any reordering or include-path tweak silently breaks. Rename the local file.

**Files:**
- Rename: `AllEffects_FastLED/noise.h` → `AllEffects_FastLED/noisePatterns.h`
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (line 52)

- [ ] **Step 1: Rename the file via git**

```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
git mv noise.h noisePatterns.h
```

- [ ] **Step 2: Update the include in the main sketch**

Edit `AllEffects_FastLED/AllEffects_FastLED.ino` line 52:

```diff
-#include "noise.h"
+#include "noisePatterns.h"
```

- [ ] **Step 3: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly.

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "refactor: rename local noise.h to noisePatterns.h to avoid FastLED clash"
```

---

## Task 9: Add `#pragma once` to every header

**Why:** None of the local headers have include guards. They work only because each is `#include`d exactly once from the main `.ino`. Add `#pragma once` to all of them for safety. (`XYMatrix.h` and `configuration.h` already got `#pragma once` in Task 2.)

**Files:**
- Modify (add `#pragma once` as line 1): `AllEffects_FastLED/effectChanging.h`, `AllEffects_FastLED/metaBalls.h`, `AllEffects_FastLED/simplePatterns.h`, `AllEffects_FastLED/pacifica.h`, `AllEffects_FastLED/pride.h`, `AllEffects_FastLED/rain.h`, `AllEffects_FastLED/fire.h`, `AllEffects_FastLED/noisePatterns.h`

- [ ] **Step 1: Add `#pragma once` to `effectChanging.h`**

In `AllEffects_FastLED/effectChanging.h`, prepend `#pragma once` so the file starts:

```c
#pragma once
#include <FastLED.h>

int lastSetResistence = -20;
...
```

- [ ] **Step 2: Add `#pragma once` to `metaBalls.h`**

Prepend `#pragma once` (and add the FastLED include that the file currently lacks but implicitly relies on):

```c
#pragma once
#include <FastLED.h>

byte dist (uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)  {
...
```

- [ ] **Step 3: Add `#pragma once` to `simplePatterns.h`**

```c
#pragma once
#include <FastLED.h>

CRGBPalette16 currentPalette = PartyColors_p;
...
```

- [ ] **Step 4: Add `#pragma once` to `pacifica.h`**

```c
#pragma once
#include <FastLED.h>
// *** PACIFICA ****
...
```

- [ ] **Step 5: Add `#pragma once` to `pride.h`**

```c
#pragma once
#include <FastLED.h>
//*** - Pride rainbows
...
```

- [ ] **Step 6: Add `#pragma once` to `rain.h`**

```c
#pragma once
#include <FastLED.h>
//*** Rain
...
```

- [ ] **Step 7: Add `#pragma once` to `fire.h`**

```c
#pragma once
#include "FastLED.h"
...
```

- [ ] **Step 8: Add `#pragma once` to `noisePatterns.h`**

```c
#pragma once
#include <FastLED.h>

// The 16 bit version of our coordinates
...
```

- [ ] **Step 9: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "chore: add #pragma once include guards to all local headers"
```

---

## Task 10: Cleanup pass — minor / style issues

**Why:** Six small hygiene fixes that don't change behaviour but remove footguns and noise.

**Files:**
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino` (line 36 — BRIGHTNESS; line 40 — selectedEffect init)
- Modify: `AllEffects_FastLED/effectChanging.h` (lines 7-9 — make arrays `static const`)
- Modify: `AllEffects_FastLED/metaBalls.h` (line 11 — drop `gHue=0` side effect)
- Modify: `AllEffects_FastLED/pride.h` (lines 37-38 — drop redundant pixelnumber reorder)
- Modify: `AllEffects_FastLED/fire.h` (lines 14-15, 144 — rename globals to fire-prefixed)

- [ ] **Step 1: Fix `selectedEffect` initialiser and `BRIGHTNESS`**

In `AllEffects_FastLED/AllEffects_FastLED.ino`:

```diff
-#define BRIGHTNESS 254
+#define BRIGHTNESS 255
```

```diff
-byte selectedEffect = -1;
+byte selectedEffect = 0;
```

- [ ] **Step 2: Make `effectChanging.h` lookup arrays `static const`**

In `AllEffects_FastLED/effectChanging.h`, replace lines 7-9:

```diff
-  int listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13};
-  int listOfPatternsForSquareMatrix[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
-  int listOfPatternsForSimpleLedStrip[] = {0,1,6,7,8, 10, 12, 14, 15, 16, 17, 18,0, 13};
+  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13};
+  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3,  4,  5, 6, 7,  8, 9, 10, 11, 12, 13};
+  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13};
```

- [ ] **Step 3: Drop `gHue=0` side effect in `metaBalls`**

In `AllEffects_FastLED/metaBalls.h`, delete line 11:

```diff
 void metaBalls()
 {
-  gHue=0;
   uint8_t bx1 = beatsin8(15, 0, NUM_COLS - 1, 0, 0);
```

- [ ] **Step 4: Drop redundant `pixelnumber` reverse in `pride`**

In `AllEffects_FastLED/pride.h`, replace lines 37-40:

```diff
-    uint16_t pixelnumber = i;
-    pixelnumber = (NUM_LEDS - 1) - pixelnumber;
-
-    nblend( leds[pixelnumber], newcolor, 64);
+    nblend( leds[i], newcolor, 64);
```

- [ ] **Step 5: Rename `fire.h` global identifiers to fire-prefixed names**

In `AllEffects_FastLED/fire.h`:

Lines 14-15 — rename `rows`/`cols` to `fireRows`/`fireCols`:

```diff
-const uint16_t rows = NUM_ROWS;
-const uint16_t cols = NUM_COLS;
+const uint16_t fireRows = NUM_ROWS;
+const uint16_t fireCols = NUM_COLS;
```

Now update every reference to `rows` and `cols` inside `fire.h`. They appear on lines: 40 (`uint8_t pix[rows][cols]`), 110 (`i < rows && j < cols`), 129 (`random(0, cols)`), 153 (`i = rows - 1`), 155 (`j < cols`), 165 (`j < cols`), 198 (`i < rows`), 200 (`j < cols`). Replace `rows` → `fireRows` and `cols` → `fireCols` in every one of those lines.

Line 144 — rename `t` to `fireNextMs`:

```diff
-unsigned long t = 0; /* keep time */
+unsigned long fireNextMs = 0; /* keep time */
```

And update lines 148-150:

```diff
-  if (t > millis())
+  if (fireNextMs > millis())
     return;
-  t = millis() + (1000 / FPS);
+  fireNextMs = millis() + (1000 / FPS);
```

- [ ] **Step 6: Compile to verify**

```bash
arduino-cli compile --fqbn arduino:avr:uno --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

Expected: compiles cleanly.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "chore: misc hygiene — BRIGHTNESS, init, static arrays, fire globals"
```

---

## Task 11: Final hardware acceptance

**Why:** Compile-clean is necessary but not sufficient. Confirm each visible effect actually renders on the panel before declaring done.

**Files:** None.

- [ ] **Step 1: Final clean compile**

```bash
arduino-cli compile --fqbn arduino:avr:uno --warnings all --libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED 2>&1 | tee /c/Users/gethi/source/pixelatedlights/docs/superpowers/plans/final-compile.log
```

Diff `final-compile.log` against `baseline-compile.log` from Task 0. Confirm: zero errors, and warning count is the same or lower (no new warnings introduced).

- [ ] **Step 2: Flash to Arduino**

Connect the Arduino over USB. Identify the port:

```bash
arduino-cli board list
```

Upload (substitute the actual port for `COM3`):

```bash
arduino-cli upload -p COM3 --fqbn arduino:avr:uno /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
```

- [ ] **Step 3: Cycle through every effect and visually confirm**

For each of the 14 selector slots (rectangular-matrix lookup), turn the potentiometer / press the button until the panel is on that effect and confirm:

| Slot | Effect index | Function | What to see |
|------|--------------|----------|-------------|
| 0 | 0 | `pride()` | Smooth rainbow waves across full panel |
| 1 | 1 | `pacifica_loop()` | Blue/green ocean wave layers |
| 2 | 2 | `metaBalls()` | Five moving warm blobs |
| 3 | 3 | `make_fire()` | **Continuous** fire from bottom — confirm Task 4 fix (no dead floor) |
| 4 | 14 | `juggle()` | 8 coloured dots weaving |
| 5 | 15 | `sinelon()` | Single sweeping dot with trail |
| 6 | 6 | `rainbow()` | Rainbow gradient |
| 7 | 7 | `justWhite()` | All-white panel |
| 8 | 17 | `fadeIn` + palette blend | Confirm effect **renders** — confirm Task 6 fix |
| 9 | 9 | `changepattern()` → `updaterain()` | Falling rain dots — runs indefinitely without freeze (Task 5 fix) |
| 10 | 16 | `confetti()` | Random sparkles |
| 11 | 11 | `bpm()` | Pulsing palette stripes |
| 12 | 12 | `rainbowWithGlitter()` | Rainbow + sparkles |
| 13 | 13 | `jusBlack()` | Panel off |

Also confirm: **every LED on the panel lights** during effects driven by noise functions (effects 4/5/8/10 on the square-matrix lookup), since Task 3 fixed the noise renderer that previously only wrote LEDs 0..5.

- [ ] **Step 4: Tag the verified commit**

```bash
git tag -a fixes-verified -m "All 19 quality fixes verified on hardware"
```

- [ ] **Step 5: Final commit (logs only)**

```bash
git add docs/superpowers/plans/final-compile.log
git commit -m "chore: capture post-fix compile log"
```

---

## Coverage Self-Check

Mapping the 19 issues from the review back to tasks:

| # | Issue | Task |
|---|-------|------|
| 1 | Sketch folder/filename mismatch | Task 1 |
| 2 | `XYMatrix.h` serpentine `COL_OFFSET` bug | Task 2 |
| 3 | `noise.h:169` bound check broken | Task 3 |
| 4 | `noise.h:167` XY arg swap | Task 3 |
| 5 | `fire.h` bottom-row reheat dead | Task 4 |
| 6 | `rain.h` `speed` overflow | Task 5 |
| 7 | `fadeIn` no `FastLED.show()` | Task 6 |
| 8 | `noise.h` `uint16_t` no return | Task 7 |
| 9 | No header guards | Task 9 |
| 10 | `noise.h` clashes with FastLED's | Task 8 |
| 11 | `selectedEffect = -1` on unsigned | Task 10 |
| 12 | `metaBalls.h` clobbers `gHue` | Task 10 |
| 13 | `effectChanging.h` arrays stack-allocated | Task 10 |
| 14 | `pride.h` pointless `pixelnumber` reorder | Task 10 |
| 15 | `fire.h` generic global names | Task 10 |
| 16 | `configuation.h` typo | Task 1 |
| 17 | `BRIGHTNESS 254` off-by-one | Task 10 |
| 18 | `fire.h::isqrt` recursive | **Deliberately not addressed** — works correctly, depth ≤ 16, iterative rewrite would be churn |
| 19 | `rain.h` redundant `+ NUM_ROWS` | **Deliberately not addressed** — harmless after Task 5 makes `speed` unsigned-modulo; removing it would force re-verification for zero benefit |

All 17 actionable issues mapped to a task. Issues 18 and 19 are documented non-fixes.

---

## Execution Notes

- Frequent commits are baked in (one commit per task). If a task balloons mid-execution, split it.
- No tests in the traditional sense — the gate is `arduino-cli compile` + visual hardware verify at the end. Do not skip Task 11.
- If a task's compile step fails, fix the underlying cause; never amend a prior commit to paper over it (per `CLAUDE.md` guidance — create a new fix commit).
- If hardware verify (Task 11 Step 3) reveals an effect still broken, open a new investigation rather than tweaking inline; the review may have missed a bug.
