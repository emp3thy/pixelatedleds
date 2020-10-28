void changeEffect() {
  gHue = 0;
  if (digitalRead (BUTTON) == HIGH) {
    selectedEffect++;
    fadeToBlackBy( leds, NUM_LEDS, 20);
    FastLED.show();
    FastLED.setBrightness(BRIGHTNESS);
    EEPROM.write(1, selectedEffect);
    asm volatile ("  jmp 0");
  }
}