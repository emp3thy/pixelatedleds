#pragma once

// for differentiating between doing a line and a matrix
#define ISMATRIX true

// ─── Vertical-strip LED frame — Teensy 4.0, 4-lane parallel output ───
// 44 vertical strips around the perimeter (11 per side, 4 sides). Tower is
// 4 ft (1219 mm) tall at 60 LEDs/m (16.67 mm pitch) → 73 LEDs per strip.
//
// **Board: Teensy 4.0 (IMXRT1062, ~1 MB RAM).** The old AVR Uno 2 KB SRAM
// cap is gone — leds[] = 3212 × 3 B ≈ 9.6 KB is trivial here.
//
// **Why parallel:** WS2811/2815 data is 800 kHz = 30 µs/LED, independent of
// MCU speed. All 3212 LEDs on one pin = 3212 × 30 µs ≈ 96 ms → ~10 fps.
// Splitting into one data lane per side keeps each chain short:
//   per lane = 11 strips × 73 = 803 LEDs → 803 × 30 µs ≈ 24 ms → ~41 fps,
//   comfortably above 24 fps film / 25 fps PAL / 30 fps NTSC.
//
// **Array layout:** leds[] is one contiguous block partitioned lane-major —
// lane L owns leds[L*LEDS_PER_LANE .. (L+1)*LEDS_PER_LANE). The Teensy-4
// block-clockless driver maps lanes to a fixed pin sequence selected by the
// first pin; starting at pin 1 the four lanes drive pins 1, 0, 24, 25.
// XY() in XYMatrix.h converts (col,row) → this partitioned index.
//
// Taller towers / higher fps: add lanes (8 lanes → ~401 LEDs/lane → ~83 fps).
#define NUM_COLS 44
#define NUM_ROWS 73
#define NUM_LANES 4
#define COLS_PER_LANE (NUM_COLS / NUM_LANES)      // 11 strips per side
#define LEDS_PER_LANE (COLS_PER_LANE * NUM_ROWS)  // 803 LEDs per lane
#define NUM_LEDS (NUM_LANES * LEDS_PER_LANE)       // 3212 total
