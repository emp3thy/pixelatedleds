#pragma once
#include <FastLED.h>
#include <math.h>
#include "configuration.h"
#include "XYMatrix.h"

// ============================================================================
// Farmhouse seasons. Spec:
//   docs/superpowers/specs/2026-06-28-farmhouse-seasons-design.md
// Layered compositor, 60x60, one year on a ~4 min loop. y=0 top.
// ============================================================================
#define FH_YEAR_MS 240000UL        // full year (~4 min). Set 20000 to scrub fast.
// #define FH_DEBUG_PHASE 0.375f   // uncomment to pin the year (0.08 spring, 0.375 summer, 0.87 mid-winter)

// season centres / boundaries
#define FH_SPRING 0.125f
#define FH_SUMMER 0.375f
#define FH_AUTUMN 0.625f
#define FH_WINTER 0.875f
// discrete event windows
#define FH_HARVEST0 0.62f
#define FH_HARVEST1 0.68f
#define FH_SNOW0    0.70f
#define FH_SNOW1    0.82f
#define FH_MELT0    0.96f
#define FH_MELT1    0.05f

static float fhSnowAmt(float p);   // fwd decl (defined in SNOW section); used by foreground path

// ---- math helpers ----
static inline float fhClampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static inline float fhSmooth(float t){ t=fhClampf(t,0,1); return t*t*(3.0f-2.0f*t); }
static inline uint8_t fhL8(uint8_t a,uint8_t b,float t){ return (uint8_t)(a+(b-a)*t+0.5f); }
static inline CRGB fhMix(CRGB a,CRGB b,float t){ t=fhClampf(t,0,1);
  return CRGB(fhL8(a.r,b.r,t),fhL8(a.g,b.g,t),fhL8(a.b,b.b,t)); }

// ---- raster helpers (coverage AA blends into leds[]) ----
static inline void fhPlot(int x,int y,CRGB c,float a){
  if(x<0||x>=NUM_COLS||y<0||y>=NUM_ROWS||a<=0.0f) return;
  uint16_t i=XY((uint8_t)x,(uint8_t)y);
  leds[i]= a>=1.0f ? c : fhMix(leds[i],c,a);
}
static inline void fhFillRow(int y,int x0,int x1,CRGB c){
  for(int x=x0;x<=x1;x++) fhPlot(x,y,c,1.0f);
}
static void fhDisc(float cx,float cy,float r,CRGB c){
  int y0=(int)floorf(cy-r-1), y1=(int)ceilf(cy+r+1);
  int x0=(int)floorf(cx-r-1), x1=(int)ceilf(cx+r+1);
  for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
    float d=sqrtf((x-cx)*(x-cx)+(y-cy)*(y-cy));
    float a=fhClampf(r+0.5f-d,0.0f,1.0f);
    fhPlot(x,y,c,a);
  }
}
// soft radial glow (falls off to nothing at the edge) for hazy light
static void fhGlow(float cx,float cy,float r,CRGB c,float maxA){
  int y0=(int)floorf(cy-r), y1=(int)ceilf(cy+r);
  int x0=(int)floorf(cx-r), x1=(int)ceilf(cx+r);
  for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
    float d=sqrtf((x-cx)*(x-cx)+(y-cy)*(y-cy));
    float a=fhClampf(1.0f-d/r,0,1); a=a*a*maxA;
    fhPlot(x,y,c,a);
  }
}
static void fhLine(int x0,int y0,int x1,int y1,CRGB c){
  int dx=abs(x1-x0),dy=-abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,e=dx+dy;
  for(;;){ fhPlot(x0,y0,c,1.0f); if(x0==x1&&y0==y1) break; int e2=2*e; if(e2>=dy){e+=dy;x0+=sx;} if(e2<=dx){e+=dx;y0+=sy;} }
}

static inline float fhGround(float x){
  return 24.0f + 1.8f*sinf(x*0.45f) + 1.3f*sinf(x*0.17f+2.0f);
}

