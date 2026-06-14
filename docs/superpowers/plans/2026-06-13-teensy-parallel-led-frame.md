# Teensy 4.0 / 4-Lane Parallel WS2815 Conversion — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the Tower Lamp from Arduino Uno (WS2812B 5V, 176 px, single pin) to Teensy 4.0 driving 3,212 px of WS2815 12V as 4 parallel data lanes (~41 fps), including firmware, CAD (peg sockets + PSU mounting), electrical wiring, and a finalized BOM.

**Architecture:** Approach A — one contiguous `leds[3212]` partitioned lane-major; only `XY()` and `setup()` change, the 18 effects are untouched. FastLED upgraded to latest for ObjectFLED DMA parallel output. Single 12V PSU + bus-bar injection; 74HCT245 level shift; standalone 12V→5V buck. Existing Fusion model houses everything.

**Tech Stack:** Teensy 4.0 (IMXRT1062), Arduino/Teensyduino, FastLED (latest), WS2815 12V strip, 74HCT245, Fusion 360 (via MCP), g++ (host unit tests).

**Spec:** `docs/superpowers/specs/2026-06-13-teensy-parallel-led-frame-design.md`

---

## File Structure

**Firmware** (`AllEffects_FastLED/`):
- `configuration.h` — already updated with Teensy 4-lane macros (`NUM_ROWS 73`, `NUM_LANES 4`, `COLS_PER_LANE 11`, `LEDS_PER_LANE 803`, `NUM_LEDS 3212`). Verified in Task 2.
- `XYMatrix.h` — `XY()` rewritten for lane-major partition; FastLED include dropped so it is host-testable.
- `AllEffects_FastLED.ino` — `setup()` parallel `addLeds`, power cap, button pin.
- `effectChanging.h` — hysteresis added to `convertToSelectedEffect()`.
- `libraries/FastLED-3.3.3/` — replaced by the latest FastLED release.
- `test/xy_test.cpp`, `test/hysteresis_test.cpp` — host unit tests (new).

**CAD** (Fusion model "Tower Lamp", edited via MCP scripts): VFrame outer block (delete legacy pegs, cut 4 sockets), base top (PSU slot already enlarged; add mounting/clearance), cable routing.

**Docs:** `docs/superpowers/BOM-tower-lamp.md` (new).

---

## Phase A — Firmware

### Task 1: Upgrade vendored FastLED to latest

**Files:**
- Delete: `AllEffects_FastLED/libraries/FastLED-3.3.3/`
- Create: `AllEffects_FastLED/libraries/FastLED/` (latest release)

- [ ] **Step 1: Record current version for rollback**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights
grep FASTLED_VERSION AllEffects_FastLED/libraries/FastLED-3.3.3/FastLED.h
```
Expected: `#define FASTLED_VERSION 3003002`

- [ ] **Step 2: Download the latest FastLED release**

Run:
```bash
cd /tmp
curl -L -o fastled.zip https://github.com/FastLED/FastLED/archive/refs/tags/3.9.13.tar.gz
```
(If 3.9.13 is not the newest tag, list tags with `curl -s https://api.github.com/repos/FastLED/FastLED/tags | grep '"name"' | head` and pick the highest stable `3.x.y`.)

- [ ] **Step 3: Replace the vendored library**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries
rm -rf FastLED-3.3.3
mkdir FastLED && tar -xzf /tmp/fastled.zip -C FastLED --strip-components=1
grep FASTLED_VERSION FastLED/src/FastLED.h 2>/dev/null || grep FASTLED_VERSION FastLED/FastLED.h
```
Expected: a version ≥ `3009000`.

- [ ] **Step 4: Confirm Teensy 4.0 + WS2815 support exist**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED/libraries/FastLED
grep -ril "mxrt1062" . | head -1
grep -ril "WS2815" . | head -1
```
Expected: at least one file each (Teensy 4.0 platform present; WS2815 chipset defined).

- [ ] **Step 5: Commit**

```bash
cd /c/Users/gethi/source/pixelatedlights
git add -A AllEffects_FastLED/libraries
git commit -m "chore(firmware): upgrade vendored FastLED to latest for Teensy 4 + WS2815"
```

---

### Task 2: Verify configuration.h macros

**Files:**
- Verify: `AllEffects_FastLED/configuration.h`

