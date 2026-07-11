#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Fireworks display (effect id 28). Spec:
//   docs/superpowers/specs/2026-07-11-fireworks-display-design.md
// Choreographed ~75 s looping show: shells rise with comet tails, pause dark
// at apex, then break into one of nine shell types. Two-buffer rendering:
// persistent trail buffer for trailed particles, crisp fresh draw for the
// rest, over a lifted navy sky. 60x60, all writes via leds[XY(x,y)].
// ============================================================================

// ---- pools & physics ----
#define FW_MAX_STARS   160
#define FW_MAX_SHELLS  12
#define FW_MAX_PEND    16
#define FW_MAX_SALUTES 4
#define FW_MAX_BURSTS  6
#define FW_G           10.0f    // star gravity px/s^2
#define FW_SHELL_G     45.0f    // shell deceleration px/s^2 (rise ~1.0-1.5 s)
#define FW_DRAG_50MS   0.92f    // drag factor per 50 ms
#define FW_GROUND_Y    59.0f
#define FW_TWO_PI      6.2831853f
#define FW_MAX_LEAD_MS 1800     // max launch lead scheduled ahead of a break

// ---- show programme (ms into the show) ----
#define FW_T_OPENER_END  6000
#define FW_T_VOLLEYS_END 21000
#define FW_T_GOLD_END    35000
#define FW_T_RHYTHM_END  50000
#define FW_T_LULL_END    55000
#define FW_T_FINALE_END  69000
#define FW_SHOW_LEN      75000

enum { FW_PEONY=0, FW_CHRYS=1, FW_DAHLIA=2, FW_WILLOW=3, FW_PALM=4,
       FW_CROSSETTE=5, FW_RING=6, FW_STROBE=7, FW_SALUTE=8 };
enum { FW_PH_OPENER=0, FW_PH_VOLLEYS, FW_PH_GOLD, FW_PH_RHYTHM,
       FW_PH_LULL, FW_PH_FINALE, FW_PH_STOP };

#define FW_SF_STROBE 0x01   // per-star blink
#define FW_SF_FAT    0x02   // stamp one neighbour pixel too (fat comet)

struct FwStar {
  float    x, y, vx, vy;
  float    age, life;       // seconds; life<=0 => free slot
  CRGB     col;             // head colour
  CRGB     tcol;            // trail stamp colour (used when trail>0)
  uint8_t  trail;           // 0 untrailed, else stamp strength 0..255
  uint8_t  flags;
  float    gmul;            // gravity multiplier
  uint16_t splitMs;         // crossette: pop after this age (0 = never)
  uint16_t strobePer;       // strobe period ms (125-333 => 3-8 Hz)
  uint16_t strobeOff;       // strobe phase offset ms
};

struct FwShell {
  bool     active;
  uint8_t  state;           // 0 rising, 1 apex pause (dark)
  float    x, y, vx, vy;    // vx: slight random launch-angle tilt off vertical
  float    burstY;
  uint8_t  type;
  CRGB     col, col2;
  uint16_t pauseMs;
  int32_t  pauseEnd;        // show-ms when the dark apex pause ends
};

struct FwPending {          // scheduled launch (break time worked backwards)
  bool    active;
  int32_t launchAt;         // show-ms
  float   x, burstY, vy0;
  uint8_t type;
  CRGB    col, col2;
  uint16_t pauseMs;
};

struct FwSalute { bool active; float x, y; int32_t until; };
struct FwBurstRec { float x; int32_t until; };   // for concurrency/spacing
struct FwFountain { bool active; float x; int32_t until; };
#define FW_MAX_FOUNTAINS 2
#define FW_N_FOUNT_CUES  3

static FwStar     fwStars[FW_MAX_STARS];
static FwShell    fwShells[FW_MAX_SHELLS];
static FwPending  fwPend[FW_MAX_PEND];
static FwSalute   fwSalutes[FW_MAX_SALUTES];
static FwBurstRec fwBursts[FW_MAX_BURSTS];
static FwFountain fwFounts[FW_MAX_FOUNTAINS];
static CRGB       fwTrail[NUM_LEDS];      // persistent trail buffer

// ground fountains: settle the audience at show open, bridge phase seams
static const int32_t FW_FOUNT_CUES[FW_N_FOUNT_CUES][2] =
  {{0, 3000}, {6200, 2800}, {21200, 3300}};   // {start ms, duration ms}

