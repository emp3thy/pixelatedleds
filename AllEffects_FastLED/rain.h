#pragma once

#include <FastLED.h>
//*** Rain
byte rain[NUM_LEDS];
byte speed = 2;
int rainTick=0;

void updaterain() {
  for (byte i = 0; i < NUM_COLS; i++) {
    for (byte j = 0; j < NUM_ROWS; j++) {
      byte layer = rain[XY(i, ((j + speed + random8(2) + NUM_ROWS) % NUM_ROWS))];   //fake scroll based on shift coordinate
      // random8(2) add glitchy look
      if (layer) {
        leds[XY(i,j)] = CHSV(141, 255, BRIGHTNESS);
      }
    }
  }

  speed = (speed + 1) % NUM_ROWS;
  fadeToBlackBy(leds, NUM_LEDS, 40);
  blurRows(leds, NUM_COLS, NUM_ROWS, 16);      //if you want
  FastLED.show();
} //updaterain

void changepattern () {
  int rand1 = random16 (NUM_LEDS);
  int rand2 = random16 (NUM_LEDS);
  if ((rain[rand1] == 1) && (rain[rand2] == 0) )   //simple get two random dot 1 and 0 and swap it,
  {
    rain[rand1] = 0;  //this will not change total number of dots
    rain[rand2] = 1;
  }
  rainTick++;
  if(rainTick==2)
  {
    rainTick=0;
    updaterain();
  }
} //changepattern

void rainInit() {                               //init array of dots. run once
  for (int i = 0; i < NUM_LEDS; i++) {
    if (random8(15) == 0) {
      rain[i] = 1;  //random8(20) number of dots. decrease for more dots
    }
    else {
      rain[i] = 0;
    }
  }
} //raininit

