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
  static IntConfigItem& getLEDHue() { static IntConfigItem led_hue("led_hue", 85); return led_hue; }
  static ByteConfigItem& getLEDValue() { static ByteConfigItem led_value("led_value", 255); return led_value; }
  static ByteConfigItem& getLEDSaturation() { static ByteConfigItem led_saturation("led_saturation", 255); return led_saturation; }
  static ByteConfigItem& getBreathPerMin() { static ByteConfigItem breath_per_min("breath_per_min", 10); return breath_per_min; }

  void begin();
  void loop();

  void togglePower() { off = !off; }
  void PowerOn()  { off = false; }
  void PowerOff()  { off = true; }
  void setOn(bool on) { off = !on; }
  void setDimming(bool dimming) { this->dimming = dimming; }

private:
  bool off;
  bool dimming = false;
  
  NeoPixelBus <NeoGrbFeature, Neo800KbpsMethod> pixels;
  NeoGamma<NeoGammaTableMethod> colorGamma;

  // Pattern methods
  void rainbowPattern();
  void pulsePattern();
  void breathPattern();

  void fill(uint8_t hue, uint8_t val, uint8_t sat);
  void show();
  void clear();
  void setPixelColor(uint8_t digit, uint8_t hue, uint8_t val, uint8_t sat);

  const uint32_t test_ms_delay = 250; 

};

#endif // BACKLIGHTS_H