static inline float fhYearPhase(){
#ifdef FH_DEBUG_PHASE
  return FH_DEBUG_PHASE;
#else
  return (float)(millis()%FH_YEAR_MS)/(float)FH_YEAR_MS;
#endif
}

// ---- cyclic keyframe interpolation (phase wraps at 1.0) ----
static float fhCycF(float p,const float* ph,const float* val,int n){
  for(int i=0;i<n-1;i++){ if(p>=ph[i] && p<ph[i+1]){
    float t=(p-ph[i])/(ph[i+1]-ph[i]); return val[i]+(val[i+1]-val[i])*fhSmooth(t); } }
  return val[n-1];
}
static CRGB fhCycRGB(float p,const float* ph,const CRGB* col,int n){
  for(int i=0;i<n-1;i++){ if(p>=ph[i] && p<ph[i+1]){
    float t=fhSmooth((p-ph[i])/(ph[i+1]-ph[i])); return fhMix(col[i],col[i+1],t); } }
  return col[n-1];
}

// =================== SKY ===================
static const float FH_SKY_PH[]  = {0.0f, FH_SPRING, FH_SUMMER, FH_AUTUMN, FH_WINTER, 1.0f};
static const CRGB  FH_SKY_TOP[] = {CRGB(0x8FCBEE),CRGB(0x8FCBEE),CRGB(0x7FC0EC),CRGB(0x9FC2D8),CRGB(0xAEB9C4),CRGB(0x8FCBEE)};
static const CRGB  FH_SKY_BOT[] = {CRGB(0xD6EEF8),CRGB(0xD6EEF8),CRGB(0xCFE6F4),CRGB(0xE6E3CF),CRGB(0xDBE1E6),CRGB(0xD6EEF8)};
static const CRGB  FH_SUN_COL[] = {CRGB(0xFFE9A0),CRGB(0xFFE9A0),CRGB(0xFFE27A),CRGB(0xFFD27A),CRGB(0xFFF3C8),CRGB(0xFFE9A0)};

static const float FH_SUNY_PH[]  = {0.0f, FH_SPRING, FH_SUMMER, FH_AUTUMN, 0.875f, 1.0f};
static const float FH_SUNY_VAL[] = {8.0f, 7.0f,      6.0f,      9.0f,      11.0f,   8.0f};
static const float FH_SUNR_PH[]  = {0.0f, FH_SUMMER, FH_WINTER, 1.0f};
static const float FH_SUNR_VAL[] = {4.0f, 4.0f,      4.0f,      4.0f};
static inline float fhSunY(float p){ return fhCycF(p,FH_SUNY_PH,FH_SUNY_VAL,6); }

static void fhDrawSky(float p){
  CRGB top=fhCycRGB(p,FH_SKY_PH,FH_SKY_TOP,6);
  CRGB bot=fhCycRGB(p,FH_SKY_PH,FH_SKY_BOT,6);
  for(uint8_t y=0;y<28;y++){
    CRGB c=fhMix(top,bot,(float)y/26.0f);
    for(uint8_t x=0;x<NUM_COLS;x++) leds[XY(x,y)]=c;
  }
  float r=fhCycF(p,FH_SUNR_PH,FH_SUNR_VAL,4);
  CRGB sc=fhCycRGB(p,FH_SKY_PH,FH_SUN_COL,6);
  float sy=fhSunY(p);
  fhGlow(50.0f, sy, r*1.9f, sc, 0.55f);     // halo -> brighter, softer sun
  fhDisc(50.0f, sy, r, sc);                 // (winter: low + big, hills occlude its lower half)
}

// =================== HILLS ===================
struct FhHill { float cx, rx, ry; };
static const FhHill FH_HILLS[2] = { {16,21,13}, {42,23,16} };

