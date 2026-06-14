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
#define OCEAN_MOON_R     4        // moon disc radius
#define OCEAN_T_MS  210000UL      // full cycle length (~3.5 min)

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
// Sunset boundary is 0.40..0.62. Hold "day" until 0.47 (~1/3 of the way down)
// before warming, peak sunset near the horizon (0.585), then night by 0.66.
static const float OCEAN_KF_PHASE[]   = {0.00f, 0.05f, 0.13f, 0.47f, 0.585f, 0.66f, 1.01f};
static const CRGB  OCEAN_KF_TOP[]     = {CRGB(0x0A1430),CRGB(0x232A63),CRGB(0x2F7FD6),CRGB(0x2F7FD6),CRGB(0x2E2350),CRGB(0x0A1430),CRGB(0x0A1430)};
static const CRGB  OCEAN_KF_HORIZON[] = {CRGB(0x122044),CRGB(0xFFD98C),CRGB(0xD8F0FF),CRGB(0xD8F0FF),CRGB(0xFFCE5A),CRGB(0x122044),CRGB(0x122044)};

// ---- shared per-frame cycle state ----
struct OceanState {
  float    phase;        // 0..1 position in the cycle
  float    nf;           // night factor 0 (day) .. 1 (night)
  float    nightVis;     // steeper night curve for moon/stars (fade fast near low sun)
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

static void oceanDrawClouds(){
  float cov = ocean.cloudCov;
  if(cov < 5.0f) return;                          // <5% reads as clear
  if(cov > 25.0f) cov = 25.0f;
  uint32_t tms = millis();
  uint16_t t = (uint16_t)tms;
  // Deterministic cloud BANK: a drifting horizontal stretch whose half-width
  // grows with coverage. Low % -> a small clump in one place; high % -> a wide
  // band. Guarantees clouds appear (doesn't depend on noise hitting a peak).
  float bankCx = NUM_COLS*0.5f + (NUM_COLS*0.40f) * sinf(tms/9000.0f);  // slow drift
  float hw     = 5.0f + (cov/25.0f) * 40.0f;     // half-width: ~5 (clump) .. ~45 (band)
  // cloud tint: white by day, warmed toward horizon colour, dimmed at night
  CRGB warm = ocean.skyHorizon;
  CRGB tint = oceanLerpRGB(CRGB(235,238,245), warm, 0.45f);
  tint = oceanLerpRGB(tint, CRGB(60,66,86), ocean.nf*0.7f);
  for(uint8_t y=0; y<OCEAN_HORIZON-4; y++){       // sky only, leave a clear strip at horizon
    for(uint8_t x=0;x<NUM_COLS;x++){
      float dxb = fabsf((float)x - bankCx);
      if(dxb > hw) continue;                       // outside the bank -> clear sky
      float edgeFade = 1.0f - dxb/hw;              // 1 centre .. 0 bank edge
      // Anisotropic streak texture: wide in x, short in y -> horizontal wisps.
      uint8_t n = inoise8(x*13 + t/20, y*58);
      if(n <= 150) continue;                        // patchy density within the bank
      float a = ((n-150)/105.0f) * (0.35f + edgeFade*0.65f);
      // keep clouds in a horizontal mid-sky band; fade toward top + horizon strip
      a *= 1.0f - fabsf((float)y/(OCEAN_HORIZON-4) - 0.45f)*1.1f;
      if(a<=0) continue; if(a>1) a=1;
      leds[XY(x,y)] = oceanLerpRGB(leds[XY(x,y)], tint, a);
    }
  }
}

static CRGB oceanMoonColor(){ return CRGB(0xCF,0xDA,0xF2); } // pale blue-white
static void oceanDrawMoon(){
  if(ocean.nightVis <= 0.01f) return;                        // hidden until sun almost gone
  float ph = (float)(oceanDayCount % 30) / 30.0f;            // 0 full .. 0.5 new .. 1 full
  float a  = cosf(2.0f*3.14159f*ph);                          // +1 full .. -1 new (terminator scale)
  bool  waxing = ph > 0.5f;                                   // 0.5..1 grows back toward full
  CRGB col = oceanMoonColor();
  float R = OCEAN_MOON_R;
  float haloR = OCEAN_MOON_R + 6.0f;                          // wide soft glow (like the sun)
  int16_t rad = (int16_t)haloR + 1;
  for(int16_t y=OCEAN_MOON_Y-rad; y<=OCEAN_MOON_Y+rad; y++){
    if(y<0||y>=OCEAN_HORIZON) continue;
    for(int16_t x=OCEAN_MOON_X-rad; x<=OCEAN_MOON_X+rad; x++){
      if(x<0||x>=NUM_COLS) continue;
      float dx=x-OCEAN_MOON_X, dy=y-OCEAN_MOON_Y, d=sqrtf(dx*dx+dy*dy);
      if(d > haloR) continue;
      // Halo FIRST (additive, also under the rim) so the AA disc edge fades
      // moon->glow instead of moon->dark-sky. Wide & soft like the sun's halo.
      float h=(1.0f-(d-OCEAN_MOON_R)/(haloR-OCEAN_MOON_R))*0.6f*ocean.nightVis;
      if(h>0){ if(h>1) h=1; leds[XY(x,y)] += CRGB((uint8_t)(col.r*h),(uint8_t)(col.g*h),(uint8_t)(col.b*h)); }
      float edge = OCEAN_MOON_R + 0.5f - d;                   // silhouette coverage (AA)
      if(edge > 0.0f){
        if(edge > 1.0f) edge = 1.0f;
        float nx=dx/R, ny=dy/R;
        float tx = a * sqrtf(1.0f - ny*ny);                  // terminator x at this row
        bool lit = waxing ? (nx >= -tx) : (nx <= tx);        // lit side of the terminator
        // Paint only the LIT side; dark side keeps sky+halo behind it.
        if(lit) leds[XY(x,y)] = oceanLerpRGB(leds[XY(x,y)], col, ocean.nightVis*edge);
      }
    }
  }
}

#define OCEAN_NUM_STARS 15
struct OceanStar { uint8_t x, y, ph; };
static OceanStar oceanStars[OCEAN_NUM_STARS];
static uint8_t oceanStarCount = 0;
static bool oceanStarsInit = false;
static void oceanInitStars(){
  oceanStarCount = 0;
  uint16_t guard = 0;
  while(oceanStarCount < OCEAN_NUM_STARS && guard++ < 3000){
    uint8_t sx = random8(NUM_COLS);
    uint8_t sy = random8(OCEAN_HORIZON-6);        // upper sky only
    bool ok = true;                                // reject if touching another star (8-neighbour)
    for(uint8_t i=0;i<oceanStarCount;i++){
      int16_t ddx = (int16_t)sx - oceanStars[i].x;
      int16_t ddy = (int16_t)sy - oceanStars[i].y;
      if(ddx>=-1 && ddx<=1 && ddy>=-1 && ddy<=1){ ok=false; break; }
    }
    if(!ok) continue;
    oceanStars[oceanStarCount].x = sx;
    oceanStars[oceanStarCount].y = sy;
    oceanStars[oceanStarCount].ph = random8();
    oceanStarCount++;
  }
  oceanStarsInit = true;
}
static void oceanDrawStars(){
  if(ocean.nightVis <= 0.01f) return;             // hidden until sun almost gone
  if(!oceanStarsInit) oceanInitStars();
  uint16_t t = millis();
  for(uint8_t i=0;i<oceanStarCount;i++){
    OceanStar &s = oceanStars[i];
    // Twinkle: wide brightness swing (dims most of the way then flares) so it
    // reads even under the viewer's brightness gain. Per-star speed via ph.
    uint8_t tw = sin8((uint8_t)(s.ph*3) + (uint16_t)(t/2));
    float a = ocean.nightVis * (0.75f + 0.25f*(tw/255.0f));
    if(a>1) a=1;
    leds[XY(s.x,s.y)] += CRGB((uint8_t)(200*a),(uint8_t)(210*a),(uint8_t)(235*a));
  }
}

static CRGB oceanSunColor();   // defined with the sun layer below

// additive shimmer column on the sea directly below a sky object at column cx.
static void oceanReflect(uint8_t cx, CRGB col, float strength){
  if(strength <= 0.0f) return;
  uint16_t t = millis();
  for(uint8_t y=OCEAN_HORIZON; y<NUM_ROWS; y++){
    float depth = (float)(y-OCEAN_HORIZON)/(float)(NUM_ROWS-OCEAN_HORIZON);
    float fade  = (1.0f - depth) * strength;                 // fades downward
    uint8_t wob = sin8(y*30 + t/26);                         // vertical wobble (slow)
    float width = 1.5f + depth*3.0f;                         // widens with depth
    for(int16_t x=cx-(int16_t)width; x<=cx+(int16_t)width; x++){
      if(x<0||x>=NUM_COLS) continue;
      float dxn = 1.0f - fabsf((float)(x-(int)cx))/(width+0.5f);
      float a = fade * dxn * (0.5f + wob/512.0f);
      if(a<=0) continue; if(a>1) a=1;
      leds[XY(x,y)] += CRGB((uint8_t)(col.r*a),(uint8_t)(col.g*a),(uint8_t)(col.b*a));
    }
  }
}

static void oceanDrawReflections(){
  if(ocean.sunUp){
    float lowness = 1.0f - oceanSmooth(ocean.alt);          // strongest when low
    oceanReflect(OCEAN_SUN_X, oceanSunColor(), 0.25f + lowness*0.75f);
    // Horizontal sun streak: left+right along the horizon onto the sea, only
    // briefly when the sun is very low (sunrise emerging / before it sets).
    if(lowness > 0.55f){
      float beam = (lowness - 0.55f) / 0.45f;               // 0..1 in the low window
      CRGB col = oceanSunColor();
      uint16_t tt = millis();
      for(int16_t yy=OCEAN_HORIZON-1; yy<=OCEAN_HORIZON+3 && yy<NUM_ROWS; yy++){
        if(yy<0) continue;
        float vrow = (yy<=OCEAN_HORIZON) ? 1.0f : (1.0f-(yy-OCEAN_HORIZON)/3.5f);
        uint8_t shimmer = sin8(yy*40 + tt/26);               // gentle wobble on the water
        for(int16_t x=0;x<NUM_COLS;x++){
          float dxr = fabsf((float)x-OCEAN_SUN_X)/(float)NUM_COLS;
          float a = beam * (1.0f-dxr) * vrow * (0.45f + shimmer/600.0f);
          if(a<=0) continue; if(a>1) a=1;
          leds[XY(x,yy)] += CRGB((uint8_t)(col.r*a),(uint8_t)(col.g*a),(uint8_t)(col.b*a));
        }
      }
    }
  }
  if(ocean.nightVis > 0.01f){
    oceanReflect(OCEAN_MOON_X, oceanMoonColor(), 0.30f * ocean.nightVis);
  }
}

static void oceanDrawOcean(){
  uint16_t t = millis();
  // Waves pulse: the scroll phase surges and eases (~16s period) instead of a
  // constant crawl. Base rate kept forward-only so it never reverses.
  uint16_t wsurge = (uint16_t)(t/26 + 28.0f*sinf(t/2400.0f));
  // base sea colour: a mirror of the horizon sky. Brightness tracks day/night so
  // the day sea is a clear lit blue (not near-black); darkened only at night.
  CRGB base = ocean.skyHorizon;
  uint8_t baseScale = 130 + (uint8_t)((1.0f - ocean.nf) * 95.0f);  // night 130 .. day ~225
  base.nscale8_video(baseScale);
  for(uint8_t y=OCEAN_HORIZON; y<NUM_ROWS; y++){
    // depth 0 at horizon -> 1 at bottom; deeper = a touch darker
    float depth = (float)(y-OCEAN_HORIZON)/(float)(NUM_ROWS-1-OCEAN_HORIZON);
    for(uint8_t x=0;x<NUM_COLS;x++){
      // slow horizontal wave bands
      uint8_t w = sin8(x*6 + y*10 + wsurge);     // pulsing wave bands
      CRGB c = base;
      c.nscale8_video(235 - (uint8_t)(depth*30));
      // wave brightness ripple +/-
      int16_t lift = ((int16_t)w - 128) / 6;     // about +/-21
      c.r = qadd8(c.r, lift>0?lift:0); c.r = qsub8(c.r, lift<0?-lift:0);
      c.g = qadd8(c.g, lift>0?lift:0); c.g = qsub8(c.g, lift<0?-lift:0);
      c.b = qadd8(c.b, lift>0?lift:0); c.b = qsub8(c.b, lift<0?-lift:0);
      leds[XY(x,y)] = c;
      // Coherent foam: on the brightest part of a wave crest, gated by a slowly
      // drifting noise field so it forms moving patches (not per-frame flicker).
      if(w > 236){
        uint8_t fn = inoise8(x*22 + t/20, y*40);
        if(fn > 150){
          uint8_t f = (uint8_t)((w-236) * 3);    // crest height -> foam strength
          leds[XY(x,y)] += CRGB(f, f, f);
        }
      }
    }
  }
}

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
  // At alt 0 the disc centre sits fully below the waterline (whole disc hidden);
  // rising altitude lifts it up through the horizon so it edges in/out instead
  // of popping as a half-circle. The ocean layer (drawn after) clips the part
  // still below the waterline.
  float syLow = OCEAN_HORIZON + OCEAN_SUN_R + 2.0f;                    // fully submerged
  float sy = syLow - ocean.alt * (syLow - OCEAN_PEAK_Y);              // disc centre row
  float sx = OCEAN_SUN_X;
  CRGB col = oceanSunColor();
  float lowness = 1.0f - oceanSmooth(ocean.alt);     // 1 at horizon, 0 at peak
  float haloR = OCEAN_SUN_R + 6.0f + lowness*8.0f;