- [ ] **Step 1: Confirm the macro values and arithmetic**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights
grep -E "NUM_COLS|NUM_ROWS|NUM_LANES|COLS_PER_LANE|LEDS_PER_LANE|NUM_LEDS" AllEffects_FastLED/configuration.h
```
Expected (already committed):
```
#define NUM_COLS 44
#define NUM_ROWS 73
#define NUM_LANES 4
#define COLS_PER_LANE (NUM_COLS / NUM_LANES)      // 11
#define LEDS_PER_LANE (COLS_PER_LANE * NUM_ROWS)  // 803
#define NUM_LEDS (NUM_LANES * LEDS_PER_LANE)       // 3212
```
No change needed if this matches. (Arithmetic check: 44/4=11, 11×73=803, 4×803=3212.)

---

### Task 3: Make XY() host-testable (drop FastLED dependency)

**Files:**
- Modify: `AllEffects_FastLED/XYMatrix.h`

- [ ] **Step 1: Remove the unnecessary FastLED include**

`XY()` returns `uint16_t` and uses only integer types — it does not need FastLED. Edit `XYMatrix.h` so the top reads:
```cpp
#pragma once
#include <stdint.h>
#include "configuration.h"
```
(Remove the `#include <FastLED.h>` line. The `.ino` already includes FastLED before this header, so the sketch build is unaffected.)

- [ ] **Step 2: Commit**

```bash
git add AllEffects_FastLED/XYMatrix.h
git commit -m "refactor(firmware): make XYMatrix.h host-testable (drop FastLED include)"
```

---

### Task 4: Rewrite XY() for the lane-major partition (TDD)

**Files:**
- Test: `AllEffects_FastLED/test/xy_test.cpp` (create)
- Modify: `AllEffects_FastLED/XYMatrix.h`

- [ ] **Step 1: Write the failing test**

Create `AllEffects_FastLED/test/xy_test.cpp`:
```cpp
#include <stdint.h>
#include <cassert>
#include <cstdio>
#include "../XYMatrix.h"

int main() {
    // lane 0, first strip (col 0, even → ascending)
    assert(XY(0, 0)  == 0);
    assert(XY(0, 72) == 72);
    // lane 0, second strip (col 1, odd → descending serpentine)
    assert(XY(1, 0)  == 73 + 72);   // 145
    assert(XY(1, 72) == 73 + 0);    // 73
    // lane boundary: col 11 starts lane 1
    assert(XY(11, 0) == 803);       // 1*LEDS_PER_LANE
    // last pixel: x=43 (lane 3, col 10 even), y=72
    assert(XY(43, 72) == 3*803 + 10*73 + 72); // 3211
    assert(XY(43, 0)  == 3*803 + 10*73 + 0);  // 3139
    // full range is within bounds and the corners are unique
    assert(XY(43, 72) == NUM_LEDS - 1);
    printf("xy_test: all assertions passed\n");
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
g++ -std=c++17 -o /tmp/xy_test test/xy_test.cpp && /tmp/xy_test
```
Expected: FAIL — the current `XY()` uses the old `x * NUM_ROWS + y` column-major formula, so `XY(11,0)` returns `11*73=803`? (coincidence) but `XY(1,0)` returns `1*73 + (73-1-0)=145`? The old serpentine differs at lane boundaries: `XY(11,0)` old = `11*73 + (73-1-0)=875`, not 803. Assertion `XY(11,0)==803` fails. Confirm a failing assertion / non-zero exit.

- [ ] **Step 3: Rewrite XY()**

In `AllEffects_FastLED/XYMatrix.h`, replace the body of `XY()` with:
```cpp
uint16_t XY(uint8_t x, uint8_t y)
{
  if (ISMATRIX)
  {
    uint8_t lane = x / COLS_PER_LANE;                  // which side (0..3)
    uint8_t col  = x % COLS_PER_LANE;                  // strip within side (0..10)
    uint8_t yy   = (col & 1) ? (NUM_ROWS - 1 - y) : y; // serpentine within lane
    return (uint16_t)lane * LEDS_PER_LANE + (uint16_t)col * NUM_ROWS + yy;
  }
  return x * 10 + y;
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
g++ -std=c++17 -o /tmp/xy_test test/xy_test.cpp && /tmp/xy_test
```
Expected: `xy_test: all assertions passed` and exit 0.

