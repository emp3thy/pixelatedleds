#pragma once

// for differentiating between doing a line and a matrix
#define ISMATRIX true

// ─── Flat square LED canvas — Teensy 4.0, 4-lane parallel output ───
// 60 vertical strips of 60 LEDs each, evenly spaced at 60 LEDs/m
// (16.67 mm pitch) → 1000 mm × 1000 mm (1.0 m) square canvas, 3600 LEDs.
//
// **Board: Teensy 4.0 (IMXRT1062, ~1 MB RAM).** The old AVR Uno 2 KB SRAM
// cap is gone — leds[] = 3600 × 3 B ≈ 10.8 KB is trivial here.
//
// **Why parallel:** WS2811/2815 data is 800 kHz = 30 µs/LED, independent of
// MCU speed. All 3600 LEDs on one pin = 3600 × 30 µs ≈ 108 ms → ~9 fps.
// Splitting the canvas into 4 column-blocks (one data lane each) keeps each
// chain short:
//   per lane = 15 strips × 60 = 900 LEDs → 900 × 30 µs ≈ 27 ms → ~37 fps,
//   comfortably above 24 fps film / 25 fps PAL / 30 fps NTSC.
//
// **Array layout:** leds[] is one contiguous block partitioned lane-major —
// lane L owns columns [L*COLS_PER_LANE .. ) = leds[L*LEDS_PER_LANE .. ).
// The Teensy-4 block-clockless driver maps lanes to a fixed pin sequence
// selected by the first pin; starting at pin 1 the four lanes drive pins
// 1, 0, 24, 25. XY() in XYMatrix.h converts (col,row) → this index.
//
// Higher fps: add lanes (8 lanes → 450 LEDs/lane → ~74 fps).
#define NUM_COLS 60
#define NUM_ROWS 60
#define NUM_LANES 4
#define COLS_PER_LANE (NUM_COLS / NUM_LANES)      // 15 strips per lane
#define LEDS_PER_LANE (COLS_PER_LANE * NUM_ROWS)  // 900 LEDs per lane
#define NUM_LEDS (NUM_LANES * LEDS_PER_LANE)       // 3600 total
