#include <FastLED.h>

void changeEffect(byte effect, byte currentEffect)
{
  if (effect != currentEffect)
  {
    gHue = 0;
    selectedEffect = effect;
    FastLED.setBrightness(BRIGHTNESS);
    asm volatile("  jmp 0");
  }
}
int lastSetResistence = -20;

void convertToSelectedEffect(int resistence)
{
  int difference = resistence - lastSetResistence;
  // just in case the value is fluctuating a couple of points in either direction
  //because I bought a cheap potentiometer
  if (lastSetResistence > 5 || lastSetResistence < -5)
  {
    byte result = resistence / 78;
    if (result > 13)
      result = 13;
    changeEffect(result, selectedEffect);
  }
}