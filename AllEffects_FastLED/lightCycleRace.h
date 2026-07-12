#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Tron-style light-cycle race. Spec:
//   docs/superpowers/specs/2026-06-29-lightcycle-race-design.md
// Stateful sim: occupancy grid + N cycles + phase machine. 60x60.
// ============================================================================
#define LC_STEP_MS        70      // race step interval
#define LC_FLASH_MS       2000    // winner flash duration
#define LC_FLASH_BLINK_MS 200     // winner pulse period
#define LC_PAUSE_MS       500     // blank pause before next round
#define LC_INTERCEPT_K    6       // how far ahead of the target the AI aims
#define LC_N              8       // number of racers
#define LC_WALL           250     // grid value for border walls (cycles are 1..LC_N)

enum { LC_RACING=0, LC_FLASH=1, LC_PAUSE=2 };

struct LcCycle { int8_t x, y, dx, dy; bool alive; };

static uint8_t  lcGrid[NUM_LEDS];          // 0 empty, 1..LC_N cycle trails, LC_WALL border
static LcCycle  lcCyc[LC_N];
static uint8_t  lcPhase      = LC_PAUSE;
static uint32_t lcPhaseStart = 0;
static uint32_t lcLastStep   = 0;
static uint32_t lcRng        = 0x1234567u;
static uint16_t lcRound      = 0;
static int8_t   lcWinner     = -1;         // -1 none/draw, else cycle index
static bool     lcInit       = false;
static uint32_t lcRoundStart = 0;          // millis at round start (for burst timing)
static uint16_t lcBurstDelay[LC_N];        // ms into the round each cycle fires its one 2s burst

// per-cycle palette: trail / head / additive glow
static const CRGB LC_TRAIL[LC_N] = {CRGB(0xE03030),CRGB(0x3060E0),CRGB(0x30C040),CRGB(0xE0B020),
                                    CRGB(0xE040C0),CRGB(0x20C0D0),CRGB(0xE06020),CRGB(0x9050F0)};
static const CRGB LC_HEAD[LC_N]  = {CRGB(0xFFE0D8),CRGB(0xD8E4FF),CRGB(0xDFFFE0),CRGB(0xFFF4D0),
                                    CRGB(0xFFD8F4),CRGB(0xD8FAFF),CRGB(0xFFE0C8),CRGB(0xECD8FF)};
static const CRGB LC_GLOW[LC_N]  = {CRGB(0x401810),CRGB(0x101840),CRGB(0x103C18),CRGB(0x383010),
                                    CRGB(0x381030),CRGB(0x0E3438),CRGB(0x381808),CRGB(0x281040)};

static inline uint32_t lcRand(){ lcRng^=lcRng<<13; lcRng^=lcRng>>17; lcRng^=lcRng<<5; return lcRng; }
static inline int lcGi(int x,int y){ return y*NUM_COLS + x; }

static void lcClearTrails(){
  for(int i=0;i<NUM_LEDS;i++) if(lcGrid[i]!=LC_WALL) lcGrid[i]=0;
}

static void lcReset(){
  for(int i=0;i<NUM_LEDS;i++) lcGrid[i]=0;
  for(int x=0;x<NUM_COLS;x++){ lcGrid[lcGi(x,0)]=LC_WALL; lcGrid[lcGi(x,NUM_ROWS-1)]=LC_WALL; }
  for(int y=0;y<NUM_ROWS;y++){ lcGrid[lcGi(0,y)]=LC_WALL; lcGrid[lcGi(NUM_COLS-1,y)]=LC_WALL; }
  lcRound++;
  lcRng = 0x9E3779B9u ^ ((uint32_t)lcRound*2654435761u); if(lcRng==0) lcRng=1;
  // 8 racers, 2 per side, starting at the very edge (just inside the wall) facing inward
  static const int8_t LC_START[LC_N][4] = {
    { 1,20, 1, 0}, {58,40,-1, 0}, {20, 1, 0, 1}, {40,58, 0,-1},   // W, E, N, S
    { 1,40, 1, 0}, {58,20,-1, 0}, {40, 1, 0, 1}, {20,58, 0,-1}    // W, E, N, S (offset)
  };
  for(int i=0;i<LC_N;i++){
    lcCyc[i] = { LC_START[i][0], LC_START[i][1], LC_START[i][2], LC_START[i][3], true };
    lcGrid[lcGi(lcCyc[i].x,lcCyc[i].y)] = (uint8_t)(i+1);
    lcBurstDelay[i] = 1500 + (uint16_t)(lcRand()%4000);   // each fires its 2s burst at a random time
  }
  lcRoundStart = millis();
  lcWinner = -1;
}