static int fhDomeTop(int x){
  int t=-1;
  for(int h=0;h<2;h++){
    float n=(x-FH_HILLS[h].cx)/FH_HILLS[h].rx; if(n<-1||n>1) continue;
    int top=24-(int)lroundf(FH_HILLS[h].ry*sqrtf(1.0f-n*n));
    if(t<0||top<t) t=top;
  }
  return t;
}

static const float FH_HILL_PH[]  = {0.0f, FH_SPRING, FH_SUMMER, FH_AUTUMN, FH_WINTER, 1.0f};
static const CRGB  FH_HILL_COL[] = {CRGB(0x7BB24A),CRGB(0x7BB24A),CRGB(0x6FA23F),CRGB(0xA8984E),CRGB(0xE8EEF2),CRGB(0x7BB24A)};
static inline CRGB fhHillGrass(float p){ return fhCycRGB(p,FH_HILL_PH,FH_HILL_COL,6); }

static void fhDrawHills(float p){
  CRGB g=fhHillGrass(p);
  for(int x=0;x<NUM_COLS;x++){
    int dt=fhDomeTop(x); if(dt<0) continue;
    int gb=(int)lroundf(fhGround(x));
    for(int y=dt;y<gb;y++) fhPlot(x,y,g,1.0f);
  }
}

// =================== WOOD (hill trees) ===================
static const CRGB FH_GREEN[7] ={CRGB(0x347E3A),CRGB(0x2E7434),CRGB(0x46A24E),CRGB(0x3E9446),CRGB(0x52AE59),CRGB(0x43994A),CRGB(0x3C913F)};
static const CRGB FH_TURN[7]  ={CRGB(0xE0892E),CRGB(0xD6552B),CRGB(0xE8B23A),CRGB(0xC24A2A),CRGB(0xB5862F),CRGB(0xE0A030),CRGB(0xD67A2A)};
static const CRGB FH_SPRGRN[7]={CRGB(0x86C552),CRGB(0xA6D86A),CRGB(0x6FB23E),CRGB(0x9ACB5A),CRGB(0x7CC04A),CRGB(0xA0D060),CRGB(0x8AC850)};

static float fhCanopyAmt(float p){
  if(p<FH_SPRING) return fhSmooth((p-0.02f)/(FH_SPRING-0.02f));
  if(p<FH_AUTUMN) return 1.0f;
  if(p<0.78f)     return 1.0f-fhSmooth((p-FH_AUTUMN)/(0.78f-FH_AUTUMN));
  return 0.0f;
}
static CRGB fhFoliage(float p,uint8_t seed){
  CRGB summer=FH_GREEN[seed%7];
  if(p<FH_SPRING) return fhMix(FH_SPRGRN[seed%7],summer,fhSmooth(p/FH_SPRING));
  if(p<FH_AUTUMN) return summer;
  float t=fhSmooth((p-FH_AUTUMN)/(0.78f-FH_AUTUMN));
  return fhMix(summer,FH_TURN[seed%7],t);
}

static void fhDrawWood(float p){
  float amt=fhCanopyAmt(p);
  for(int h=0;h<2;h++){
    const FhHill& H=FH_HILLS[h];
    for(int ax=(int)(H.cx-H.rx)+2; ax<=(int)(H.cx+H.rx)-2; ax+=2){
      float n=(ax-H.cx)/H.rx; if(n<-0.94f||n>0.94f) continue;
      int top=24-(int)lroundf(H.ry*sqrtf(1.0f-n*n));
      int bl=(int)lroundf(fhGround(ax));
      int start=top+2+(ax&1);
      for(int ty=start; ty+4<=bl; ty+=4){
        int tx=ax+((ty&1)?1:0);
        uint8_t seed=(uint8_t)(ax*3+ty);
        fhPlot(tx,ty+1,CRGB(0x4A3318),1.0f);
        fhPlot(tx,ty+2,CRGB(0x4A3318),1.0f);
        if(amt>0.02f) fhDisc(tx,ty,2.3f*amt+0.3f, fhFoliage(p,seed));
      }
    }
  }
}

