#include <FastLED.h>

void changeEffect(byte effect,byte currentEffect)
{
  if (effect != currentEffect)
  {
    gHue = 0;
    selectedEffect=effect;
    FastLED.setBrightness(BRIGHTNESS);
    EEPROM.write(2, effect);
    asm volatile("  jmp 0");
  }
}

void convertToSelectedEffect(int resistence)
{
  byte result = resistence / 70;
  if  (result>14)
    result=14;
  changeEffect(result, selectedEffect);
}