- [ ] **Step 5: Add a bijection test (no two coordinates map to the same index)**

Append to `main()` in `xy_test.cpp`, before the success print:
```cpp
    {
        static bool seen[NUM_LEDS] = {false};
        int count = 0;
        for (uint8_t x = 0; x < NUM_COLS; ++x)
            for (uint8_t y = 0; y < NUM_ROWS; ++y) {
                uint16_t i = XY(x, y);
                assert(i < NUM_LEDS);
                assert(!seen[i]);   // no collisions
                seen[i] = true;
                ++count;
            }
        assert(count == NUM_LEDS); // every index hit exactly once
    }
```

- [ ] **Step 6: Run again to verify the bijection holds**

Run:
```bash
g++ -std=c++17 -o /tmp/xy_test test/xy_test.cpp && /tmp/xy_test
```
Expected: `xy_test: all assertions passed`, exit 0.

- [ ] **Step 7: Commit**

```bash
git add AllEffects_FastLED/XYMatrix.h AllEffects_FastLED/test/xy_test.cpp
git commit -m "feat(firmware): lane-major XY() mapping for 4-lane parallel output"
```

---

### Task 5: Add pot hysteresis to convertToSelectedEffect() (TDD)

**Files:**
- Test: `AllEffects_FastLED/test/hysteresis_test.cpp` (create)
- Modify: `AllEffects_FastLED/effectChanging.h`

- [ ] **Step 1: Make the decode host-testable**

`effectChanging.h` includes `<FastLED.h>` but the function uses none of it. Edit the top of `effectChanging.h`:
```cpp
#pragma once
#include <stdint.h>
#include "configuration.h"
```
(Remove `#include <FastLED.h>`; replace `byte` with `uint8_t` throughout this file so it compiles on the host. Arduino aliases `byte`→`uint8_t`, so this is behaviour-neutral on the Teensy.)

- [ ] **Step 2: Write the failing test**

Create `AllEffects_FastLED/test/hysteresis_test.cpp`:
```cpp
#include <stdint.h>
#include <cassert>
#include <cstdio>
#include "../effectChanging.h"

int main() {
    // Boundary between band 0 and band 1 is at 57. Dead-band = 28.
    // Settle just below the boundary (56 → band 0).
    uint8_t settled = convertToSelectedEffect(56);
    // Jitter +2 to 58 — naively band 1, but within the dead-band → must stay band 0 (no flicker).
    uint8_t jitter = convertToSelectedEffect(58);
    assert(jitter == settled);
    // A deliberate move well beyond the dead-band DOES change band.
    uint8_t moved = convertToSelectedEffect(90); // |90-56|=34 > 28 → relatch → band 1
    assert(moved != settled);
    printf("hysteresis_test: all assertions passed\n");
    return 0;
}
```

- [ ] **Step 3: Run the test to verify it fails**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
g++ -std=c++17 -o /tmp/hyst_test test/hysteresis_test.cpp && /tmp/hyst_test
```
Expected: FAIL — current code recomputes `resistence/57` every call with no dead-band, so `jitter` (58/57=1) differs from `settled` (56/57=0); assertion `jitter == settled` fails.

- [ ] **Step 4: Implement hysteresis**

Replace the body of `convertToSelectedEffect()` in `effectChanging.h` with:
```cpp
uint8_t convertToSelectedEffect(int resistence)
{
  static const uint8_t listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13, 19, 20, 21, 22};
  static const uint8_t listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3,  4,  5, 6, 7,  8, 9, 10, 11, 12, 13,  0,  0,  0,  0};
  static const uint8_t listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13,  0,  0,  0,  0};

  static int lastBandReading = -1000; // raw reading that last set the band
  const int DEADBAND = 28;            // ~half of a 57-count step

  // Only re-latch the band when the pot moves beyond the dead-band.
  if (lastBandReading < -500 || (resistence - lastBandReading) > DEADBAND
                              || (lastBandReading - resistence) > DEADBAND) {
    lastBandReading = resistence;
  }

  uint8_t result = lastBandReading / 57;
  if (result > 17) result = 17;

  if (!ISMATRIX)
    return listOfPatternsForSimpleLedStrip[result];
  if (NUM_COLS == NUM_ROWS)
    return listOfPatternsForSquareMatrix[result];
  return listOfPatternsForRectangularMatrix[result];
}
```
(Remove the now-unused `lastSetResistence` global and the dead `difference` line.)

- [ ] **Step 5: Run the test to verify it passes**

Run:
```bash
g++ -std=c++17 -o /tmp/hyst_test test/hysteresis_test.cpp && /tmp/hyst_test
```
Expected: `hysteresis_test: all assertions passed`, exit 0.

- [ ] **Step 6: Commit**

```bash
git add AllEffects_FastLED/effectChanging.h AllEffects_FastLED/test/hysteresis_test.cpp
git commit -m "feat(firmware): add pot hysteresis to kill pattern boundary flicker"
```

---

### Task 6: Update setup() — parallel output, power cap, button pin

**Files:**
- Modify: `AllEffects_FastLED/AllEffects_FastLED.ino`

- [ ] **Step 1: Change the button pin define**

In `AllEffects_FastLED.ino`, change:
```cpp
#define BUTTON 2
```
to:
```cpp
#define BUTTON A0   // Teensy 4.0 analog-capable pin; pot powered from 3.3V
```

- [ ] **Step 2: Replace the LED registration and add the power cap**

Replace the `setup()` body's `FastLED.addLeds<...>` line:
```cpp
  FastLED.addLeds<WS2811, PIN, GRB>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