// =================== FIELD ===================
static inline CRGB fhStripe(CRGB base,CRGB hi,CRGB lo,float v){
  return v>0 ? fhMix(base,hi,v*0.5f) : fhMix(base,lo,-v*0.5f);
}
static CRGB fhCropColor(float p,int fx,int fy){
  float v=sinf(fy*0.9f + 1.3f*sinf(fx*0.16f));
  CRGB earth=fhStripe(CRGB(0x5A4028),CRGB(0x6E5234),CRGB(0x412C1A),v);
  CRGB green=fhStripe(CRGB(0xADCB55),CRGB(0xC6DC72),CRGB(0x93B845),v);
  CRGB gold =fhStripe(CRGB(0xE8B43A),CRGB(0xF8D758),CRGB(0xC6871E),v);
  CRGB stub =fhStripe(CRGB(0xE4D7A0),CRGB(0xF0E6BE),CRGB(0xCBBA7E),v);
  if(p<FH_SPRING) return earth;
  if(p<FH_SUMMER){ float gp=(p-FH_SPRING)/(FH_SUMMER-FH_SPRING);
    float thr=((fx*7+fy*13)%100)/100.0f; float t=fhClampf((gp-thr)*3.0f,0,1);
    return fhMix(earth,green,t); }
  if(p<FH_HARVEST0){ float rp=(p-FH_SUMMER)/(FH_HARVEST0-FH_SUMMER);
    float off=((fx*3+fy*5)%10)/30.0f; float t=fhClampf((rp-off)*1.6f,0,1);
    return fhMix(green,gold,t); }
  if(p<FH_HARVEST1){ float hp=(p-FH_HARVEST0)/(FH_HARVEST1-FH_HARVEST0);
    float sm=fx/60.0f; float t=fhClampf((hp-sm)*6.0f,0,1);
    return fhMix(gold,stub,t); }
  if(p<FH_MELT0) return stub;
  return earth;
}

static const float FH_MARGIN_PH[]  = {0.0f, FH_SUMMER, FH_AUTUMN, 1.0f};
static const CRGB  FH_MARGIN_COL[] = {CRGB(0x7CB24A),CRGB(0x74A83F),CRGB(0x9AA24A),CRGB(0x7CB24A)};
static inline CRGB fhGrassMargin(float p,int y){
  CRGB c=fhCycRGB(p,FH_MARGIN_PH,FH_MARGIN_COL,4);
  return (y&1)? c : fhMix(c,CRGB(0xFFFFFF),0.04f);
}

static void fhDrawField(float p){
  for(int fx=0;fx<NUM_COLS;fx++){
    int g0=(int)lroundf(fhGround(fx));
    float g1f=fhGround(fx)+2.6f+1.2f*sinf(fx*0.32f);
    for(int gy=g0; gy<(int)floorf(g1f); gy++) fhPlot(fx,gy,fhGrassMargin(p,gy),1.0f);
    for(int fy=(int)floorf(g1f); fy<46; fy++){
      CRGB col=fhCropColor(p,fx,fy);
      float cov=fhClampf((fy+1)-g1f,0,1);
      if(cov<1.0f) col=fhMix(fhGrassMargin(p,fy),col,cov);
      fhPlot(fx,fy,col,1.0f);
    }
  }
}

// =================== BARN ===================
static void fhGambrelRoof(CRGB c){
  for(int y=31;y<=37;y++){
    float L,R;
    if(y<34){ float hw=(y-31)*2.0f; L=42-hw; R=42+hw; }
    else    { float k=(y-34);       L=36-k;  R=48+k;  }
    for(int x=(int)floorf(L);x<=(int)ceilf(R);x++){
      float cov=fhClampf(min((float)x+1,R)-max((float)x,L),0,1);
      fhPlot(x,y,c,cov);
    }
  }
}
static void fhDrawBarn(float p){
  fhGambrelRoof(CRGB(0x8F97A1));                              // grey roof (snow cap added in winter)
  for(int y=37;y<46;y++) fhFillRow(y,34,49,CRGB(0xBE3B2C));   // red body
  for(int y=37;y<46;y++){ fhPlot(38,y,CRGB(0xB23528),1.0f); fhPlot(45,y,CRGB(0xB23528),1.0f); }
  for(int y=40;y<46;y++) fhFillRow(y,38,44,CRGB(0x241006));   // open doorway (centred)
}

