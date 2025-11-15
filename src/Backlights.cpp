#include "Backlights.h"
#include <math.h>

boolean Backlights::backlightState = false;
byte Backlights::backlightHue = 128;
byte Backlights::backlightSaturation = 255;
byte Backlights::backlightBrightness = 255;
#if (NUM_LEDS > 6)
byte Backlights::underlightHue = 128;
byte Backlights::underlightSaturation = 128;
byte Backlights::underlightBrightness = 128;
#endif

void Backlights::begin()  {
	pixels.Begin(); // This initializes the NeoPixel library.
	pixels.Show();
}

void Backlights::loop() {
  //   enum patterns { dark, constant, rainbow, pulse, breath, num_patterns };
  uint8_t current_pattern = getLEDPattern();

  if (backlightState) {
#if (NUM_LEDS > 6)
    fill(backlightHue, backlightSaturation, backlightBrightness, 0, 6);
    fill(underlightHue, underlightSaturation, underlightBrightness, 6, NUM_LEDS);
#else
    fill(backlightHue, backlightSaturation, backlightBrightness, 0, NUM_LEDS);
#endif
    show();
  } else if (off || current_pattern == dark) {
    clear();
    show();
  }
  else if (current_pattern == constant) {
    uint16_t val = getLEDValue();

    val = val * brightness / 255;
    
    fill(getLEDHue(), getLEDSaturation(), val, 0, NUM_LEDS);
    show();
  }
  else if (current_pattern == rainbow) {
    rainbowPattern();
  }
  else if (current_pattern == pulse) {
    pulsePattern();
  }
  else if (current_pattern == breath) {
    breathPattern();
  }
}

void Backlights::pulsePattern() {
  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float valAdjust = 1 + abs(sin(2 * M_PI * millis() / pulse_length_millis)) * 254;
  uint16_t val = valAdjust * getLEDValue().value / 256;
  val = val * brightness / 255;

  fill(getLEDHue(), getLEDSaturation(), val, 0, NUM_LEDS);

  show();
}

void Backlights::breathPattern() {
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  // Avoid a 0 value as it shuts off the LEDs and we have to re-initialize.
  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float val = (exp(sin(2 * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;
  val = val * getLEDValue().value / 256;
  val = val * brightness / 255;

  fill(getLEDHue(), getLEDSaturation(), val, 0, NUM_LEDS);

  show();
}

void Backlights::rainbowPattern() {
  const uint16_t hue_per_digit = (256/NUM_DIGITS)/2;

  uint16_t hue = millis()/((21-getBreathPerMin().value) * 10) % 256;  
  
  uint16_t val = getLEDValue();

  val = val * brightness / 255;

  for (uint8_t digit=0; digit < NUM_LEDS; digit++) {
    // Shift the hue for this LED.
    uint16_t digitHue = (hue + digit*hue_per_digit) % 256;
 		setPixelColor(digit, digitHue, getLEDSaturation(), val);
  }
  show();
}

void Backlights::fill(byte hue, byte sat, byte val, byte start, byte end) {
	RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);

  for (uint8_t digit=start; digit < end; digit++) {
		pixels.SetPixelColor(digit, colorGamma.Correct(color));
  }
}

void Backlights::clear() {
  fill(0, 0, 0, 0, NUM_LEDS);
}

void Backlights::show() {
  pixels.Show();
}

void Backlights::setPixelColor(uint8_t digit, uint8_t hue, uint8_t sat, uint8_t val) {
		RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);
		pixels.SetPixelColor(digit, colorGamma.Correct(color));
}

const String Backlights::patterns_str[Backlights::num_patterns] = 
  { "Dark", "Constant", "Rainbow", "Pulse", "Breath" };
