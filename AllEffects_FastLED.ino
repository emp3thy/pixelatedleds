#include "FastLED.h"
#include <EEPROM.h>
#define NUM_LEDS 100
CRGB leds[NUM_LEDS];
#define PIN 5

#define BUTTON 2
#define BRIGHTNESS          254
#define FRAMES_PER_SECOND  120
#define NUM_ROWS 10
#define NUM_COLS 10

byte selectedEffect = -1;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
byte rain[NUM_LEDS];
byte counter = 1;
int speed = 2;

void setup()
{
  delay( 3000); // 3 second delay for boot recovery, and a moment of silence
  FastLED.addLeds<WS2811, PIN, GRB>(leds, NUM_LEDS)
  .setCorrection( TypicalLEDStrip );
  pinMode(2, INPUT_PULLUP); // internal pull-up resistor
  attachInterrupt (digitalPinToInterrupt (BUTTON), changeEffect, CHANGE); // pressed
  FastLED.setBrightness(254);
  rainInit();
  selectedEffect = EEPROM.read(1);
}

void loop() {

  EVERY_N_MILLISECONDS(100) {
    switch (selectedEffect)
    {
      case 2:
        metaBalls();
        break;
      case 7:
        updaterain();
        break;
      case 3:
        make_fire();
        break;
      default:
        break;
    }
  }

  EVERY_N_MILLISECONDS(30) {
    switch (selectedEffect)
    {
      case 7:
        changepattern();
        break;
      default:
        break;
    }

  }
  EVERY_N_MILLISECONDS( 20 ) {
    switch (selectedEffect)
    {
      case 5 :
      case 10 :
      case 4 :
      case 8 :
      case 9 :
        gHue++;
        break;
      case 6:
        justWhite();
        break;
      case 1:
        pacifica_loop();
        break;
      default:
        break;
    }        FastLED.show();
  }
  if (selectedEffect > 10 || selectedEffect < 0) {
    selectedEffect = 0;
    EEPROM.write(1, selectedEffect);
  }

  switch (selectedEffect) {

    case 5  :
      rainbow();
      break;
    case 10  :
      rainbowWithGlitter();
      break;
    case 4  :
      confetti();
      break;
    case 8  :
      sinelon();
      break;
    case 9  :
      bpm();
      break;
    case 0:
      pride();
      break;
    default:
      break;
  }
}
// *************************
// ** LEDEffect Functions **
// *************************


void changeEffect() {
  gHue = 0;
  if (digitalRead (BUTTON) == HIGH) {
    selectedEffect++;
    fadeToBlackBy( leds, NUM_LEDS, 20);
    EEPROM.write(1, selectedEffect);
    asm volatile ("  jmp 0");
  }
}

// *** REPLACE FROM HERE ***
void metaBalls()
{
  uint8_t bx1 = beatsin8(15, 0, NUM_COLS - 1, 0, 0);
  uint8_t by1 = beatsin8(18, 0, NUM_ROWS - 1, 0, 0);
  uint8_t bx2 = beatsin8(28, 0, NUM_COLS - 1, 0, 16);
  uint8_t by2 = beatsin8(23, 0, NUM_ROWS - 1, 0, 16);
  uint8_t bx3 = beatsin8(30, 0, NUM_COLS - 1, 0, 32);
  uint8_t by3 = beatsin8(24, 0, NUM_ROWS - 1, 0, 32);
  uint8_t bx4 = beatsin8(17, 0, NUM_COLS - 1, 0, 64);
  uint8_t by4 = beatsin8(25, 0, NUM_ROWS - 1, 0, 64);
  uint8_t bx5 = beatsin8(19, 0, NUM_COLS - 1, 0, 85);
  uint8_t by5 = beatsin8(21, 0, NUM_ROWS - 1, 0, 85);

  for (int i = 0; i < NUM_COLS; i++)    {
    for (int j = 0; j < NUM_ROWS; j++) {

      byte  sum =  dist(i, j, bx1, by1);
      sum = qadd8(sum, dist(i, j, bx2, by2));
      sum = qadd8(sum, dist(i, j, bx3, by3));
      sum = qadd8(sum, dist(i, j, bx4, by4));
      sum = qadd8(sum, dist(i, j, bx5, by5));

      leds[XY (i, j)] =  ColorFromPalette(HeatColors_p, sum + 165, BRIGHTNESS);
    }
  }

  blur2d(leds, NUM_COLS, NUM_ROWS, 32 );
  FastLED.show();
}

byte dist (uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)  {
  int a = y2 - y1;
  int b = x2 - x1;
  a *= a;
  b *= b;
  byte dist = 220 / sqrt16(a + b);
  return dist;
}