// warm-lit central hayloft window in the gable; drawn over the snow so it stays lit in winter
static void fhDrawBarnWindow(float p){
  for(int y=34;y<=37;y++){ fhPlot(40,y,CRGB(0x3A2A14),1.0f); fhPlot(43,y,CRGB(0x3A2A14),1.0f); }
  fhFillRow(34,40,43,CRGB(0x3A2A14)); fhFillRow(37,40,43,CRGB(0x3A2A14));   // frame
  uint32_t t=millis(); float gl=0.85f+0.15f*sinf(t*0.002f);
  CRGB glass=fhMix(CRGB(0xC8A14A),CRGB(0xFBE38A),gl);
  fhPlot(41,35,glass,1.0f); fhPlot(42,35,glass,1.0f);
  fhPlot(41,36,glass,1.0f); fhPlot(42,36,glass,1.0f);
}

// =================== WALL + HERO TREE ===================
static void fhDrawWall(float p){
  for(int x=0;x<NUM_COLS;x++){
    if(x>=27 && x<=32) continue;
    fhPlot(x,46,(x%3==0)?CRGB(0x6E6C68):CRGB(0x94918C),1.0f);
    fhPlot(x,47,(x%3==0)?CRGB(0x6E6C68):CRGB(0x94918C),1.0f);
    fhPlot(x,48,((x+1)%3==0)?CRGB(0x6E6C68):CRGB(0x9C998F),1.0f);
    fhPlot(x,49,((x+1)%3==0)?CRGB(0x6E6C68):CRGB(0x9C998F),1.0f);
    fhPlot(x,46,CRGB(0xC2BFB8),1.0f);
    fhPlot(x,48,CRGB(0x6E6C68),1.0f);
  }
  for(int y=44;y<50;y++){ fhPlot(27,y,CRGB(0x5A3F22),1.0f); fhPlot(32,y,CRGB(0x5A3F22),1.0f); }
  for(int y=45;y<50;y++) fhFillRow(y,28,31,CRGB(0x8A6A3E));
  fhFillRow(46,28,31,CRGB(0xA6824E)); fhFillRow(48,28,31,CRGB(0xA6824E));
}

static void fhDrawHeroTree(float p){
  float amt=fhCanopyAmt(p);
  if(amt<=0.02f) return;   // leafless winter tree is drawn on top of the snow (fhDrawBareTree)
  for(int y=40;y<47;y++){ fhPlot(12,y,CRGB(0x6B4A2B),1.0f); fhPlot(13,y,CRGB(0x6B4A2B),1.0f); }
  static const float CL[6][4]={{12,34,3.2f,0},{10,37,3.0f,1},{14,37,3.0f,2},{12,38,3.4f,3},{13,33,2.4f,4},{10,34,2.2f,5}};
  for(int i=0;i<6;i++){
    CRGB c=fhFoliage(p,(uint8_t)CL[i][3]);
    fhDisc(CL[i][0],CL[i][1],CL[i][2]*amt+0.3f,c);
  }
  if(p<FH_SPRING){
    float bl=fhSmooth(1.0f-(p/FH_SPRING));
    if(bl>0.05f){
      fhDisc(11,35,1.2f,fhMix(CRGB(0xF2B6C6),CRGB(0xFFFFFF),0.3f));
      fhDisc(14,36,1.0f,CRGB(0xF2B6C6));
    }
  }
}