static bool     fwInitDone   = false;
static uint32_t fwLastMs     = 0;
static uint32_t fwShowStart  = 0;
static int8_t   fwPhase      = -1;
static int32_t  fwNextA      = 0;   // per-phase event cursor A (mines/comets)
static int32_t  fwNextB      = 0;   // per-phase event cursor B (breaks)
static uint8_t  fwCntA       = 0;
static uint8_t  fwCntB       = 0;
static bool     fwRingDone   = false;
static bool     fwLullFired  = false;
static uint8_t  fwFountIdx   = 0;   // next fountain cue
static uint32_t fwRng        = 0xC0FFEE21u;

// ---- chemistry palette ----
static const CRGB FW_C_RED    = CRGB(255, 28, 12);   // strontium
static const CRGB FW_C_GREEN  = CRGB( 20, 255, 32);  // barium
static const CRGB FW_C_GOLD   = CRGB(255, 150, 24);  // sodium
static const CRGB FW_C_WHITE  = CRGB(255, 240, 216); // magnesium
static const CRGB FW_C_SILVER = CRGB(196, 208, 228);
static const CRGB FW_C_PURPLE = CRGB(184, 40, 255);  // rare accent
static const CRGB FW_C_BLUE   = CRGB( 48, 88, 255);  // rare accent (reads badly)
static const CRGB FW_C_EMBER  = CRGB( 60, 20, 0);    // end-of-life shift target
static const CRGB FW_C_BG     = CRGB(0x14, 0x1C, 0x34);  // survives global scaling

// nominal star life per type (ms) — used for burst-occupancy bookkeeping
static const uint16_t FW_LIFE_MS[9] =
  {1000, 1500, 1800, 4500, 2000, 1300, 1000, 3000, 200};

static inline uint32_t fwRand() {
  fwRng ^= fwRng << 13; fwRng ^= fwRng >> 17; fwRng ^= fwRng << 5; return fwRng;
}
static inline float fwRandF() { return (float)(fwRand() & 0xFFFF) / 65536.0f; }
static inline float fwJit()   { return 1.0f + (fwRandF() - 0.5f) * 0.24f; }  // +/-12%
static inline int   fwRandI(int lo, int hi) { return lo + (int)(fwRand() % (uint32_t)(hi - lo + 1)); }

static inline void fwStamp(int x, int y, const CRGB& c) {
  if (x >= 0 && x < NUM_COLS && y >= 0 && y < NUM_ROWS)
    fwTrail[XY((uint8_t)x, (uint8_t)y)] += c;
}
static inline void fwPlot(int x, int y, const CRGB& c) {
  if (x >= 0 && x < NUM_COLS && y >= 0 && y < NUM_ROWS)
    leds[XY((uint8_t)x, (uint8_t)y)] += c;
}

// full brightness for the first 30% of life, then quadratic fade
// b=(1-u)^2 + ember colour shift in the last 20%
static CRGB fwStarColour(const FwStar& s, const CRGB& base) {
  float t = s.age / s.life; if (t > 1.0f) t = 1.0f;
  float b = 1.0f;
  if (t > 0.3f) { float u = (t - 0.3f) / 0.7f; b = (1.0f - u) * (1.0f - u); }
  CRGB c = base;
  if (t > 0.8f) c = blend(c, FW_C_EMBER, (uint8_t)((t - 0.8f) * 5.0f * 255.0f));
  c.nscale8_video((uint8_t)(b * 255.0f));
  return c;
}

static void fwSpawnStar(float x, float y, float vx, float vy, CRGB col, CRGB tcol,
                        float life, uint8_t trail, uint8_t flags, float gmul,
                        uint16_t splitMs) {
  for (int i = 0; i < FW_MAX_STARS; i++) {
    FwStar& s = fwStars[i];
    if (s.life > 0.0f) continue;
    s.x = x; s.y = y; s.vx = vx; s.vy = vy;
    s.age = 0.0f; s.life = life;
    s.col = col; s.tcol = tcol;
    s.trail = trail; s.flags = flags; s.gmul = gmul; s.splitMs = splitMs;
    s.strobePer = (uint16_t)fwRandI(125, 333);      // 3-8 Hz
    s.strobeOff = (uint16_t)(fwRand() % s.strobePer);
    return;
  }
}

static void fwRecordBurst(float x, int32_t showMs, uint16_t lifeMs) {
  int slot = 0; int32_t oldest = 0x7FFFFFFF;
  for (int i = 0; i < FW_MAX_BURSTS; i++) {
    if (fwBursts[i].until <= showMs) { slot = i; break; }
    if (fwBursts[i].until < oldest) { oldest = fwBursts[i].until; slot = i; }
  }
  fwBursts[slot].x = x; fwBursts[slot].until = showMs + (int32_t)lifeMs;
}

