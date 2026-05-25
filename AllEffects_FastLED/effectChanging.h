#pragma once

#include <FastLED.h>

int lastSetResistence = -20;

byte convertToSelectedEffect(int resistence)
{
  int listOfPatternsForRectangularMatrix[] = {0, 1, 2, 3, 14, 15, 6, 7, 17, 9, 16, 11, 12, 13};
  int listOfPatternsForSquareMatrix[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  int listOfPatternsForSimpleLedStrip[] = {0,1,6,7,8, 10, 12, 14, 15, 16, 17, 18,0, 13};
  int difference = resistence - lastSetResistence;
  byte result = resistence / 78;
  if (result > 13)
  {
    result = 13;
  }
  if (!ISMATRIX)
    return listOfPatternsForSimpleLedStrip[result];
  if (NUM_COLS == NUM_ROWS)
    return listOfPatternsForSquareMatrix[result];
  return listOfPatternsForRectangularMatrix[result];
}