// =================== FOREGROUND ===================
static float fhFlowerAmt(float p){
  if(p<0.04f) return 0.0f;
  if(p<FH_SUMMER) return fhSmooth((p-0.04f)/(FH_SUMMER-0.04f));
  if(p<FH_AUTUMN) return 1.0f-0.4f*fhSmooth((p-FH_SUMMER)/(FH_AUTUMN-FH_SUMMER));
  if(p<0.72f)     return 0.6f*(1.0f-fhSmooth((p-FH_AUTUMN)/(0.72f-FH_AUTUMN)));
  return 0.0f;
}
static inline float fhPathCenter(int gy){ float t=(gy-49)/11.0f; return 29.0f+7.0f*sinf(t*3.6f)*t; }
static inline float fhPathHalf(int gy){ float t=(gy-49)/11.0f; return 2.4f+t*4.2f; }

static void fhDrawForeground(float p){
  CRGB fg = (p<FH_AUTUMN)? CRGB(0x6FA537) : fhMix(CRGB(0x6FA537),CRGB(0xB6A24C),fhSmooth((p-FH_AUTUMN)/0.13f));
  for(int y=50;y<60;y++) fhFillRow(y,0,59,fg);

  float snowP=fhSnowAmt(p);                               // winter: path goes icy grey-white (still visible)
  CRGB ptop=fhMix(CRGB(0xC8B07A),CRGB(0xCFD4D8),snowP), pbot=fhMix(CRGB(0xB49A63),CRGB(0xBBC1C6),snowP);
  for(int y=50;y<60;y++){                                  // start below the wall so it doesn't cover the gate
    float c=fhPathCenter(y), hh=fhPathHalf(y), tt=(y-49)/11.0f;
    CRGB pc=fhMix(ptop,pbot,tt);
    float L=c-hh, R=c+hh;
    for(int x=(int)floorf(L)-1;x<=(int)ceilf(R);x++){
      float cov=fhClampf(min((float)x+1,R)-max((float)x,L),0,1);
      if(cov>0) fhPlot(x,y,pc,cov);
    }
  }

  float fa=fhFlowerAmt(p);
  if(fa>0.05f){
    static const int FX[8]={5,11,18,24,40,46,52,57};
    static const int FYr[8]={54,57,52,58,53,56,51,55};
    static const CRGB FC[3]={CRGB(0xF2F0E2),CRGB(0xF2D24A),CRGB(0xE68FB0)};
    for(int i=0;i<8;i++){
      float thr=(i+1)/9.0f;
      if(fa<thr) continue;
      float c=fhPathCenter(FYr[i]), hh=fhPathHalf(FYr[i]);
      if(FX[i]>c-hh-1 && FX[i]<c+hh+1) continue;
      fhPlot(FX[i],FYr[i],FC[i%3],fhClampf((fa-thr)*4,0,1));
    }
  }

  bool ears = (p>=FH_SUMMER && p<FH_HARVEST0);
  if(ears){
    float et=fhClampf((p-FH_SUMMER)/(FH_HARVEST0-FH_SUMMER),0,1);
    CRGB stalk=fhMix(CRGB(0x7FA840),CRGB(0xC9A23A),et), head=fhMix(CRGB(0xA6C85A),CRGB(0xF2C648),et);
    static const int EX[8]={3,6,9,2,57,54,51,58};
    static const int EY[8]={57,55,58,53,57,55,58,53};
    for(int i=0;i<8;i++){
      for(int k=0;k<6;k++) fhPlot(EX[i],EY[i]+k,stalk,1.0f);
      fhDisc(EX[i],EY[i]-1,1.6f,head);
      fhPlot(EX[i],EY[i]-3,head,1.0f);
    }
  }
}