// mines: fan of 8 short tailed stars from the bottom edge, no rise
static void fwMine(float x, CRGB col) {
  for (int i = 0; i < 8; i++) {
    float ang = -FW_TWO_PI / 4.0f + ((float)i / 7.0f - 0.5f) * 1.2f;
    float v = (38.0f + fwRandF() * 14.0f);
    fwSpawnStar(x, FW_GROUND_Y, cosf(ang) * v, sinf(ang) * v,
                col, col, 1.0f * fwJit(), 210, 0, 1.0f, 0);
  }
}

// comet: rises with a fat tail and fades, no burst
static void fwComet(float x, CRGB col) {
  fwSpawnStar(x, FW_GROUND_Y, (fwRandF() - 0.5f) * 6.0f, -(46.0f + fwRandF() * 12.0f),
              col, col, 1.2f * fwJit(), 255, FW_SF_FAT, 2.5f, 0);
}

// fountain: continuous gold spark spray from a fixed bottom-edge point
static void fwUpdateFountains(int32_t showMs) {
  for (int i = 0; i < FW_MAX_FOUNTAINS; i++) {
    FwFountain& f = fwFounts[i];
    if (!f.active) continue;
    if (showMs >= f.until) { f.active = false; continue; }
    for (int k = 0; k < 2; k++) {              // 2 sparks per frame (~40/s)
      CRGB c = (fwRand() % 100 < 20) ? FW_C_WHITE : FW_C_GOLD;
      fwSpawnStar(f.x + (fwRandF() - 0.5f) * 1.5f, FW_GROUND_Y,
                  (fwRandF() - 0.5f) * 12.0f, -(18.0f + fwRandF() * 16.0f),
                  c, c, 0.4f + fwRandF() * 0.4f, 140, 0, 1.0f, 0);
    }
  }
}

static void fwBurst(float x, float y, uint8_t type, CRGB col, CRGB col2, int32_t showMs) {
  float base = fwRandF() * FW_TWO_PI;
  switch (type) {
    case FW_PEONY:       // 18 uniform radial, single colour, crisp
      for (int i = 0; i < 18; i++) {
        float a = base + FW_TWO_PI * i / 18.0f, v = 40.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, col, col, 1.0f * fwJit(), 0, 0, 1.0f, 0);
      }
      break;
    case FW_CHRYS:       // peony + silver-tinged persistent trails
      for (int i = 0; i < 18; i++) {
        float a = base + FW_TWO_PI * i / 18.0f, v = 40.0f * fwJit();
        CRGB tc = blend(col, FW_C_SILVER, 150);
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, col, tc, 1.5f * fwJit(), 210, 0, 1.0f, 0);
      }
      break;
    case FW_DAHLIA:      // few big fast stars, largest radius
      for (int i = 0; i < 9; i++) {
        float a = base + FW_TWO_PI * i / 9.0f, v = 44.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, col, col, 1.8f * fwJit(), 0, 0, 1.0f, 0);
      }
      break;
    case FW_WILLOW:      // gold, low v0, long hanging streamers
      for (int i = 0; i < 14; i++) {
        float a = base + FW_TWO_PI * i / 14.0f, v = 19.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, FW_C_GOLD, FW_C_GOLD,
                    4.5f * fwJit(), 255, 0, 1.0f, 0);
      }
      break;
    case FW_PALM:        // 6 thick comet arms biased up/outward (fronds)
      for (int i = 0; i < 6; i++) {
        float a = base + FW_TWO_PI * i / 6.0f, v = 26.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v - 12.0f, col, col,
                    2.0f * fwJit(), 255, FW_SF_FAT, 1.0f, 0);
      }
      break;
    case FW_CROSSETTE:   // 5 comets that each split into 3-4 small stars
      for (int i = 0; i < 5; i++) {
        float a = base + FW_TWO_PI * i / 5.0f, v = 28.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, col, col,
                    1.3f, 210, 0, 1.0f, (uint16_t)fwRandI(550, 700));
      }
      break;
    case FW_RING:        // uniform expanding circle (no jitter) + pistil
      for (int i = 0; i < 16; i++) {
        float a = base + FW_TWO_PI * i / 16.0f;
        fwSpawnStar(x, y, cosf(a) * 38.0f, sinf(a) * 38.0f, col, col, 1.0f, 0, 0, 1.0f, 0);
      }
      for (int i = 0; i < 5; i++) {
        float a = fwRandF() * FW_TWO_PI, v = 7.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, col2, col2, 0.9f * fwJit(), 0, 0, 1.0f, 0);
      }
      break;
    case FW_STROBE:      // white, slow drift, per-star 3-8 Hz blink
      for (int i = 0; i < 20; i++) {
        float a = base + FW_TWO_PI * i / 20.0f, v = 13.0f * fwJit();
        fwSpawnStar(x, y, cosf(a) * v, sinf(a) * v, FW_C_WHITE, FW_C_WHITE,
                    3.0f * fwJit(), 0, FW_SF_STROBE, 0.25f, 0);
      }
      break;
    case FW_SALUTE:      // 7 px white flash disc, instant black after
      for (int i = 0; i < FW_MAX_SALUTES; i++) {
        if (fwSalutes[i].active) continue;
        fwSalutes[i].active = true; fwSalutes[i].x = x; fwSalutes[i].y = y;
        fwSalutes[i].until = showMs + 130;
        break;
      }
      break;
  }
  // record at 60% of nominal life: long pieces (willow/strobe) must not starve
  // the air-count gate once their stars are already dim
  fwRecordBurst(x, showMs, (uint16_t)((FW_LIFE_MS[type] * 3) / 5));
}

