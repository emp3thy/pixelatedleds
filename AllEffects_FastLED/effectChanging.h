#include <FastLED.h>

void changeEffect(byte effect, byte currentEffect)
{
  if (effect != currentEffect)
  {
    gHue = 0;
    selectedEffect = effect;
    FastLED.setBrightness(BRIGHTNESS);
    EEPROM.write(2, effect);
    asm volatile("  jmp 0");
  }
}
int lastSetResistence = -20;

void convertToSelectedEffect(int resistence)
{
  int difference = resistence - lastSetResistence;
  // just in case the value is fluctuating a couple of points in either direction
  //because I bought a cheap potentiometer
  if (lastSetResistence > 10 || lastSetResistence < -10)
  {
    byte result = resistence / 70;
    if (result > 14)
      result = 14;
    changeEffect(result, selectedEffect);
  }
}