```
with:
```cpp
  FastLED.addLeds<NUM_LANES, WS2815, 1, GRB>(leds, LEDS_PER_LANE)
      .setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(12, 75000); // ~75A/~900W under the PSU; tune later
```
(`FIRST_PIN = 1` maps the 4 lanes to Teensy pins 1, 0, 24, 25 in lane order. Remove the now-unused `pinMode(2, INPUT_PULLUP);` line — the pot is an analog input, not a pulled-up button.)

- [ ] **Step 3: Verify the changes compile-read correctly**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights
grep -nE "BUTTON|addLeds|setMaxPower|pinMode" AllEffects_FastLED/AllEffects_FastLED.ino
```
Expected: `BUTTON A0`, the parallel `addLeds<NUM_LANES, WS2815, 1, GRB>`, the power cap line, and no `pinMode(2, ...)`.

- [ ] **Step 4: Commit**

```bash
git add AllEffects_FastLED/AllEffects_FastLED.ino
git commit -m "feat(firmware): 4-lane parallel WS2815 output, 12V power cap, pot on A0"
```

---

### Task 7: Compile the sketch for Teensy 4.0

**Files:**
- Build only (no source change)

- [ ] **Step 1: Install arduino-cli + Teensy core if not present**

Run:
```bash
arduino-cli version || echo "install arduino-cli first"
arduino-cli config add board_manager.additional_urls https://www.pjrc.com/teensy/package_teensy_index.json
arduino-cli core update-index
arduino-cli core install teensy:avr
```
Expected: Teensy core installed (`teensy:avr`).

- [ ] **Step 2: Compile against the local libraries folder**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights
arduino-cli compile --fqbn teensy:avr:teensy40 \
  --libraries AllEffects_FastLED/libraries \
  AllEffects_FastLED