// ---- scheduler helpers (break time cued, launch worked backwards) ----

static int fwCountAir(int32_t showMs) {
  int n = 0;
  for (int i = 0; i < FW_MAX_SHELLS; i++)  if (fwShells[i].active) n++;
  for (int i = 0; i < FW_MAX_PEND; i++)    if (fwPend[i].active) n++;
  for (int i = 0; i < FW_MAX_BURSTS; i++)  if (fwBursts[i].until > showMs) n++;
  return n;
}

static bool fwXFree(float x, int32_t showMs) {   // centres >= 20 px apart
  for (int i = 0; i < FW_MAX_SHELLS; i++)
    if (fwShells[i].active && fabsf(fwShells[i].x - x) < 20.0f) return false;
  for (int i = 0; i < FW_MAX_PEND; i++)
    if (fwPend[i].active && fabsf(fwPend[i].x - x) < 20.0f) return false;
  for (int i = 0; i < FW_MAX_BURSTS; i++)
    if (fwBursts[i].until > showMs && fabsf(fwBursts[i].x - x) < 20.0f) return false;
  return true;
}

static float fwPickX(int lo, int hi, int32_t showMs) {
  float x = (float)fwRandI(lo, hi);
  for (int t = 0; t < 8; t++) {
    x = (float)fwRandI(lo, hi);
    if (fwXFree(x, showMs)) break;
  }
  return x;
}

// queue a shell so its BREAK lands at breakAt (launch time back-computed
// from the known rise duration + the dark apex pause)
static void fwQueueShell(int32_t breakAt, float x, uint8_t type, float burstY,
                         CRGB col, CRGB col2) {
  for (int i = 0; i < FW_MAX_PEND; i++) {
    FwPending& p = fwPend[i];
    if (p.active) continue;
    p.active = true;
    p.x = x; p.burstY = burstY; p.type = type; p.col = col; p.col2 = col2;
    p.pauseMs = (uint16_t)fwRandI(100, 200);
    p.vy0 = -sqrtf(2.0f * FW_SHELL_G * (FW_GROUND_Y - burstY));
    int32_t riseMs = (int32_t)(1000.0f * (-p.vy0) / FW_SHELL_G);
    p.launchAt = breakAt - riseMs - (int32_t)p.pauseMs;
    return;
  }
}

static void fwPopPending(int32_t showMs) {
  for (int i = 0; i < FW_MAX_PEND; i++) {
    FwPending& p = fwPend[i];
    if (!p.active || showMs < p.launchAt) continue;
    for (int j = 0; j < FW_MAX_SHELLS; j++) {
      FwShell& sh = fwShells[j];
      if (sh.active) continue;
      sh.active = true; sh.state = 0;
      sh.x = p.x; sh.y = FW_GROUND_Y; sh.vy = p.vy0;
      sh.vx = -p.vy0 * (fwRandF() - 0.5f) * 0.10f;   // +/- ~3 deg off vertical
      sh.burstY = p.burstY; sh.type = p.type;
      sh.col = p.col; sh.col2 = p.col2;
      sh.pauseMs = p.pauseMs; sh.pauseEnd = 0;
      break;
    }
    p.active = false;
  }
}

