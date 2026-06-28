#pragma once

#include <FastLED.h>

int lastSetResistence = -20;

byte convertToSelectedEffect(int resistence)
{
  // Both matrix tables share the same slider→effect mapping so the viewer's
  // NAMES labels (pride…voronoi…jusBlack…water lilies…kusama) stay correct in
  // either aspect ratio. 2D art effects: mondrian=19, plasma=20, aurora=21,
  // voronoi=22, waterLilies=23, kusamaDots=24.
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 13};
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 19, 20, 21, 22, 23, 24, 25, 26, 13};
  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13,  0,  0,  0,  0,  0,  0,  0};
  int difference = resistence - lastSetResistence;
  byte result = resistence / 57;
  if (result > 21)
  {
    result = 21;
  }
  if (!ISMATRIX)
    return listOfPatternsForSimpleLedStrip[result];
  if (NUM_COLS == NUM_ROWS)
    return listOfPatternsForSquareMatrix[result];
  return listOfPatternsForRectangularMatrix[result];
}