```
Expected: "Sketch uses … bytes" with no errors. RAM usage for `leds[]` (3212×3 = 9636 B) is well within the Teensy's ~1 MB.

- [ ] **Step 3: If compile fails on a deprecated FastLED API**

Read the error, fix the specific call in the named effect file to the current FastLED signature (effects use stable calls — `XY`, `CHSV`, `fill_*`, `noise`, `nblend`; failures are unlikely). Re-run Step 2 until clean. Commit any fixes:
```bash
git add -A AllEffects_FastLED
git commit -m "fix(firmware): adapt effect calls to current FastLED API"
```

- [ ] **Step 4: Record the build result**

The clean compile is the firmware verification gate. Note the reported flash/RAM figures in the commit message of the next step or the PR description.

---

## Phase B — CAD (Fusion model, via MCP scripts)

> Run these through the Fusion MCP `fusion_mcp_execute` `script` tool. Each script prints a result used as the verification. The model is "Tower Lamp"; confirm it is the active document first.

### Task 8: Delete VFrame's 4 legacy bottom pegs

**Files:**
- Fusion model: VFrame outer block

- [ ] **Step 1: Identify the legacy peg bodies/features**

The legacy pegs are the 4 downward tabs on the VFrame underside (≈9.8 × 2.8 × 4.9 mm, hanging below y=0 local). Run a script that lists, for the `Vertical LED Frame:1` occurrence's `VFrame outer block`, any peg geometry below the main block underside, and prints their bounding boxes. Confirm 4 pegs at the corners.

- [ ] **Step 2: Delete the peg material**

Run a Fusion script that removes the 4 peg tabs (delete the originating extrude features if present, else cut them with tool boxes spanning each peg's bbox). Print the body's new min-Y to confirm the underside is now flat (no tabs protruding below the block underside).
Expected: body underside flat at the seated plane; no sub-underside protrusions.

- [ ] **Step 3: Verify and save**

Run a script printing the VFrame outer block bounding box. Expected: the negative-Y peg protrusions are gone. Save the document via the MCP `document` `save` op (or note manual save).

---

### Task 9: Cut 4 sockets for the base-top pegs

**Files:**
- Fusion model: VFrame outer block

- [ ] **Step 1: Locate the base-top peg centres (do not hardcode — derive from the model)**

Run this script (adjust occurrence names only if the model renamed them):
```python
import adsk.core, adsk.fusion
def run(_ctx):
    app = adsk.core.Application.get()
    d = adsk.fusion.Design.cast(app.activeProduct)
    root = d.rootComponent
    bt = None
    for b in root.occurrences.itemByName('Base Assembly:1').bRepBodies:
        if b.name == 'base top': bt = b
    # peg tops are the small Y-up planar faces at the plate top (4 of them)
    pegs = []
    for f in bt.faces:
        g = f.geometry
        if isinstance(g, adsk.core.Plane) and g.normal.z == 0: continue
        if isinstance(g, adsk.core.Plane) and abs(g.normal.y - 1.0) < 1e-3:
            bb = f.boundingBox; mn, mx = bb.minPoint, bb.maxPoint
            if (mx.x-mn.x) < 1.5 and (mx.z-mn.z) < 1.5 and mn.y > 1.0:  # small, above plate
                pegs.append(((mn.x+mx.x)/2, (mn.y+mx.y)/2, (mn.z+mx.z)/2))
    print('peg top centres (cm):', [tuple(round(v,3) for v in p) for p in pegs])
```
Expected: 4 centres near (9.75, 1.3, -16.125), (9.75, 1.3, -0.875), (-3.75, 1.3, -16.125), (-3.75, 1.3, -0.875) cm.

- [ ] **Step 2: Cut the 4 sockets into the VFrame underside**

Run a script that, for each peg centre (x, z), builds a tool box of **10.2 mm (X) × 4.7 mm (Z) × 8.2 mm (Y)** centred on (x, z) and starting at the VFrame seated underside, extending upward 8.2 mm, then boolean-subtracts all four from `VFrame outer block`. Use `TemporaryBRepManager.createBox` + `Combine`/`booleanOperation(CutFeature)`. Print success.

- [ ] **Step 3: Verify the pegs fully enter and the bay floor is not breached**

Run the boolean-check script (peg box ∩ VFrame and peg box − VFrame) used during design:
```python
# For each peg box (10.0 x 4.5 x 8.0 mm = real peg size) seated into VFrame:
#   exposed = peg − VFrame_solid  → expect ~0 mm³ (peg fully housed in socket walls)
# Also confirm wall material remains outboard of each socket (≈0.9 mm under bays).
```
Expected: exposed peg volume ≈ 0 mm³ for all 4; remaining outer wall ≥ ~0.8 mm under the bays. Save the document.

- [ ] **Step 4: Visual confirm**

Capture an MCP `screenshot` (direction `iso-bottom-left`) of the VFrame underside seated on base top. Confirm 4 sockets aligned over the 4 pegs.

---

### Task 10: PSU vertical clearance + mounting features

**Files:**
- Fusion model: base / lower cavity

- [ ] **Step 1: Confirm vertical clearance for the chosen PSU**

Run a script that, given the chosen PSU height (e.g. MSP-1000-12 ≈ 199 mm long when standing), checks the unobstructed vertical run inside the lower cavity from the base floor up — i.e. no base-top peg, socket, or first LED frame intrudes into the PSU's standing footprint (footprint placed under the 75 × 140 mm slot). Print the available clear height and the first obstruction Y.
Expected: clear height ≥ PSU standing height; report any obstruction.

- [ ] **Step 2: Add PSU mounting features**

Add bosses/brackets (or screw posts matching the PSU's mounting holes) in the base to retain the PSU vertically under the slot. Model per the chosen PSU's hole pattern (from its datasheet). Print/screenshot confirmation.

- [ ] **Step 3: Save + screenshot**

Capture an MCP `screenshot` showing the PSU mount position relative to the 75 × 140 mm slot. Save the document.

---

### Task 11: Cable routing channels

**Files:**
- Fusion model: base + VFrame

- [ ] **Step 1: Add data-line routing**

Add a channel/grommet hole from the electronics area in the base up to the bottom of each side's first strip — 4 data runs. Diameter to suit a 3-core jumper (~4–5 mm) per side. Print/screenshot.

- [ ] **Step 2: Add power-bus routing**

Add channel(s)/holes for the 12 V bus taps from the base bus bar to each strip's bottom (44 taps, grouped per side). Print/screenshot.

- [ ] **Step 3: Save**

Save the document. Capture a final assembled `screenshot` (direction `iso-top-right`).

---

## Phase C — Electrical, Assembly, Validation

### Task 12: Finalize the BOM

**Files:**
- Create: `docs/superpowers/BOM-tower-lamp.md`

- [ ] **Step 1: Write the BOM with the selected PSU**

Create `docs/superpowers/BOM-tower-lamp.md`:
```markdown
# Tower Lamp — Bill of Materials