static CRGB fwVolleyColour(bool noBlue = false) {   // saturated singles; purple/blue rare
  uint32_t r = fwRand() % 100;
  if (r < 30) return FW_C_RED;
  if (r < 60) return FW_C_GREEN;
  if (r < 84) return FW_C_GOLD;
  if (r < 94) return FW_C_PURPLE;
  return noBlue ? FW_C_GOLD : FW_C_BLUE;
}

static void fwSchedule(int32_t showMs) {
  int8_t ph = FW_PH_STOP;
  if      (showMs < FW_T_OPENER_END)  ph = FW_PH_OPENER;
  else if (showMs < FW_T_VOLLEYS_END) ph = FW_PH_VOLLEYS;
  else if (showMs < FW_T_GOLD_END)    ph = FW_PH_GOLD;
  else if (showMs < FW_T_RHYTHM_END)  ph = FW_PH_RHYTHM;
  else if (showMs < FW_T_LULL_END)    ph = FW_PH_LULL;
  else if (showMs < FW_T_FINALE_END)  ph = FW_PH_FINALE;

  // ground fountains fire on their cue regardless of phase
  if (fwFountIdx < FW_N_FOUNT_CUES && showMs >= FW_FOUNT_CUES[fwFountIdx][0]) {
    for (int i = 0; i < FW_MAX_FOUNTAINS; i++) {
      if (fwFounts[i].active) continue;
      fwFounts[i].active = true;
      fwFounts[i].x = (float)fwRandI(18, 41);
      fwFounts[i].until = showMs + FW_FOUNT_CUES[fwFountIdx][1];
      break;
    }
    fwFountIdx++;
  }

  if (ph != fwPhase) {                        // phase entry
    fwPhase = ph; fwCntA = 0; fwCntB = 0;
    switch (ph) {
      case FW_PH_OPENER:  fwNextA = 200;   fwNextB = 1800;  break;
      case FW_PH_VOLLEYS: fwNextB = FW_T_OPENER_END + 1500; break;
      case FW_PH_GOLD:    fwNextB = FW_T_VOLLEYS_END + 1000;
                          fwNextA = FW_T_VOLLEYS_END + 2200; break;
      case FW_PH_RHYTHM:  fwNextB = FW_T_GOLD_END + 500;    break;
      case FW_PH_LULL:    fwLullFired = false;              break;
      case FW_PH_FINALE:  fwNextB = FW_T_LULL_END + 800;    break;
      default: break;
    }
  }

  switch (ph) {
    case FW_PH_OPENER: {
      // first 2 s: 3-4 fast comets + mines straight off the ground
      while (fwCntA < 4 && showMs >= fwNextA) {
        float x = (float)fwRandI(8, 51);
        if (fwCntA & 1) fwComet(x, FW_C_GOLD); else fwMine(x, FW_C_SILVER);
        fwNextA += 550; fwCntA++;
      }
      // then red/gold/silver peonies & dahlias, 1 per 1.5 s
      if (fwNextB < FW_T_OPENER_END + 500 && showMs >= fwNextB - FW_MAX_LEAD_MS) {
        if (fwCountAir(showMs) >= 2) { fwNextB += 150; break; }
        static const CRGB oc[3] = {FW_C_RED, FW_C_GOLD, FW_C_SILVER};
        uint8_t ty = (fwCntB & 1) ? FW_DAHLIA : FW_PEONY;
        float by = (float)((ty == FW_DAHLIA) ? fwRandI(10, 16) : fwRandI(12, 20));
        fwQueueShell(fwNextB, fwPickX(10, 49, showMs), ty, by, oc[fwCntB % 3], oc[fwCntB % 3]);
        fwNextB += 1100; fwCntB++;
      }
      break;
    }
    case FW_PH_VOLLEYS: {
      if (fwNextB >= FW_T_VOLLEYS_END - 500 || showMs < fwNextB - FW_MAX_LEAD_MS) break;
      int air = fwCountAir(showMs);
      if (air >= 2) { fwNextB += 150; break; }
      bool mirrored = (air == 0) && (fwRand() % 100 < 25);
      if (mirrored) {                          // complementary-pair volley
        bool rg = (fwRand() & 1);
        CRGB a = rg ? FW_C_RED : FW_C_PURPLE, b = rg ? FW_C_GREEN : FW_C_GOLD;
        uint8_t ty = (fwRand() & 1) ? FW_PEONY : FW_CHRYS;
        float by = (float)fwRandI(12, 20);
        float x = (float)fwRandI(10, 24);
        fwQueueShell(fwNextB, x, ty, by, a, a);
        fwQueueShell(fwNextB, 59.0f - x, ty, by, b, b);
      } else if (!fwRingDone && showMs > 13000) {   // exactly one ring mid-phase
        fwRingDone = true;
        CRGB c = (fwRand() & 1) ? FW_C_GREEN : FW_C_PURPLE;
        CRGB p = (c.g > 128) ? FW_C_RED : FW_C_GOLD;  // contrasting pistil
        fwQueueShell(fwNextB, fwPickX(14, 45, showMs), FW_RING, (float)fwRandI(14, 20), c, p);
      } else {
        uint8_t ty = (fwRand() & 1) ? FW_PEONY : FW_CHRYS;
        CRGB c = fwVolleyColour();
        fwQueueShell(fwNextB, fwPickX(8, 51, showMs), ty, (float)fwRandI(12, 20), c, c);
      }
      fwNextB += fwRandI(2000, 3000);
      break;
    }
    case FW_PH_GOLD: {
      // ground comets as punctuation between the big gold pieces
      if (showMs >= fwNextA && fwNextA < FW_T_GOLD_END - 1500) {
        fwComet((float)fwRandI(8, 51), FW_C_GOLD);
        fwNextA += fwRandI(3000, 4500);
      }
      if (fwNextB >= FW_T_GOLD_END - 1000 || showMs < fwNextB - FW_MAX_LEAD_MS) break;
      if (fwCountAir(showMs) >= 2) { fwNextB += 150; break; }   // at most two alight, staggered
      if (fwCntB & 1) {                        // palm, gold or silver
        CRGB c = (fwRand() % 100 < 40) ? FW_C_SILVER : FW_C_GOLD;
        fwQueueShell(fwNextB, fwPickX(10, 49, showMs), FW_PALM, (float)fwRandI(16, 22), c, c);
      } else {                                 // willow, solo centre-sky
        fwQueueShell(fwNextB, fwPickX(20, 39, showMs), FW_WILLOW, (float)fwRandI(14, 18),
                     FW_C_GOLD, FW_C_GOLD);
      }
      fwNextB += fwRandI(4000, 5000); fwCntB++;
      break;
    }
    case FW_PH_RHYTHM: {
      // metronomic 1 per 1.7 s, alternating low mine / high break
      if (fwNextB >= FW_T_RHYTHM_END - 300) break;
      if ((fwCntB & 1) == 0) {                 // low mine, fired on the beat
        if (showMs >= fwNextB) {
          fwMine((float)fwRandI(8, 51), (fwRand() & 1) ? FW_C_GOLD : FW_C_GREEN);
          fwNextB += 1700; fwCntB++;
        }
      } else if (showMs >= fwNextB - FW_MAX_LEAD_MS) {  // high break, cued ahead
        if (fwCountAir(showMs) < 2) {
          static const uint8_t seq[4] = {FW_CROSSETTE, FW_RING, FW_STROBE, FW_DAHLIA};
          uint8_t ty = seq[(fwCntB >> 1) & 3];
          CRGB c = (ty == FW_STROBE) ? FW_C_WHITE
                 : (ty == FW_CROSSETTE) ? ((fwRand() & 1) ? FW_C_GOLD : FW_C_RED)
                 : fwVolleyColour();
          CRGB p = (ty == FW_RING) ? ((c.g > 128) ? FW_C_RED : FW_C_GOLD) : c;
          fwQueueShell(fwNextB, fwPickX(8, 51, showMs), ty,
                       (float)((ty == FW_DAHLIA) ? fwRandI(10, 16) : fwRandI(14, 24)), c, p);
          fwNextB += 1700; fwCntB++;           // advance only when the beat fired
        } else if (showMs - fwNextB > 1700) {  // a full beat behind: re-anchor
          fwNextB += 1700;
        }
      }
      break;
    }
    case FW_PH_LULL: {
      // 2.5 s of black sky, then one slow gold willow before the finale
      if (!fwLullFired && showMs >= FW_T_RHYTHM_END + 2200) {
        fwLullFired = true;
        fwQueueShell(FW_T_RHYTHM_END + 4000, (float)fwRandI(24, 35), FW_WILLOW,
                     15.0f, FW_C_GOLD, FW_C_GOLD);
      }
      break;
    }
    case FW_PH_FINALE: {
      if (fwNextB >= FW_T_FINALE_END - 400 || showMs < fwNextB - FW_MAX_LEAD_MS) break;
      if (fwNextB >= FW_T_FINALE_END - 2000) {           // last 2 s: salutes only,
        if (showMs >= fwNextB) {                         // fired direct — no rise,
          for (int i = 0; i < FW_MAX_SALUTES; i++) {     // real salutes are reports
            if (fwSalutes[i].active) continue;
            fwSalutes[i].active = true;
            fwSalutes[i].x = (float)fwRandI(8, 51);
            fwSalutes[i].y = (float)fwRandI(10, 18);
            fwSalutes[i].until = showMs + 130;
            break;
          }
          fwNextB += fwRandI(250, 400);
        }
      } else {                                           // ramp 1/s -> 3/s, cap lifted
        float f = (float)(fwNextB - FW_T_LULL_END) / (float)(FW_T_FINALE_END - FW_T_LULL_END);
        static const uint8_t seq[3] = {FW_DAHLIA, FW_CHRYS, FW_WILLOW};
        uint8_t ty = seq[fwRand() % 3];
        CRGB c;                                          // converge on gold + white
        if (f > 0.4f) c = (fwRand() & 1) ? FW_C_GOLD : FW_C_WHITE;
        else c = fwVolleyColour(true);                   // no blue in the finale
        fwQueueShell(fwNextB, (float)fwRandI(6, 53), ty,
                     (float)((ty == FW_WILLOW) ? fwRandI(13, 18) : fwRandI(10, 18)), c, c);
        fwNextB += (int32_t)(1000.0f - 667.0f * f);
      }
      break;
    }
    default: break;   // FW_PH_STOP: hard cut, nothing launches
  }
}