static inline bool lcBursting(int i){
  uint32_t e = millis() - lcRoundStart;
  return e >= lcBurstDelay[i] && e < (uint32_t)lcBurstDelay[i] + 2000;
}

// choose a move for cycle i (straight / left-turn / right-turn) that stays alive and
// cuts toward a point ahead of its NEAREST living opponent. false => no safe move.
// y is down: right-turn = (dx,dy)->(-dy,dx), left-turn = (dx,dy)->(dy,-dx).
static bool lcChoose(int i, int8_t& outdx, int8_t& outdy){
  const LcCycle& c = lcCyc[i];
  int bj=-1, bd=1<<30;
  for(int j=0;j<LC_N;j++){ if(j==i||!lcCyc[j].alive) continue;
    int dd=abs(lcCyc[j].x-c.x)+abs(lcCyc[j].y-c.y); if(dd<bd){ bd=dd; bj=j; } }
  int ix, iy;
  if(bj<0){ ix=NUM_COLS/2; iy=NUM_ROWS/2; }
  else { ix=lcCyc[bj].x+lcCyc[bj].dx*LC_INTERCEPT_K; iy=lcCyc[bj].y+lcCyc[bj].dy*LC_INTERCEPT_K; }
  if(ix<1)ix=1; if(ix>NUM_COLS-2)ix=NUM_COLS-2;
  if(iy<1)iy=1; if(iy>NUM_ROWS-2)iy=NUM_ROWS-2;
  int8_t cand[3][2] = { {c.dx,c.dy}, {(int8_t)c.dy,(int8_t)-c.dx}, {(int8_t)-c.dy,(int8_t)c.dx} };
  float best=-1e9f; bool found=false;
  for(int k=0;k<3;k++){
    int nx=c.x+cand[k][0], ny=c.y+cand[k][1];
    if(nx<0||nx>=NUM_COLS||ny<0||ny>=NUM_ROWS) continue;
    if(lcGrid[lcGi(nx,ny)]!=0) continue;
    int open=0, ox=nx, oy=ny;                          // empty run straight ahead (anti dead-end)
    for(int s=0;s<8;s++){ ox+=cand[k][0]; oy+=cand[k][1];
      if(ox<0||ox>=NUM_COLS||oy<0||oy>=NUM_ROWS||lcGrid[lcGi(ox,oy)]!=0) break; open++; }
    float score = -2.6f*(float)(abs(nx-ix)+abs(ny-iy)) + open*0.35f + (float)(lcRand()%5)*0.08f;
    if(k==0) score += 0.1f;                            // tiny straight preference
    if(score>best){ best=score; outdx=cand[k][0]; outdy=cand[k][1]; found=true; }
  }
  return found;
}

