# Tower Lamp — Teensy 4.0 / 4-Lane Parallel WS2815 Conversion

**Date:** 2026-06-13
**Status:** Approved design (pre-implementation)
**Scope:** Firmware + electrical + CAD + Bill of Materials

## 1. Overview & Goal

Convert the Tower Lamp from a single-pin Arduino Uno (WS2812B 5 V, 176 px) to an
**Arduino-compatible Teensy 4.0** driving the full **4 ft** tower: **44 vertical
strips × 73 LEDs = 3,212 px** of **WS2815 12 V** addressable strip, output as
**4 parallel data lanes** (one per tower side) for broadcast-smooth framerate.

Target framerate: WS2815/WS2811 data is 800 kHz = 30 µs/LED, independent of MCU
speed. One lane = 11 strips × 73 = 803 LEDs → 803 × 30 µs ≈ 24 ms/frame ≈
**~41 fps**, comfortably above 24 fps film / 25 fps PAL / 30 fps NTSC.

### Decisions locked during brainstorming
- **Board:** Teensy 4.0 (IMXRT1062, ~1 MB RAM, 600 MHz, 3.3 V logic, not 5 V tolerant).
- **Strip:** WS2815 12 V, 60 LEDs/m, 1 LED = 1 pixel, 10 mm wide.
- **FastLED:** upgrade vendored 3.3.2 → latest (3.9/3.10).
- **Power:** single large 12 V PSU at the base, bus-bar distribution.
- **Firmware refactor:** Approach A — single contiguous `leds[]`, change only `XY()` + `setup()`; effects untouched.
- **Control:** keep the existing potentiometer + 18-pattern decode; rewire to A0 / 3.3 V; add hysteresis.

## 2. Firmware Architecture (Approach A)

**Data flow:** pot (A0) → `convertToSelectedEffect()` (with hysteresis) →
`selectedEffect` → effect writes `leds[]` through `XY(x, y)` → `FastLED.show()`
drives 4 lanes in parallel.

FastLED owns the parallel transmission (4 pins simultaneously, timing, DMA) and
assumes `leds[]` is partitioned **lane-major**. Our code owns the
coordinate→lane mapping (`XY()`) and the matching physical wiring. The 18 effects
are lane-unaware — they call `leds[XY(x,y)]` and are not modified.

### Changes, isolated to 3 files

**`configuration.h`** (already updated):
```c
#define NUM_COLS 44
#define NUM_ROWS 73
#define NUM_LANES 4
#define COLS_PER_LANE (NUM_COLS / NUM_LANES)      // 11
#define LEDS_PER_LANE (COLS_PER_LANE * NUM_ROWS)  // 803
#define NUM_LEDS (NUM_LANES * LEDS_PER_LANE)       // 3212
```

**`XYMatrix.h`** — rewrite `XY()` for the lane-major partition:
```c
uint16_t XY(uint8_t x, uint8_t y) {
  uint8_t lane = x / COLS_PER_LANE;        // which side (0..3)
  uint8_t col  = x % COLS_PER_LANE;        // strip within side (0..10)
  uint8_t yy   = (col & 1) ? (NUM_ROWS - 1 - y) : y;  // serpentine within lane
  return lane * LEDS_PER_LANE + col * NUM_ROWS + yy;
}
```
Serpentine parity assumes each side's strips alternate up/down and each lane
starts at the bottom of its first strip. Confirm against the physical wiring;
flip parity/offset if the build differs.

