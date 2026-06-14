#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Ocean day/night cycle. Spec:
//   docs/superpowers/specs/2026-06-14-ocean-sunrise-cycle-design.md
// Layered compositor; sky=rows 0..44, ocean=rows 45..59 (bottom 1/4).
// ============================================================================
#define OCEAN_HORIZON   45        // first ocean row
#define OCEAN_PEAK_Y     8        // sun's high-noon row
#define OCEAN_SUN_X     30        // sun centre column
#define OCEAN_SUN_R      5        // sun disc radius
#define OCEAN_MOON_X    44        // moon centre column (right side)
#define OCEAN_MOON_Y    12        // moon centre row
#define OCEAN_MOON_R     3        // moon disc radius
#define OCEAN_T_MS  20000UL   // DEV: restore to 210000UL in Task 8      // full cycle length (~3.5 min)

// sub-phase boundaries as a fraction of the cycle
#define OCEAN_SR_END  0.18f       // end of sunrise
#define OCEAN_DAY_END 0.40f       // end of day
#define OCEAN_SS_END  0.62f       // end of sunset (then night to 1.0)

// ---- math helpers ----
static inline float oceanSmooth(float t){ if(t<0)t=0; if(t>1)t=1; return t*t*(3.0f-2.0f*t); }
static inline uint8_t oceanLerp8(uint8_t a,uint8_t b,float t){ return (uint8_t)(a+(b-a)*t+0.5f); }
static inline CRGB oceanLerpRGB(const CRGB&a,const CRGB&b,float t){
  return CRGB(oceanLerp8(a.r,b.r,t), oceanLerp8(a.g,b.g,t), oceanLerp8(a.b,b.b,t));
}

// ---- sky palette keyframes (top colour, horizon colour) around the cycle ----
// night -> dawn -> day -> sunset -> night. Night top is a *visible* navy.
static const float OCEAN_KF_PHASE[]   = {0.00f, 0.09f, 0.29f, 0.51f, 0.62f, 1.01f};
static const CRGB  OCEAN_KF_TOP[]     = {CRGB(0x0A1430),CRGB(0x232A63),CRGB(0x2F7FD6),CRGB(0x2E2350),CRGB(0x0A1430),CRGB(0x0A1430)};
static const CRGB  OCEAN_KF_HORIZON[] = {CRGB(0x122044),CRGB(0xFFD98C),CRGB(0xD8F0FF),CRGB(0xFFCE5A),CRGB(0x122044),CRGB(0x122044)};

// ---- shared per-frame cycle state ----
struct OceanState {
  float    phase;        // 0..1 position in the cycle
  float    nf;           // night factor 0 (day) .. 1 (night)
  float    alt;          // sun altitude 0 (horizon) .. 1 (peak)
  bool     sunUp;        // is the sun above the horizon (drawn)?
  CRGB     skyTop, skyHorizon;
  float    cloudCov;     // rendered cloud coverage %
};
static OceanState ocean;

// ---- cloud coverage walk state ----
static float   oceanCloudPrev = 0.0f;   // coverage target at start of current stage
static float   oceanCloudNext = 0.0f;   // coverage target at end of current stage
static int8_t  oceanCloudDir  = 1;      // +1 rising, -1 falling
static int8_t  oceanLastStage = -1;     // last seen stage (0..3); -1 = uninitialised
static uint16_t oceanDayCount = 0;

static inline uint8_t oceanStage(float p){
  if(p<OCEAN_SR_END)  return 0;
  if(p<OCEAN_DAY_END) return 1;
  if(p<OCEAN_SS_END)  return 2;
  return 3;
}
static float oceanStageProgress(float p){
  if(p<OCEAN_SR_END)  return p/OCEAN_SR_END;
  if(p<OCEAN_DAY_END) return (p-OCEAN_SR_END)/(OCEAN_DAY_END-OCEAN_SR_END);
  if(p<OCEAN_SS_END)  return (p-OCEAN_DAY_END)/(OCEAN_SS_END-OCEAN_DAY_END);
  return (p-OCEAN_SS_END)/(1.0f-OCEAN_SS_END);
}
static float oceanNightFactor(float p){
  if(p<OCEAN_SR_END)  return 1.0f - oceanSmooth(p/OCEAN_SR_END);
  if(p<OCEAN_DAY_END) return 0.0f;
  if(p<OCEAN_SS_END)  return oceanSmooth((p-OCEAN_DAY_END)/(OCEAN_SS_END-OCEAN_DAY_END));
  return 1.0f;
}
static float oceanSunAlt(float p){
  if(p<OCEAN_SR_END)  return oceanSmooth(p/OCEAN_SR_END);
  if(p<OCEAN_DAY_END) return 1.0f;
  if(p<OCEAN_SS_END)  return 1.0f - oceanSmooth((p-OCEAN_DAY_END)/(OCEAN_SS_END-OCEAN_DAY_END));
  return 0.0f;
}
static void oceanSkyColors(float p, CRGB &top, CRGB &hor){
  uint8_t n = sizeof(OCEAN_KF_PHASE)/sizeof(float);
  for(uint8_t i=0;i<n-1;i++){
    if(p>=OCEAN_KF_PHASE[i] && p<OCEAN_KF_PHASE[i+1]){
      float t = oceanSmooth((p-OCEAN_KF_PHASE[i])/(OCEAN_KF_PHASE[i+1]-OCEAN_KF_PHASE[i]));
      top = oceanLerpRGB(OCEAN_KF_TOP[i],     OCEAN_KF_TOP[i+1],     t);
      hor = oceanLerpRGB(OCEAN_KF_HORIZON[i], OCEAN_KF_HORIZON[i+1], t);
      return;
    }
  }
  top = OCEAN_KF_TOP[0]; hor = OCEAN_KF_HORIZON[0];
}