uint16_t XY (uint8_t x, uint8_t y) {
  if ( (x % 2) == 0)
  {
    return x * 10 + y;
  }
  return x * 10 + (9 - y);
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void justWhite()
{
  for (int i = 0; i < NUM_LEDS; i++ ) {
    leds[i] = CHSV(0, 0, BRIGHTNESS);
  }
  FastLED.show();
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS - 1 );
  leds[pos] += CHSV( gHue, 255, 192);
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for ( int i = 0; i < 8; i++) {
    leds[beatsin16( i + 7, 0, NUM_LEDS - 1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
// *** PACIFICA ****

CRGBPalette16 pacifica_palette_1 =
{ 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
  0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50
};
CRGBPalette16 pacifica_palette_2 =
{ 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
  0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F
};
CRGBPalette16 pacifica_palette_3 =
{ 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
  0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF
};


void pacifica_loop()
{
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011, 10, 13));
  sCIStart2 -= (deltams21 * beatsin88(777, 8, 11));
  sCIStart3 -= (deltams1 * beatsin88(501, 5, 7));
  sCIStart4 -= (deltams2 * beatsin88(257, 4, 6));

  // Clear out the LED array to a dim background blue-green
  fill_solid( leds, NUM_LEDS, CRGB( 2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer( pacifica_palette_1, sCIStart1, beatsin16( 3, 11 * 256, 14 * 256), beatsin8( 10, 70, 130), 0 - beat16( 301) );
  pacifica_one_layer( pacifica_palette_2, sCIStart2, beatsin16( 4,  6 * 256,  9 * 256), beatsin8( 17, 40,  80), beat16( 401) );
  pacifica_one_layer( pacifica_palette_3, sCIStart3, 6 * 256, beatsin8( 9, 10, 38), 0 - beat16(503));
  pacifica_one_layer( pacifica_palette_3, sCIStart4, 5 * 256, beatsin8( 8, 10, 28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
  FastLED.show();
}

// Add one layer of waves into the led array
void pacifica_one_layer( CRGBPalette16 & p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16( waveangle ) + 32768;
    uint16_t cs = scale16( s16 , wavescale_half ) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16( ci) + 32768;
    uint8_t sindex8 = scale16( sindex16, 240);
    CRGB c = ColorFromPalette( p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps()
{
  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );

  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if ( l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8( overage, overage);
      leds[i] += CRGB( overage, overage2, qadd8( overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors()
{
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].blue = scale8( leds[i].blue,  145);
    leds[i].green = scale8( leds[i].green, 200);
    leds[i] |= CRGB( 2, 5, 7);
  }
}

// *** FIRE!

#define  MAT_COL_MAJOR       /* define if matrix is column-major (that is pixel 1 is in the same column as pixel 0) */
#undef MAT_TOP             /* define if matrix 0,0 is in top row of display; undef if bottom */
#define MAT_LEFT            /* define if matrix 0,0 is on left edge of display; undef if right */
#define MAT_ZIGZAG          /* define if matrix zig-zags ---> <--- ---> <---; undef if scanning ---> ---> ---> */

/* Display size; can be smaller than matrix size, and if so, you can move the origin.
   This allows you to have a small fire display on a large matrix sharing the display
   with other stuff. See README at Github. */
const uint16_t rows = NUM_ROWS;
const uint16_t cols = NUM_COLS;
const uint16_t xorg = 0;
const uint16_t yorg = 0;

/* Flare constants */
const uint8_t flarerows = 2;    /* number of rows (from bottom) allowed to flare */
const uint8_t maxflare = 8;     /* max number of simultaneous flares */
const uint8_t flarechance = 50; /* chance (%) of a new flare (if there's room) */
const uint8_t flaredecay = 15;  /* decay rate of flare radiation; 14 is good */


/* This is the map of colors from coolest (black) to hottest. Want blue flames? Go for it! */
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
  0x807080
};
const uint8_t NCOLORS = (sizeof(colors) / sizeof(colors[0]));

uint8_t pix[rows][cols];
uint8_t nflare = 0;
uint32_t flare[maxflare];

/** pos - convert col/row to pixel position index. This takes into account
    the serpentine display, and mirroring the display so that 0,0 is the
    bottom left corner and (NUM_COLS-1,NUM_ROWS-1) is upper right. You may need
    to jockey this around if your display is different.
*/
#ifndef MAT_LEFT
#define __MAT_RIGHT
#endif
#ifndef MAT_TOP
#define __MAT_BOTTOM
#endif

const uint8_t phy_h = NUM_COLS;
const uint8_t phy_w = NUM_ROWS;
uint16_t pos( uint16_t col, uint16_t row ) {
  uint16_t phy_x = xorg + (uint16_t) row;
  uint16_t phy_y = yorg + (uint16_t) col;

#if defined(MAT_LEFT) && defined(MAT_ZIGZAG)
  if ( ( phy_y & 1 ) == 1 ) {
    phy_x = phy_w - phy_x - 1;
  }
#elif defined(__MAT_RIGHT) && defined(MAT_ZIGZAG)
  if ( ( phy_y & 1 ) == 0 ) {
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

uint32_t isqrt(uint32_t n) {
  if ( n < 2 ) return n;
  uint32_t smallCandidate = isqrt(n >> 2) << 1;
  uint32_t largeCandidate = smallCandidate + 1;
  return (largeCandidate * largeCandidate > n) ? smallCandidate : largeCandidate;
}

// Set pixels to intensity around flare
void glow( int x, int y, int z ) {
  int b = z * 10 / flaredecay + 1;
  for ( int i = (y - b); i < (y + b); ++i ) {
    for ( int j = (x - b); j < (x + b); ++j ) {
      if ( i >= 0 && j >= 0 && i < rows && j < cols ) {
        int d = ( flaredecay * isqrt((x - j) * (x - j) + (y - i) * (y - i)) + 5 ) / 10;
        uint8_t n = 0;
        if ( z > d ) n = z - d;
        if ( n > pix[i][j] ) { // can only get brighter
          pix[i][j] = n;
        }
      }
    }
  }
}

void newflare() {
  if ( nflare < maxflare && random(1, 101) <= flarechance ) {
    int x = random(0, cols);
    int y = random(0, flarerows);
    int z = NCOLORS - 1;
    flare[nflare++] = (z << 16) | (y << 8) | (x & 0xff);
    glow( x, y, z );
  }
}

/** make_fire() animates the fire display. It should be called from the
    loop periodically (at least as often as is required to maintain the
    configured refresh rate). Better to call it too often than not enough.
    It will not refresh faster than the configured rate. But if you don't
    call it frequently enough, the refresh rate may be lower than
    configured.
*/
void make_fire() {
  uint16_t i, j;

  // First, move all existing heat points up the display and fade
  for ( i = rows - 1; i > 0; --i ) {
    for ( j = 0; j < cols; ++j ) {
      uint8_t n = 0;
      if ( pix[i - 1][j] > 0 )
        n = pix[i - 1][j] - 1;
      pix[i][j] = n;
    }
  }

  // Heat the bottom row
  for ( j = 0; j < cols; ++j ) {
    i = pix[0][j];
    if ( i > 0 ) {
      pix[0][j] = random(NCOLORS - 6, NCOLORS - 2);
    }
  }

  // flare
  for ( i = 0; i < nflare; ++i ) {
    int x = flare[i] & 0xff;
    int y = (flare[i] >> 8) & 0xff;
    int z = (flare[i] >> 16) & 0xff;
    glow( x, y, z );
    if ( z > 1 ) {
      flare[i] = (flare[i] & 0xffff) | ((z - 1) << 16);
    } else {
      // This flare is out
      for ( int j = i + 1; j < nflare; ++j ) {
        flare[j - 1] = flare[j];
      }
      --nflare;
    }
  }
  newflare();

  // Set and draw
  for ( i = 0; i < rows; ++i ) {
    for ( j = 0; j < cols; ++j ) {
      leds[pos(j, i)] = colors[pix[i][j]];
    }
  }
  FastLED.show();
}

//*** - Pride rainbows
void pride()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }
  FastLED.show();
}
//*** Rain

void changepattern () {
  int rand1 = random16 (NUM_LEDS);
  int rand2 = random16 (NUM_LEDS);
  if ((rain[rand1] == 1) && (rain[rand2] == 0) )   //simple get two random dot 1 and 0 and swap it,
  {
    rain[rand1] = 0;  //this will not change total number of dots
    rain[rand2] = 1;
  }
} //changepattern

void rainInit() {                               //init array of dots. run once
  for (int i = 0; i < NUM_LEDS; i++) {
    if (random8(10) == 0) {
      rain[i] = 1;  //random8(20) number of dots. decrease for more dots
    }
    else {
      rain[i] = 0;
    }
  }
} //raininit

void updaterain() {
  for (byte i = 0; i < NUM_COLS; i++) {
    for (byte j = 0; j < NUM_ROWS; j++) {
      byte layer = rain[XY(i, ((j + speed + random8(2) + NUM_ROWS) % NUM_ROWS))];   //fake scroll based on shift coordinate
      // random8(2) add glitchy look
      if (layer) {
        leds[XY((NUM_COLS - 1) - i, (NUM_ROWS - 1) - j)] = CHSV(141, 255, BRIGHTNESS);
      }
    }
  }

  speed ++;
  fadeToBlackBy(leds, NUM_LEDS, 40);
  blurRows(leds, NUM_COLS, NUM_ROWS, 16);      //if you want
  FastLED.show();
} //updaterain
