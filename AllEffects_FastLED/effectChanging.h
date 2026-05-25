#pragma once

#include <FastLED.h>

int lastSetResistence = -20;

byte convertToSelectedEffect(int resistence)
{
  static const byte listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13, 19, 20, 21, 22};
  static const byte listOfPatternsForSquareMatrix[]      = {0, 1, 2, 3,  4,  5, 6, 7,  8, 9, 10, 11, 12, 13,  0,  0,  0,  0};
  static const byte listOfPatternsForSimpleLedStrip[]    = {0, 1, 6, 7,  8, 10, 12,14, 15,16, 17, 18,  0, 13,  0,  0,  0,  0};
  int difference = resistence - lastSetResistence;
  byte result = resistence / 57;
  if (result > 17)
  {
    result = 17;
  }
  if (!ISMATRIX)
    return listOfPatternsForSimpleLedStrip[result];
  if (NUM_COLS == NUM_ROWS)
    return listOfPatternsForSquareMatrix[result];
  return listOfPatternsForRectangularMatrix[result];
}