  // Halo first (warm), THEN the anti-aliased disc on top. Warming the rim before
  // the disc blend means the soft edge fades sun->warm, not sun->blue-sky (which
  // gave a blue fringe against the daytime sky). Sub-pixel edge = smooth glide.
  for(int16_t y=0; y<OCEAN_HORIZON; y++){
    for(int16_t x=0; x<NUM_COLS; x++){
      float dx=x-sx, dy=y-sy, d=sqrtf(dx*dx+dy*dy);
      if(d < haloR){                                        // warm halo (also under the rim)
        oceanAddPix(x,y,col,(1.0f-(d-OCEAN_SUN_R)/(haloR-OCEAN_SUN_R))*0.7f);
      }
      float disc = OCEAN_SUN_R + 0.5f - d;                 // >=1 interior, fades 1->0 at edge
      if(disc > 0.0f){
        if(disc > 1.0f) disc = 1.0f;
        leds[XY(x,y)] = oceanLerpRGB(leds[XY(x,y)], col, disc);
      }
    }
  }
  // (Horizontal sunset/sunrise streak is drawn in oceanDrawReflections, after
  // the ocean, so it lands on the sea near the horizon.)
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
  // moon/stars hidden until the sun's lower edge reaches the water (nf~0.82),
  // then ramp up fast; mirror on sunrise so they vanish as the sun lifts.
  ocean.nightVis = oceanSmooth((ocean.nf - 0.82f) / 0.18f);
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
