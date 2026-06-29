#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Tron-style light-cycle race. Spec:
//   docs/superpowers/specs/2026-06-29-lightcycle-race-design.md
// Stateful sim: occupancy grid + two cycles + phase machine. 60x60.
// ============================================================================
#define LC_STEP_MS        70      // race step interval
#define LC_FLASH_MS       2000    // winner flash duration
#define LC_FLASH_BLINK_MS 200     // winner pulse period
#define LC_PAUSE_MS       500     // blank pause before next round
#define LC_INTERCEPT_K    6       // how far ahead of the opponent the AI aims

enum { LC_EMPTY=0, LC_RED=1, LC_BLUE=2, LC_WALL=3 };
enum { LC_RACING=0, LC_FLASH=1, LC_PAUSE=2 };

struct LcCycle { int8_t x, y, dx, dy; bool alive; };

static uint8_t  lcGrid[NUM_LEDS];          // one cell per LED, indexed by lcGi()
static LcCycle  lcRed, lcBlue;
static uint8_t  lcPhase      = LC_PAUSE;
static uint32_t lcPhaseStart = 0;
static uint32_t lcLastStep   = 0;
static uint32_t lcRng        = 0x1234567u;
static uint16_t lcRound      = 0;
static uint8_t  lcWinner     = 0;          // 0 none/draw, 1 red, 2 blue
static bool     lcInit       = false;

static inline uint32_t lcRand(){ lcRng^=lcRng<<13; lcRng^=lcRng>>17; lcRng^=lcRng<<5; return lcRng; }
static inline int lcGi(int x,int y){ return y*NUM_COLS + x; }

static void lcClearTrails(){
  for(int i=0;i<NUM_LEDS;i++) if(lcGrid[i]!=LC_WALL) lcGrid[i]=LC_EMPTY;
}

static void lcReset(){
  for(int i=0;i<NUM_LEDS;i++) lcGrid[i]=LC_EMPTY;
  for(int x=0;x<NUM_COLS;x++){ lcGrid[lcGi(x,0)]=LC_WALL; lcGrid[lcGi(x,NUM_ROWS-1)]=LC_WALL; }
  for(int y=0;y<NUM_ROWS;y++){ lcGrid[lcGi(0,y)]=LC_WALL; lcGrid[lcGi(NUM_COLS-1,y)]=LC_WALL; }
  lcRound++;
  lcRng = 0x9E3779B9u ^ ((uint32_t)lcRound*2654435761u); if(lcRng==0) lcRng=1;
  int rowR = 12 + (int)(lcRand()%36);                 // 12..47
  int rowB; do { rowB = 12 + (int)(lcRand()%36); } while(abs(rowB-rowR)<4);
  lcRed  = { 8,  (int8_t)rowR,  1, 0, true };
  lcBlue = { 51, (int8_t)rowB, -1, 0, true };
  lcGrid[lcGi(lcRed.x, lcRed.y)]   = LC_RED;
  lcGrid[lcGi(lcBlue.x,lcBlue.y)]  = LC_BLUE;
  lcWinner = 0;
}

// pick a move (straight / left-turn / right-turn) that stays alive and cuts toward
// a point ahead of the opponent; false => no safe move (crash). y is down, so
// right-turn = (dx,dy)->(-dy,dx), left-turn = (dx,dy)->(dy,-dx).
static bool lcChoose(const LcCycle& c, const LcCycle& opp, int8_t& outdx, int8_t& outdy){
  int8_t cand[3][2] = { {c.dx,c.dy}, {(int8_t)c.dy,(int8_t)-c.dx}, {(int8_t)-c.dy,(int8_t)c.dx} };
  int ix = opp.x + opp.dx*LC_INTERCEPT_K; if(ix<1)ix=1; if(ix>NUM_COLS-2)ix=NUM_COLS-2;
  int iy = opp.y + opp.dy*LC_INTERCEPT_K; if(iy<1)iy=1; if(iy>NUM_ROWS-2)iy=NUM_ROWS-2;
  float best=-1e9f; bool found=false;
  for(int k=0;k<3;k++){
    int nx=c.x+cand[k][0], ny=c.y+cand[k][1];
    if(nx<0||nx>=NUM_COLS||ny<0||ny>=NUM_ROWS) continue;
    if(lcGrid[lcGi(nx,ny)]!=LC_EMPTY) continue;
    int open=0, ox=nx, oy=ny;                          // empty run straight ahead (anti dead-end)
    for(int s=0;s<8;s++){ ox+=cand[k][0]; oy+=cand[k][1];
      if(ox<0||ox>=NUM_COLS||oy<0||oy>=NUM_ROWS||lcGrid[lcGi(ox,oy)]!=LC_EMPTY) break; open++; }
    // chase hard: closing distance to the intercept point dominates; open-run is only
    // a light self-preservation nudge so they don't dive into a dead end.
    float score = -2.6f*(float)(abs(nx-ix)+abs(ny-iy)) + open*0.35f + (float)(lcRand()%5)*0.08f;
    if(k==0) score += 0.1f;                            // tiny straight preference
    if(score>best){ best=score; outdx=cand[k][0]; outdy=cand[k][1]; found=true; }
  }
  return found;
}