| # | Item | Qty | Spec / Notes | Source |
|---|------|-----|--------------|--------|
| 1 | Teensy 4.0 | 1 | IMXRT1062 dev board, 3.3V logic | PJRC / UK reseller |
| 2 | WS2815 12V strip, 60/m, IP30, black PCB | 11 × 5 m reels | 53.6 m used (44 strips × 73 LED); offcut spare | — |
| 3 | 12V PSU | 1 | **<chosen: MSP-1000-12 / S-1000-12 / SE-1000-12>**, ≥80A at 12V | Amazon.co.uk / eBay.co.uk |
| 4 | 12V→5V buck converter | 1 | standalone 5V rail (Teensy + 74HCT245), ≥2A | — |
| 5 | 74HCT245 octal level shifter | 1 | 3.3V→5V, 4 data lines (+ protoboard) | — |
| 6 | 470 Ω resistor | 4 | series on each data line | — |
| 7 | Bus bar / 10–12 AWG wire | — | 12V distribution, ~80A trunk | — |
| 8 | 18–20 AWG wire | — | per-strip 12V/GND taps | — |
| 9 | Main DC fuse + holder | 1 | ~100A, PSU output | — |
| 10 | Branch fuses + holders | — | bus branches | — |
| 11 | 1000 µF / 25V capacitor | 1 | across PSU output at first injection | — |
| 12 | Potentiometer + knob | reuse | pattern selector, wired to A0 / 3.3V | existing |
| 13 | IEC inlet + fuse + switch | 1 | AC side | — |
| 14 | Mains cable, earth bonding | — | AC side | — |
| 15 | Connectors / heat-shrink / mounting hardware | — | assembly | — |

PSU selection clears the 75 × 140 mm base-top slot for all three candidates;
pick per budget/availability and re-verify the slot vs datasheet.
```
Fill the PSU row with the actually-chosen model.

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/BOM-tower-lamp.md
git commit -m "docs: add Tower Lamp bill of materials"
```

---

### Task 13: Bench bring-up — one lane, level shifter, buck

**Files:**
- Hardware bench (no repo change); record results in a commit note

- [ ] **Step 1: Wire the 5V rail**