// =================== SNOW ===================
static float fhSnowAmt(float p){
  if(p>=FH_SNOW0 && p<FH_SNOW1) return fhSmooth((p-FH_SNOW0)/(FH_SNOW1-FH_SNOW0));
  if(p>=FH_SNOW1 && p<FH_MELT0) return 1.0f;
  if(p>=FH_MELT0) return 1.0f-fhSmooth((p-FH_MELT0)/((1.0f-FH_MELT0)+FH_MELT1));
  if(p<FH_MELT1)  return 1.0f-fhSmooth(((1.0f-FH_MELT0)+p)/((1.0f-FH_MELT0)+FH_MELT1));
  return 0.0f;
}
static float fhSnowCover(int x,int y,float p){
  float amt=fhSnowAmt(p); if(amt<=0.0f || y<14) return 0.0f;
  float j=((x*7+y*13)%5)/40.0f;
  float cov=fhClampf((amt-j)*3.0f,0,1);
  if(y>=49){ float c=fhPathCenter(y),hh=fhPathHalf(y); float d=fabsf((float)x-c);
    float clear=fhClampf((hh+0.5f-d)/2.0f,0,1);   // 1 on path centre -> 0 off; ~2px fuzzy fade
    cov*=(1.0f-clear);                            // path centre stays clear, snow feathers in at edges
  }
  return cov;
}
static void fhDrawSnow(float p){
  if(fhSnowAmt(p)<=0.0f) return;
  for(int y=14;y<NUM_ROWS;y++) for(int x=0;x<NUM_COLS;x++){
    // keep structures clean: barn is drawn on top of the snow (after this layer)
    if(y>=47 && y<50) continue;                      // stone-wall faces (top row 46 keeps a cap)
    if(x>=27 && x<=32 && y>=44 && y<50) continue;    // wooden gate
    float cov=fhSnowCover(x,y,p); if(cov<=0) continue;
    // rolling snow drifts: wavy blue-grey shadow bands + white highlights (ref style),
    // plus a gentle depth gradient toward the foreground. Field and hill share this tone.
    float band=0.5f+0.5f*sinf(y*0.55f + 1.4f*sinf(x*0.13f) + sinf(x*0.05f));
    float depth=fhClampf((y-14)/45.0f,0,1);
    CRGB snow=fhMix(CRGB(0xF2F6FA),CRGB(0xBFD0E2), band*0.5f + depth*0.22f);
    fhPlot(x,y,snow,cov);
  }
}

// bare hero tree, drawn ON TOP of the snow so it reads in the winter foreground
static void fhDrawBareTree(float p){
  if(fhCanopyAmt(p)>0.02f) return;                    // only when leafless (winter)
  float xa=fhSmooth((p-0.815f)/0.015f)*(1.0f-fhSmooth((p-0.905f)/0.015f));
  if(xa>0.2f) return;                                 // hidden behind the Christmas tree
  CRGB w=CRGB(0x4A3520);
  for(int y=33;y<47;y++){ fhPlot(12,y,w,1.0f); fhPlot(13,y,w,1.0f); }       // trunk
  fhPlot(11,37,w,1.0f); fhPlot(10,35,w,1.0f); fhPlot(9,34,w,1.0f);          // up-left limb
  fhPlot(14,37,w,1.0f); fhPlot(15,35,w,1.0f); fhPlot(16,34,w,1.0f);         // up-right limb
  fhPlot(12,33,w,1.0f); fhPlot(11,32,w,1.0f); fhPlot(13,32,w,1.0f);         // crown
  fhPlot(10,39,w,1.0f); fhPlot(15,39,w,1.0f);                              // lower limbs
  // a little snow caught on the branches
  fhPlot(12,31,CRGB(0xEFF4F8),0.8f); fhPlot(9,33,CRGB(0xEFF4F8),0.7f); fhPlot(16,33,CRGB(0xEFF4F8),0.7f);
}