// ---- simulation ----

static void fwUpdateShells(float dt, int32_t showMs) {
  for (int i = 0; i < FW_MAX_SHELLS; i++) {
    FwShell& sh = fwShells[i];
    if (!sh.active) continue;
    if (sh.state == 0) {                       // rising
      sh.vy += FW_SHELL_G * dt;
      sh.x  += sh.vx * dt;
      sh.y  += sh.vy * dt;
      if (sh.y <= sh.burstY || sh.vy >= -4.0f) {   // apex: go dark, then break
        sh.state = 1;
        sh.pauseEnd = showMs + (int32_t)sh.pauseMs;
      }
    } else if (showMs >= sh.pauseEnd) {
      fwBurst(sh.x, sh.y, sh.type, sh.col, sh.col2, showMs);
      sh.active = false;
    }
  }
}

static void fwUpdateStars(float dt) {
  float drag = powf(FW_DRAG_50MS, dt / 0.05f);
  for (int i = 0; i < FW_MAX_STARS; i++) {
    FwStar& s = fwStars[i];
    if (s.life <= 0.0f) continue;
    s.age += dt;
    if (s.splitMs && s.age * 1000.0f >= (float)s.splitMs) {   // crossette pop
      int n = 3 + (int)(fwRand() & 1);
      CRGB c = blend(s.col, FW_C_WHITE, 90);
      float px = s.x, py = s.y;
      s.life = 0.0f;
      for (int k = 0; k < n; k++) {
        float a = fwRandF() * FW_TWO_PI, v = 15.0f + fwRandF() * 10.0f;
        fwSpawnStar(px, py, cosf(a) * v, sinf(a) * v, c, c, 0.7f * fwJit(), 0, 0, 1.0f, 0);
      }
      continue;
    }
    if (s.age >= s.life) { s.life = 0.0f; continue; }
    s.vx *= drag; s.vy *= drag;
    s.vy += FW_G * s.gmul * dt;
    s.x  += s.vx * dt;
    s.y  += s.vy * dt;
    if (s.y > FW_GROUND_Y + 0.5f || s.x < -2.0f || s.x > NUM_COLS + 1.0f || s.y < -2.0f)
      s.life = 0.0f;
  }
}

