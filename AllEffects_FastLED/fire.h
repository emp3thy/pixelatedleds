#pragma once

#include "FastLED.h"

/* MATRIX CONFIGURATION -- PLEASE SEE THE README (GITHUB LINK ABOVE) */
#define MAT_COL_MAJOR /* define if matrix is column-major (that is pixel 1 is in the same column as pixel 0) */
#undef MAT_TOP        /* define if matrix 0,0 is in top row of display; undef if bottom */
#define MAT_LEFT       /* define if matrix 0,0 is on left edge of display; undef if right */
#define MAT_ZIGZAG    /* define if matrix zig-zags ---> <--- ---> <---; undef if scanning ---> ---> ---> */
#define FPS 60        /* Refresh rate (raised so flames climb faster) */

const uint16_t fireRows = NUM_ROWS;
const uint16_t fireCols = NUM_COLS;
const uint16_t xorg = 0;
const uint16_t yorg = 0;

/* Heat palette: coolest (black) to hottest (near-white). */
const uint32_t colors[] = {
    0x000000,
    0x100000,
    0x300000,
    0x600000,
    0x800000,
    0xA00000,
    0xC02000,
    0xC04000,
    0xC06000,
    0xC08000,
    0x807080};
const uint8_t NCOLORS = (sizeof(colors) / sizeof(colors[0]));

// Fire2012-style cooling/sparking model. Unlike the old fixed-decrement model
// (which died after ~8 rows because the palette only had 11 levels), heat is
// tracked at full 0..255 resolution and COOLING is scaled by the matrix height,
// so flames climb the entire 73-row tower with a natural falloff toward the top.
//   FIRE_COOLING  : higher = shorter flames. Tuned for the tall vertical frame.
//   FIRE_SPARKING : chance (0-255) of a new spark at the base each frame.
#define FIRE_COOLING  70
#define FIRE_SPARKING 120

// heat[row][col], row 0 = bottom (hottest). Static → lives in .bss, not on stack.
static uint8_t heat[fireRows][fireCols];

/** pos - convert col/row to pixel index, honouring the serpentine/orientation
 *  config above so row 0 is the bottom row. */
#ifndef MAT_LEFT
#define __MAT_RIGHT
#endif
#ifndef MAT_TOP
#define __MAT_BOTTOM
#endif
#if defined(MAT_COL_MAJOR)
const uint8_t phy_h = NUM_COLS;
const uint8_t phy_w = NUM_ROWS;
#else
const uint8_t phy_h = NUM_ROWS;
const uint8_t phy_w = NUM_COLS;
#endif
uint16_t pos(uint16_t col, uint16_t row)
{
#if defined(MAT_COL_MAJOR)
  uint16_t phy_x = xorg + (uint16_t)row;
  uint16_t phy_y = yorg + (uint16_t)col;
#else
  uint16_t phy_x = xorg + (uint16_t)col;
  uint16_t phy_y = yorg + (uint16_t)row;
#endif
#if defined(MAT_LEFT) && defined(MAT_ZIGZAG)
  if ((phy_y & 1) == 1)
  {
    phy_x = phy_w - phy_x - 1;
  }
#elif defined(__MAT_RIGHT) && defined(MAT_ZIGZAG)
  if ((phy_y & 1) == 0)
  {
    phy_x = phy_w - phy_x - 1;
  }
#elif defined(__MAT_RIGHT)
  phy_x = phy_w - phy_x - 1;
#endif
#if defined(MAT_TOP) and defined(MAT_COL_MAJOR)
  phy_x = phy_w - phy_x - 1;
#elif defined(MAT_TOP)
  phy_y = phy_h - phy_y - 1;
#endif
  return phy_x + phy_y * phy_w;
}

// Map a 0..255 heat value onto the colors[] palette with linear interpolation.
CRGB heatToColor(uint8_t h)
{
  uint16_t scaled = (uint16_t)h * (NCOLORS - 1); // 0 .. (NCOLORS-1)*255
  uint8_t idx = scaled / 255;                    // palette slot
  if (idx >= NCOLORS - 1)
    return CRGB(colors[NCOLORS - 1]);
  uint8_t frac = scaled % 255;                   // blend toward next slot
  return blend(CRGB(colors[idx]), CRGB(colors[idx + 1]), frac);
}

unsigned long fireNextMs = 0;
void make_fire()
{
  if (fireNextMs > millis())
    return;
  fireNextMs = millis() + (1000 / FPS);

  for (uint16_t j = 0; j < fireCols; j++)
  {
    // 1) Cool every cell a little. Cooling is scaled by height so the flame
    //    front reaches the top of a tall display instead of dying low.
    for (uint16_t i = 0; i < fireRows; i++)
    {
      uint8_t cooldown = random8(0, ((FIRE_COOLING * 10) / fireRows) + 2);
      heat[i][j] = (cooldown >= heat[i][j]) ? 0 : (heat[i][j] - cooldown);
    }

    // 2) Heat drifts upward and diffuses (average of the two cells below).
    for (uint16_t i = fireRows - 1; i >= 2; i--)
    {
      heat[i][j] = (heat[i - 1][j] + heat[i - 2][j] + heat[i - 2][j]) / 3;
    }

    // 3) Randomly ignite new sparks near the base.
    if (random8() < FIRE_SPARKING)
    {
      uint8_t y = random8(fireRows < 3 ? fireRows : 3);
      heat[y][j] = qadd8(heat[y][j], random8(160, 255));
    }

    // 4) Render this column. heat row 0 is the base (hottest); map it to the
    //    bottom of the display so flames rise upward (not inverted).
    for (uint16_t i = 0; i < fireRows; i++)
    {
      leds[pos(j, fireRows - 1 - i)] = heatToColor(heat[i][j]);
    }
  }
  FastLED.show();
}