// advance cloud + day counters once per stage transition
static void oceanUpdateStageState(float p){
  uint8_t st = oceanStage(p);
  if(st == oceanLastStage) return;
  // detect a wrap into stage 0 from stage 3 -> a new day
  if(st == 0 && oceanLastStage == 3) oceanDayCount++;
  oceanLastStage = st;
  // step cloud coverage 1..3% in the current direction, ping-pong at 0/25
  oceanCloudPrev = oceanCloudNext;
  uint8_t step = random8(1,4);            // 1..3
  oceanCloudNext += oceanCloudDir * (float)step;
  if(oceanCloudNext >= 25.0f){ oceanCloudNext = 25.0f; oceanCloudDir = -1; }
  if(oceanCloudNext <= 0.0f ){ oceanCloudNext = 0.0f;  oceanCloudDir = +1; }
}

// ---- layer stubs (filled in later tasks) ----
static void oceanDrawClouds()      {}
static void oceanDrawStars()       {}
static void oceanDrawMoon()        {}
static void oceanDrawOcean()       {}
static void oceanDrawReflections() {}

// additive write into a sky pixel, clamped
static inline void oceanAddPix(int16_t x,int16_t y,const CRGB&col,float a){
  if(a<=0.0f) return; if(a>1.0f) a=1.0f;
  if(x<0||x>=NUM_COLS||y<0||y>=OCEAN_HORIZON) return;
  leds[XY(x,y)] += CRGB((uint8_t)(col.r*a),(uint8_t)(col.g*a),(uint8_t)(col.b*a));
}

// Sun colour by altitude: deep orange-red low -> white-gold high.
static CRGB oceanSunColor(){
  CRGB low(0xFF,0x5A,0x2A), high(0xFF,0xF4,0xD0);
  return oceanLerpRGB(low, high, oceanSmooth(ocean.alt));
}

static void oceanDrawSun(){
  if(!ocean.sunUp) return;
  float sy = OCEAN_HORIZON - ocean.alt * (OCEAN_HORIZON - OCEAN_PEAK_Y); // disc centre row
  float sx = OCEAN_SUN_X;
  CRGB col = oceanSunColor();
  float lowness = 1.0f - oceanSmooth(ocean.alt);     // 1 at horizon, 0 at peak
  float haloR = OCEAN_SUN_R + 6.0f + lowness*8.0f;

  // disc + radial halo (no per-pixel atan2)
  for(int16_t y=0; y<OCEAN_HORIZON; y++){
    for(int16_t x=0; x<NUM_COLS; x++){
      float dx=x-sx, dy=y-sy, d=sqrtf(dx*dx+dy*dy);
      if(d <= OCEAN_SUN_R){ leds[XY(x,y)] = col; continue; }   // solid disc
      if(d < haloR){
        oceanAddPix(x,y,col,(1.0f-(d-OCEAN_SUN_R)/(haloR-OCEAN_SUN_R))*0.7f);
      }
    }
  }
  // sunburst rays: 12 explicit tapered spokes, stronger when the sun is low
  if(lowness > 0.05f){
    const uint8_t nRays = 12;
    float rayLen = (OCEAN_SUN_R + 4.0f) + lowness*14.0f;
    for(uint8_t r=0;r<nRays;r++){
      float ang = (2.0f*3.14159f*r)/nRays;
      float ca=cosf(ang), sa=sinf(ang);
      for(float dd=OCEAN_SUN_R; dd<rayLen; dd+=1.0f){
        float a = (1.0f - dd/rayLen) * lowness * 0.6f;
        oceanAddPix((int16_t)(sx+ca*dd+0.5f),(int16_t)(sy+sa*dd+0.5f),col,a);
      }
    }
  }
}

// ---- sky layer: vertical gradient top->horizon over the sky rows ----
static void oceanDrawSky(){
  for(uint8_t y=0; y<OCEAN_HORIZON; y++){
    float t = (float)y/(float)(OCEAN_HORIZON-1);     // 0 top .. 1 horizon
    CRGB c = oceanLerpRGB(ocean.skyTop, ocean.skyHorizon, t);
    for(uint8_t x=0;x<NUM_COLS;x++) leds[XY(x,y)] = c;
  }
}

// ---- entry point ----
void oceanSunrise(){
  uint32_t ms = millis() % OCEAN_T_MS;
  ocean.phase = (float)ms / (float)OCEAN_T_MS;
  oceanUpdateStageState(ocean.phase);
  ocean.nf  = oceanNightFactor(ocean.phase);
  ocean.alt = oceanSunAlt(ocean.phase);
  ocean.sunUp = ocean.phase < OCEAN_SS_END;     // sun shown sunrise..sunset
  oceanSkyColors(ocean.phase, ocean.skyTop, ocean.skyHorizon);
  // rendered cloud coverage eases across the stage between prev/next targets
  ocean.cloudCov = oceanCloudPrev +
                   (oceanCloudNext - oceanCloudPrev) * oceanSmooth(oceanStageProgress(ocean.phase));

  oceanDrawSky();
  oceanDrawClouds();
  oceanDrawStars();
  oceanDrawMoon();
  oceanDrawSun();
  oceanDrawOcean();
  oceanDrawReflections();
  FastLED.show();
}