// winter easter egg: a small lit Christmas tree in front of the bare tree,
// only during the MIDDLE THIRD of winter (~0.83..0.905), fading in/out at the edges.
static void fhDrawXmasTree(float p){
  float amt = fhSmooth((p-0.815f)/0.015f) * (1.0f - fhSmooth((p-0.905f)/0.015f));
  if(amt<=0.05f) return;
  CRGB g=CRGB(0x1F6B2E);
  // big conifer centred on x=12, directly in front of the bare tree (rows 38..49)
  static const int8_t rowL[12]={12,11,11,10,10, 9, 9, 8, 8, 8,12,12};
  static const int8_t rowR[12]={12,13,13,14,14,15,15,16,16,16,12,12};
  for(int i=0;i<12;i++){ int y=38+i; CRGB c=(i>=10)?CRGB(0x5A3F22):g;   // last 2 rows = trunk
    for(int x=rowL[i];x<=rowR[i];x++) fhPlot(x,y,c,amt); }
  uint32_t t=millis();
  float sb=0.6f+0.4f*sinf(t*0.004f);                                    // star twinkle
  fhPlot(12,37,CRGB(0xFFE24A),amt*fhClampf(sb,0,1));
  static const int  LX[12]={11,13,10,14,12, 9,15,11,13,12,10,14};
  static const int  LY[12]={40,41,43,43,42,45,45,46,46,44,46,47};
  static const CRGB PAL[4]={CRGB(0xFF3B30),CRGB(0xFFD24A),CRGB(0x4AA8FF),CRGB(0xCFFFD6)};
  for(int i=0;i<12;i++){
    float b=0.45f+0.55f*sinf(t*0.006f + i*1.7f);                        // each light twinkles
    CRGB c=PAL[(i + (int)(t/600))%4];                                   // colours cycle
    fhPlot(LX[i],LY[i],c,amt*fhClampf(b,0,1));
  }
}


// =================== WEATHER PARTICLES ===================
#define FH_NPART 40
static void fhDrawWeather(float p){
  float leaves = (p>=FH_AUTUMN && p<FH_SNOW0)? fhSmooth((p-FH_AUTUMN)/(FH_SNOW0-FH_AUTUMN)) : 0.0f;
  float snow   = (p>=FH_SNOW0 && p<FH_MELT0)? 1.0f : 0.0f;
  uint32_t t=millis();
  // spring showers: blue rain, intermittent (~50% on/off over ~80s)
  bool springRain = (p>=0.04f && p<FH_SUMMER) && (sinf((float)t*0.00008f) > 0.0f);
  for(int i=0;i<FH_NPART;i++){
    float col = (float)((i*37)%60);
    float speed = springRain ? (15.0f + (i%5)) : (6.0f + (i%5));   // rain falls faster
    float sway = sinf((t*0.001f)+(i*1.3f))* (springRain?0.6f:2.0f); // rain barely drifts
    float fall = fmodf((t*0.001f*speed)+(i*7), 64.0f);
    int x=(int)(col+sway), y=(int)(fall-4);
    if(y<0||y>=NUM_ROWS||x<0||x>=NUM_COLS) continue;
    if(snow>0)          fhPlot(x,y,CRGB(0xF2F6F8),0.9f);
    else if(leaves>0)   fhPlot(x,y,FH_TURN[i%7],0.85f*leaves);
    else if(springRain) fhPlot(x,y,CRGB(0x5AA0E6),1.0f);    // blue rain (opaque -> no brown tint over the field)
  }
}

// =================== COMPOSITOR ===================
static void fhRender(float p){
  fhDrawSky(p);
  fhDrawHills(p);
  fhDrawWood(p);
  fhDrawField(p);
  fhDrawWall(p);
  fhDrawHeroTree(p);
  fhDrawForeground(p);
  fhDrawSnow(p);
  fhDrawBarn(p);           // barn drawn over the snow (clean red walls, no skip-rectangle halo)
  fhDrawBarnWindow(p);
  fhDrawBareTree(p);
  fhDrawXmasTree(p);
  fhDrawWeather(p);
}

void farmhouseSeasons(){
  fhRender(fhYearPhase());
  FastLED.show();
}