static void lcStep(){
  int8_t ndx[LC_N], ndy[LC_N];
  bool   ok[LC_N], crash[LC_N];
  int    nx[LC_N], ny[LC_N];
  for(int i=0;i<LC_N;i++){
    ok[i]=false; crash[i]=false;
    if(!lcCyc[i].alive) continue;
    ok[i]=lcChoose(i, ndx[i], ndy[i]);
    if(ok[i]){ nx[i]=lcCyc[i].x+ndx[i]; ny[i]=lcCyc[i].y+ndy[i]; }
    else crash[i]=true;                                // no safe move
  }
  // two cycles aiming at the same cell this step both crash (head-on / cross)
  for(int i=0;i<LC_N;i++) if(lcCyc[i].alive&&ok[i])
    for(int j=i+1;j<LC_N;j++) if(lcCyc[j].alive&&ok[j])
      if(nx[i]==nx[j]&&ny[i]==ny[j]){ crash[i]=true; crash[j]=true; }
  // apply
  for(int i=0;i<LC_N;i++){
    if(!lcCyc[i].alive) continue;
    if(crash[i]) lcCyc[i].alive=false;
    else { lcCyc[i].dx=ndx[i]; lcCyc[i].dy=ndy[i]; lcCyc[i].x=nx[i]; lcCyc[i].y=ny[i];
           lcGrid[lcGi(nx[i],ny[i])]=(uint8_t)(i+1); }
  }
  // burst: a cycle in its 2-second window takes one EXTRA cell this step (~2x speed)
  for(int i=0;i<LC_N;i++){
    if(!lcCyc[i].alive || !lcBursting(i)) continue;
    int8_t edx,edy;
    if(lcChoose(i, edx,edy)){
      int ex=lcCyc[i].x+edx, ey=lcCyc[i].y+edy;
      if(lcGrid[lcGi(ex,ey)]==0){ lcCyc[i].dx=edx; lcCyc[i].dy=edy; lcCyc[i].x=ex; lcCyc[i].y=ey; lcGrid[lcGi(ex,ey)]=(uint8_t)(i+1); }
      else lcCyc[i].alive=false;
    } else lcCyc[i].alive=false;
  }
  // race ends when <=1 cycle remains
  int n=0, last=-1; for(int i=0;i<LC_N;i++) if(lcCyc[i].alive){ n++; last=i; }
  if(n<=1){ lcWinner=(n==1)?(int8_t)last:-1; lcPhase=LC_FLASH; lcPhaseStart=millis(); }
}

static void lcRender(){
  for(int y=0;y<NUM_ROWS;y++) for(int x=0;x<NUM_COLS;x++){
    uint8_t g=lcGrid[lcGi(x,y)];
    CRGB c = CRGB(0x0A0E1A);
    if(g==LC_WALL) c = CRGB(0x2A3040);
    else if(g>=1 && g<=LC_N){
      CRGB base = LC_TRAIL[g-1];
      if(lcPhase==LC_FLASH){
        bool blinkOn = ((millis()/LC_FLASH_BLINK_MS)&1)==0;
        if(lcWinner<0 || (g-1)==lcWinner){ c = blinkOn ? CRGB(0xE8ECF4) : base; }  // winner/draw pulses white
        else { c = base; c.nscale8(70); }                                          // losers dimmed
      } else c = base;
    }
    leds[XY((uint8_t)x,(uint8_t)y)] = c;
  }
  if(lcPhase==LC_RACING){
    const int8_t nb[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<LC_N;i++){
      if(!lcCyc[i].alive) continue;
      bool burst=lcBursting(i);
      CRGB glow = burst ? (LC_GLOW[i] + LC_GLOW[i]) : LC_GLOW[i];   // brighter aura while bursting
      for(int k=0;k<4;k++){ int gx=lcCyc[i].x+nb[k][0], gy=lcCyc[i].y+nb[k][1];
        if(gx>=0&&gx<NUM_COLS&&gy>=0&&gy<NUM_ROWS) leds[XY((uint8_t)gx,(uint8_t)gy)] += glow; }
      leds[XY((uint8_t)lcCyc[i].x,(uint8_t)lcCyc[i].y)] = burst ? CRGB(0xFFFFFF) : LC_HEAD[i];
    }
  }
}

void lightCycleRace(){
  uint32_t t=millis();
  if(!lcInit){ lcInit=true; lcReset(); lcPhase=LC_RACING; lcLastStep=t; }
  if(lcPhase==LC_RACING){
    if(t-lcLastStep >= LC_STEP_MS){ lcLastStep=t; lcStep(); }
  } else if(lcPhase==LC_FLASH){
    if(t-lcPhaseStart >= LC_FLASH_MS){ lcClearTrails(); lcPhase=LC_PAUSE; lcPhaseStart=t; }
  } else { // LC_PAUSE
    if(t-lcPhaseStart >= LC_PAUSE_MS){ lcReset(); lcPhase=LC_RACING; lcLastStep=t; }
  }
  lcRender();
  FastLED.show();
}