static void lcStep(){
  int8_t rdx=0,rdy=0,bdx=0,bdy=0;
  bool rOk = lcChoose(lcRed,  lcBlue, rdx,rdy);
  bool bOk = lcChoose(lcBlue, lcRed,  bdx,bdy);
  int rnx=lcRed.x+rdx,  rny=lcRed.y+rdy;
  int bnx=lcBlue.x+bdx, bny=lcBlue.y+bdy;
  bool rCrash=!rOk, bCrash=!bOk;
  if(rOk && bOk && rnx==bnx && rny==bny){ rCrash=true; bCrash=true; }   // head-on into same cell
  if(!rCrash){ lcRed.dx=rdx; lcRed.dy=rdy; lcRed.x=rnx; lcRed.y=rny; lcGrid[lcGi(rnx,rny)]=LC_RED; }
  else lcRed.alive=false;
  if(!bCrash){ lcBlue.dx=bdx; lcBlue.dy=bdy; lcBlue.x=bnx; lcBlue.y=bny; lcGrid[lcGi(bnx,bny)]=LC_BLUE; }
  else lcBlue.alive=false;
  if(rCrash||bCrash){ lcWinner = (rCrash&&bCrash)?0 : (rCrash?2:1); lcPhase=LC_FLASH; lcPhaseStart=millis(); }
}

static void lcRender(){
  for(int y=0;y<NUM_ROWS;y++) for(int x=0;x<NUM_COLS;x++){
    uint8_t g=lcGrid[lcGi(x,y)];
    CRGB c = CRGB(0x0A0E1A);
    if(g==LC_WALL) c = CRGB(0x2A3040);
    else if(g==LC_RED || g==LC_BLUE){
      CRGB base = (g==LC_RED)?CRGB(0xE03030):CRGB(0x3060E0);
      if(lcPhase==LC_FLASH){
        bool blinkOn = ((millis()/LC_FLASH_BLINK_MS)&1)==0;
        if(lcWinner==0 || lcWinner==g){ c = blinkOn ? CRGB(0xE8ECF4) : base; }  // winner/draw pulses white
        else { c = base; c.nscale8(80); }                                       // loser dimmed ~31%
      } else c = base;
    }
    leds[XY((uint8_t)x,(uint8_t)y)] = c;
  }
  if(lcPhase==LC_RACING){
    const int8_t nb[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    if(lcRed.alive){
      for(int k=0;k<4;k++){ int gx=lcRed.x+nb[k][0], gy=lcRed.y+nb[k][1];
        if(gx>=0&&gx<NUM_COLS&&gy>=0&&gy<NUM_ROWS) leds[XY((uint8_t)gx,(uint8_t)gy)] += CRGB(0x401810); }
      leds[XY((uint8_t)lcRed.x,(uint8_t)lcRed.y)] = CRGB(0xFFE0D8);
    }
    if(lcBlue.alive){
      for(int k=0;k<4;k++){ int gx=lcBlue.x+nb[k][0], gy=lcBlue.y+nb[k][1];
        if(gx>=0&&gx<NUM_COLS&&gy>=0&&gy<NUM_ROWS) leds[XY((uint8_t)gx,(uint8_t)gy)] += CRGB(0x101840); }
      leds[XY((uint8_t)lcBlue.x,(uint8_t)lcBlue.y)] = CRGB(0xD8E4FF);
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
