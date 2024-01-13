#ifndef BACKLIGHTS_H
#define BACKLIGHTS_H

#include "GLOBAL_DEFINES.h"

/*
 * A simple sub-class of the Adafruit_NeoPixel class, to configure it for
 * the EleksTube-IPS clock, and to add a little functionality.
 * 
 * The good news is, the pixel numbers in the string line up perfectly 
 * with the #defines in Hardware.h, so you can pass SECONDS_ONES directly
 * as the pixel index, no mapping required.
 * 
 * Otherwise, class Backlights behaves exactly as Adafruit_NeoPixel does.
 */
#include <stdint.h>
#include <ConfigItem.h>
#include <NeoPixelBus.h>

class Backlights {
public:
  Backlights() : pixels(NUM_DIGITS, BACKLIGHTS_PIN)
    {}

  enum patterns { dark, test, constant, rainbow, pulse, breath, num_patterns };
  const static String patterns_str[num_patterns];

  static ByteConfigItem& getLEDPattern() { static ByteConfigItem led_pattern("led_pattern", 3); return led_pattern; }
  static IntConfigItem& getColorPhase() { static IntConfigItem color_phase("color_phase", 0); return color_phase; }
  static ByteConfigItem& getLEDIntensity() { static ByteConfigItem led_intensity("led_intensity", 7); return led_intensity; }
  static ByteConfigItem& getBreathPerMin() { static ByteConfigItem breath_per_min("breath_per_min", 10); return breath_per_min; }
  BaseConfigItem **getConfigSet();
  void begin();
  void loop();

  void togglePower() { off = !off; pattern_needs_init = true; }
  void PowerOn()  { off = false; pattern_needs_init = true; }
  void PowerOff()  { off = true; pattern_needs_init = true; }
  void setOn(bool on) { off = !on; pattern_needs_init = true; }
  void setDimming(bool dimming) { this->dimming = dimming; }

  void setColorPhase(uint16_t phase) { getColorPhase().value = phase % max_phase; pattern_needs_init = true; }
  void adjustColorPhase(int16_t adj);
  
  void setIntensity(uint8_t intensity);
  void adjustIntensity(int16_t adj);

private:
  uint8_t old_pattern = 255;
  bool off;
  bool pattern_needs_init = true;
  bool dimming = false;
  
  NeoPixelBus <NeoGrbFeature, Neo800KbpsMethod> pixels;

  // Pattern methods
  void testPattern();
  void rainbowPattern();
  void pulsePattern();
  void breathPattern();

  // Helper methods
  uint8_t phaseToIntensity(uint16_t phase);
  uint32_t phaseToColor(uint16_t phase);
  void fill(uint32_t color);
  void show();
  void clear();
  void setBrightness(uint8_t level);
  void setPixelColor(uint8_t digit, uint32_t color);

  const uint16_t max_phase = 768;   // 256 up, 256 down, 256 off
  const uint8_t max_intensity = 8;  // 0 to 7
  const uint32_t test_ms_delay = 250; 

};

#endif // BACKLIGHTS_H