static void fwReset(uint32_t now) {
  fwInitDone = true;
  for (int i = 0; i < FW_MAX_STARS; i++)   fwStars[i].life = 0.0f;
  for (int i = 0; i < FW_MAX_SHELLS; i++)  fwShells[i].active = false;
  for (int i = 0; i < FW_MAX_PEND; i++)    fwPend[i].active = false;
  for (int i = 0; i < FW_MAX_SALUTES; i++) fwSalutes[i].active = false;
  for (int i = 0; i < FW_MAX_BURSTS; i++)  { fwBursts[i].x = -100.0f; fwBursts[i].until = 0; }
  for (int i = 0; i < FW_MAX_FOUNTAINS; i++) fwFounts[i].active = false;
  for (int i = 0; i < NUM_LEDS; i++)       fwTrail[i] = CRGB::Black;
  fwPhase = -1; fwRingDone = false; fwLullFired = false; fwFountIdx = 0;
  fwShowStart = now; fwLastMs = now;
  fwRng = 0x9E3779B9u ^ now; if (fwRng == 0) fwRng = 1;
}

// ---- entry ----

void fireworks() {
  uint32_t now = millis();
  if (!fwInitDone) fwReset(now);
  else if (now - fwLastMs > 1000)              // stutter or re-entry: pause the
    fwShowStart += now - fwLastMs;             // show clock, never reset mid-show
  float dt = (float)(now - fwLastMs) * 0.001f;
  if (dt > 0.1f) dt = 0.1f;                                  // clamp to 100 ms
  fwLastMs = now;

  int32_t showMs = (int32_t)(now - fwShowStart);
  if (showMs >= FW_SHOW_LEN) {                               // loop the programme
    fwShowStart = now; showMs = 0;
    fwPhase = -1; fwRingDone = false; fwLullFired = false; fwFountIdx = 0;
    for (int i = 0; i < FW_MAX_PEND; i++)      fwPend[i].active = false;
    for (int i = 0; i < FW_MAX_BURSTS; i++)    fwBursts[i].until = 0;
    for (int i = 0; i < FW_MAX_FOUNTAINS; i++) fwFounts[i].active = false;
  }

  fwSchedule(showMs);
  fwPopPending(showMs);
  fwUpdateFountains(showMs);
  fwUpdateShells(dt, showMs);
  fwUpdateStars(dt);

  // decay the persistent trail buffer (time-based, ~28/255 per 50 ms)
  uint8_t fade = (uint8_t)fminf(255.0f, dt * 560.0f);
  fadeToBlackBy(fwTrail, NUM_LEDS, fade);

  // stamp trailed particles into the trail buffer
  for (int i = 0; i < FW_MAX_STARS; i++) {
    const FwStar& s = fwStars[i];
    if (s.life <= 0.0f || s.trail == 0) continue;
    CRGB tc = fwStarColour(s, s.tcol);
    tc.nscale8(s.trail);
    int px = (int)(s.x + 0.5f), py = (int)(s.y + 0.5f);
    fwStamp(px, py, tc);
    if (s.flags & FW_SF_FAT) fwStamp(px + 1, py, tc);   // fat comet: neighbour too
  }
  for (int i = 0; i < FW_MAX_SHELLS; i++) {             // rising shells trail gold
    const FwShell& sh = fwShells[i];
    if (!sh.active || sh.state != 0) continue;          // dark during apex pause
    fwStamp((int)(sh.x + 0.5f), (int)(sh.y + 0.5f), CRGB(90, 60, 16));
  }

  // compose: lifted navy sky (never pure black), plus trails
  for (int i = 0; i < NUM_LEDS; i++) { leds[i] = FW_C_BG; leds[i] += fwTrail[i]; }

  // untrailed particles drawn fresh — crisp, no persistence
  for (int i = 0; i < FW_MAX_STARS; i++) {
    const FwStar& s = fwStars[i];
    if (s.life <= 0.0f || s.trail != 0) continue;
    if (s.flags & FW_SF_STROBE) {                       // 3-8 Hz per-star blink
      uint32_t ms = (uint32_t)(s.age * 1000.0f) + s.strobeOff;
      if ((ms % s.strobePer) >= (uint32_t)(s.strobePer / 2)) continue;   // 50% duty
    }
    fwPlot((int)(s.x + 0.5f), (int)(s.y + 0.5f), fwStarColour(s, s.col));
  }

  // rising shell heads (bright comet dot over the gold tail)
  for (int i = 0; i < FW_MAX_SHELLS; i++) {
    const FwShell& sh = fwShells[i];
    if (!sh.active || sh.state != 0) continue;
    fwPlot((int)(sh.x + 0.5f), (int)(sh.y + 0.5f), CRGB(255, 214, 140));
  }

  // salute flash discs (2 frames, then instant black)
  for (int i = 0; i < FW_MAX_SALUTES; i++) {
    FwSalute& sa = fwSalutes[i];
    if (!sa.active) continue;
    if (showMs >= sa.until) { sa.active = false; continue; }
    int cx = (int)(sa.x + 0.5f), cy = (int)(sa.y + 0.5f);
    for (int dy = -3; dy <= 3; dy++)
      for (int dx = -3; dx <= 3; dx++)
        if (dx * dx + dy * dy <= 12) fwPlot(cx + dx, cy + dy, CRGB(255, 250, 240));
  }

  FastLED.show();
}
