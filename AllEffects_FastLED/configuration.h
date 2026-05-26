#pragma once

// for differentiating between doing a line and a matrix
#define ISMATRIX true

// Vertical-strip LED frame: 44 vertical strips around perimeter (11/side).
// NUM_ROWS = LEDs per strip (depends on tower height; 16.67mm pitch).
//
// **AVR Uno SRAM constraint**: 2048 bytes total. The generative effects
// added in PR #2 (Mondrian leaf list, voronoi seeds, palette state)
// already consume ~1380 bytes of globals. Each LED adds 3 bytes to leds[],
// leaving room for at most ~220 LEDs in `leds[]`. With NUM_COLS=44 that
// caps NUM_ROWS at 4–5. NUM_ROWS=4 fits with margin.
//
// For taller towers move to a board with more SRAM:
//   Arduino Mega 2560 (8KB SRAM): NUM_ROWS up to ~60 → 12cm strip → 7 frames.
//   ESP32 (320KB SRAM): essentially unlimited.
#define NUM_COLS 44
#define NUM_ROWS 4
#define NUM_LEDS 176