PSU 12V → buck input. Buck 5V → Teensy VIN **and** (separate wire) → 74HCT245 Vcc. Tie all grounds together (PSU, buck, Teensy, '245, strip). Pot outer legs to 3.3V and GND, wiper to A0.

- [ ] **Step 2: Wire one data lane through the shifter**

Teensy pin 1 → 74HCT245 A-input (with 470 Ω series) → '245 B-output (5V) → DIN of one test strip (cut to ~73 LEDs). Strip 12V/GND from the PSU (fused).

- [ ] **Step 3: Flash and confirm output**

Flash the sketch (`arduino-cli upload --fqbn teensy:avr:teensy40 -p <port> AllEffects_FastLED`). Power on. Confirm the strip lights and animates.
Expected: pixels light; colours correct (see Step 4).

- [ ] **Step 4: Confirm colour order (GRB)**

Select a known pattern and confirm red shows red. If colours are swapped, change the chipset order in `setup()` (`GRB`→`RGB`/`BRG` as needed), recompile, re-flash. Commit any order change:
```bash
git add AllEffects_FastLED/AllEffects_FastLED.ino
git commit -m "fix(firmware): correct WS2815 colour order to <ORDER>"
```

- [ ] **Step 5: Confirm framerate + hysteresis**

Temporarily add `Serial.println(FastLED.getFPS());` in `loop()` (or check via a known-timing pattern). Expected: solid frame rate; with one short lane it will be high. Turn the pot slowly across a pattern boundary — confirm no flicker (hysteresis working). Remove the temporary Serial line before final.

---

### Task 14: Full assembly + power injection

**Files:**
- Hardware (no repo change)

- [ ] **Step 1: Mount strips and chain data per side**

Seat all 44 strips in the VFrame slots. Within each side, daisy-chain DATA across the 11 strips serpentine (DOUT→DIN), matching the `XY()` parity (col 0 up, col 1 down, …). Each side's first DIN ← its lane pin via the '245.

- [ ] **Step 2: Confirm serpentine parity matches XY()**

Run a single-pixel walk test (light index 0, then 1, 2, …) and confirm the lit pixel moves bottom→top on col 0, top→bottom on col 1, across to the next side at the lane boundary. If a column runs the wrong way, either flip that strip's physical orientation or invert the parity in `XY()` (`(col & 1)` → `!(col & 1)`); recompile. Commit any `XY()` change:
```bash
git add AllEffects_FastLED/XYMatrix.h AllEffects_FastLED/test/xy_test.cpp
git commit -m "fix(firmware): correct XY serpentine parity to match wiring"
```

- [ ] **Step 3: Wire power injection**

Bus bar around the base from the PSU (main fuse). Tap 12V/GND to the bottom of every strip (branch-fused per side). 1000 µF cap across the PSU output at the first injection. Do NOT power strips through the data chain.

- [ ] **Step 4: Power-on staged**

Power one side at a time at low brightness; verify no excessive voltage drop (measure 12V at the top of a strip — expect within ~0.5 V of the base). Then all four.

---

### Task 15: Final integration validation

**Files:**
- Hardware + repo (final tuning commit)

- [ ] **Step 1: Measure full-array draw at the brightness cap**

Run a bright pattern (e.g. white/fill) at the capped brightness. Measure PSU current. Expected: within the PSU rating; if near the limit, lower the `setMaxPowerInVoltsAndMilliamps` mA value, recompile.

- [ ] **Step 2: Confirm framerate across effects**

Cycle the pot through all patterns. Confirm smooth motion (target ≥30 fps; design ~41 fps). Note any effect that drops frames.

- [ ] **Step 3: Final commit + tag the tuned power cap**

```bash
git add AllEffects_FastLED/AllEffects_FastLED.ino
git commit -m "chore(firmware): final power cap + validated full-tower integration"
```

- [ ] **Step 4: Run host unit tests one last time (regression)**

Run:
```bash
cd /c/Users/gethi/source/pixelatedlights/AllEffects_FastLED
g++ -std=c++17 -o /tmp/xy_test test/xy_test.cpp && /tmp/xy_test
g++ -std=c++17 -o /tmp/hyst_test test/hysteresis_test.cpp && /tmp/hyst_test
```
Expected: both print "all assertions passed".

---

## Self-Review Notes (coverage vs spec)
- §2 firmware (config, XY, setup, hysteresis): Tasks 2, 4, 6, 5.
- §3 FastLED upgrade: Task 1.
- §4 electrical (level shift, buck, PSU, injection, protection, power cap): Tasks 6, 13, 14, 12.
- §5 CAD (peg sockets, legacy peg delete, PSU mount/clearance, routing): Tasks 8, 9, 10, 11.
- §6 BOM: Task 12.
- §7 testing (XY unit, bench, power, incremental, colour order): Tasks 4, 13, 14, 15.
- Open items: colour order (Task 13 Step 4), serpentine parity (Task 14 Step 2), PSU selection (Task 12 Step 1).