**`AllEffects_FastLED.ino` `setup()`:**
```c
FastLED.addLeds<NUM_LANES, WS2815, 1, GRB>(leds, LEDS_PER_LANE)
    .setCorrection(TypicalLEDStrip);
FastLED.setMaxPowerInVoltsAndMilliamps(12, 75000);  // ~75A / ~900W, under the 83A/1000W PSU; tune to taste
FastLED.setBrightness(BRIGHTNESS);
```
- Parallel `FIRST_PIN = 1` → lanes map to Teensy pins **1, 0, 24, 25** (fixed by
  the block driver's pin table; lane 0→pin 1, lane 1→pin 0, …).
- `#define BUTTON A0` (was 2). Keep ADC at default 10-bit.
- Color order GRB — verify on the first strip with RGBCalibrate.

**`effectChanging.h`** — add hysteresis. Use the currently-dead `lastSetResistence`:
re-evaluate the pattern index only when the pot reading moves more than a
dead-band (≈ half a 57-count step) from the value that last set the pattern.
Keep `÷57` and the 18-entry `listOfPatternsForRectangularMatrix` (selected
because `NUM_COLS 44 ≠ NUM_ROWS`).

## 3. FastLED Upgrade

Re-vendor FastLED **3.3.2 → latest (3.9/3.10)**.

- **Gains:** ObjectFLED DMA parallel output on Teensy 4.0 → non-blocking `show()`
  (CPU free during the ~24 ms transmit, so compute-heavy effects stay smooth);
  WS2815 chipset/timing; arbitrary-pin output and an easy 8-lane path; six years
  of bug fixes; improved power management.
- **Required, not just nice:** WS2815 support and reliable Teensy-4 parallel are
  cleanest on the new library.
- **Cost / risk:** re-vendor the library folder; regression-test the 18 effects.
  API risk is low — effects use stable calls (`XY`, `CHSV`, `fill_*`, `noise`,
  `nblend`).

## 4. Electrical

- **Logic level shift:** Teensy 3.3 V → 5 V on all 4 data lines via one
  **74HCT245**; 470 Ω series resistor on each data line.
- **12 V→5 V buck — standalone unit.** Takes 12 V from the PSU and provides a
  dedicated 5 V rail with **two separate output wires: one to the Teensy's 5 V
  input, one to the 74HCT245 Vcc.** It is its own block, not lumped with either.
- **Power:** single **12 V ~1000 W PSU** (e.g. Mean Well LRS-1000-12, 83 A) →
  **bus bar** around the base → **per-strip 12 V / GND taps**. Power is injected
  per strip from the bus, never through the data chain. WS2815 at 12 V handles
  the 1.2 m strip on a single bottom feed; add a top feed only if the top dims.
- **Protection:** main DC fuse (~100 A) at the PSU; per-branch fuses on the bus;
  1000 µF / 25 V cap across the PSU output at the first injection point; AC side
  IEC inlet + fuse + switch + earth bond.
- **Common ground:** PSU 12 V GND, buck GND, Teensy GND, 74HCT245 GND, and all
  strip grounds tie together.
- **Brightness cap** via `setMaxPowerInVoltsAndMilliamps(12, …)` keeps draw under
  the PSU rating. Full white is ~800–960 W (≈0.25–0.30 W/LED × 3212), near the
  1000 W rating, so the cap is protection, not just aesthetics.

### Lane → slice → pin → side mapping
| Lane | `leds[]` slice | Teensy pin | Physical side | Grid cols |
|------|----------------|-----------|---------------|-----------|
| 0 | 0 – 802 | 1 | Side 0 | 0–10 |
| 1 | 803 – 1605 | 0 | Side 1 | 11–21 |
| 2 | 1606 – 2408 | 24 | Side 2 | 22–32 |
| 3 | 2409 – 3211 | 25 | Side 3 | 33–43 |

Within each side, 11 strips daisy-chain for DATA (serpentine, handled by `XY()`).

## 5. CAD / Housing — Existing Tower Lamp (Fusion 360)

No new enclosure. Everything lives in the **existing model**.

- **Base Assembly** houses: the PSU, Teensy, the 12 V→5 V buck, the 74HCT245,
  fuse/bus distribution, and the existing **potentiometer fitting**.
- **VFrame outer block** holds the 44 strips in its 11 mm slots. WS2815 is 10 mm
  wide → 0.5 mm/side clearance, slot ~2 mm deep — no slot change needed.
- **Peg/socket (the original task that started this work):** delete VFrame's 4
  legacy bottom pegs; cut 4 sockets (peg + 0.1 mm = 10.2 × 4.7 × 8.2 mm) at the
  base-top peg positions into the already-thickened walls. Walls were extended
  +1.5 mm earlier, so the socket retains ~0.9 mm cover under the bay recesses
  (no breach). Verify by boolean intersection after the cut.
- **Routing:** add cable channels / grommet through the existing base and VFrame
  for the 4 data lines and the 12 V bus taps.

### PSU mounting — vertical (requires base-top hole enlargement)
The ~1000 W PSU is mounted **vertically** (standing on its narrow end). The
interior cavity (~144 × 144 mm) easily accepts the footprint and the 4 ft tower
easily accepts the height, so no base enlargement is needed for volume.

**However, the PSU will not pass through the base-top opening as-is.** Measured
base-top through-openings: a main rectangular slot ~43 mm (X) × 100 mm (Z), and a
secondary rounded opening ~45 × 40.5 mm. A vertical 1000 W 12 V PSU end face is
~115 × 40 mm — the ~115 mm width exceeds the 100 mm slot by ~15 mm. The slot
thickness (~40 mm) clears the 43 mm dimension.

**Required CAD change:** widen the main base-top slot to the chosen PSU's end-face
cross-section + ~3 mm clearance per side. For a ~115 × 40 mm PSU end that is
≈ **121 × 46 mm**. Finalize the exact dimensions against the datasheet of the
PSU actually purchased. Also confirm unobstructed vertical clearance for the PSU
height in the lower cavity (clear of the base-top pegs and the first LED frame)
and add PSU mounting features.

## 6. Bill of Materials

| Item | Qty | Notes |
|------|-----|-------|
| Teensy 4.0 | 1 | IMXRT1062 dev board |
| WS2815 12 V strip, 60/m, IP30, black PCB | ~53.6 m = 11 × 5 m reels | 4 reels cover 4 sides with offcut spare; 73 LED/strip |
| 12 V PSU ~1000 W | 1 | e.g. Mean Well LRS-1000-12 (83 A); pending PSU-fit decision |
| 12 V→5 V buck converter | 1 | standalone 5 V rail for Teensy + 74HCT245 |
| 74HCT245 octal level shifter | 1 | 3.3 V→5 V on 4 data lines (+ protoboard/breakout) |
| 470 Ω resistor | 4 | series on each data line |
| Bus bar / 10–12 AWG trunk | — | 12 V distribution for ~80 A |
| 18–20 AWG wire | — | per-strip power taps |
| Main DC fuse + holder (~100 A) | 1 | PSU output |
| Per-branch fuses + holders | — | bus branches |
| 1000 µF / 25 V capacitor | 1 | across PSU output at first injection |
| Potentiometer + knob | reuse | existing pattern selector |
| IEC inlet + fuse + switch | 1 | AC side |
| Mains wire, earth bonding | — | AC side |
| Connectors / heat-shrink / mounting hardware | — | assembly |

Exact quantities and part links finalized in the implementation plan.

## 7. Testing

- **`XY()` unit check** — host-compile or a test sketch that prints indices;
  verify each (col, row) → correct lane/index and serpentine direction before
  touching hardware.
- **Bench** — cut one reel into a few strips, wire 1–2 lanes, confirm parallel
  output, framerate (`FastLED.getFPS()`), pattern selection, and hysteresis.
- **Power** — measure real draw at the chosen brightness cap; confirm within PSU.
- **Incremental** — fully validate one side (1 lane, 11 strips) before
  replicating ×4.
- **Color order** — RGBCalibrate on the first WS2815 strip to confirm GRB.

## 8. Open Items
1. **Base-top hole enlargement** (§5) — the vertical PSU (~115 × 40 mm end) does
   not pass the existing ~43 × 100 mm slot; widen to ≈121 × 46 mm (PSU end + 3 mm/
   side), final size from the purchased PSU's datasheet. Also confirm vertical
   clearance and add mounting features. Required CAD change, not just a check.
2. **WS2815 color order** — confirm GRB on first strip.
3. **Serpentine parity** in `XY()` — confirm against actual wiring direction.
