#include "Backlights.h"
#include <math.h>

#define BACKLIGHT_DIMMED_INTENSITY  4

void Backlights::begin()  {
	pixels.Begin(); // This initializes the NeoPixel library.
	pixels.Show();
}

void Backlights::loop() {
  //   enum patterns { dark, constant, rainbow, pulse, breath, num_patterns };
  uint8_t current_pattern = getLEDPattern();

  if ((off && !dimming) || current_pattern == dark) {
    clear();
    show();
  }
  else if (current_pattern == constant) {
    uint8_t val = getLEDValue();

    if (dimming) {
      val /= 2;
    }
    fill(getLEDHue(), getLEDSaturation(), val);
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
  uint8_t val = valAdjust * getLEDValue().value / 256;
  if (dimming) {
    val = val / 2 ;
  }  

  fill(getLEDHue(), getLEDSaturation(), val);

  show();
}

void Backlights::breathPattern() {
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  // Avoid a 0 value as it shuts off the LEDs and we have to re-initialize.
  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float val = (exp(sin(2 * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;
  val = val * getLEDValue().value / 256;

  if (dimming) {
    val = val / 2;
  }  

  fill(getLEDHue(), getLEDSaturation(), val);

  show();
}

void Backlights::rainbowPattern() {
  // Divide by 3 to spread it out some, so the whole rainbow isn't displayed at once.
  // TODO Make this /3 a parameter
  const uint16_t hue_per_digit = (256/NUM_DIGITS)/2;

  // Divide by 10 to slow down the rainbow rotation rate.
  // TODO Make this /10 a parameter
  uint16_t hue = millis()/100 % 256;  
  
  uint8_t val = getLEDValue();

  if (dimming) {
    val /= 2;
  }

  for (uint8_t digit=0; digit < NUM_DIGITS; digit++) {
    // Shift the hue for this LED.
    uint16_t digitHue = (hue + digit*hue_per_digit) % 256;
 		setPixelColor(digit, digitHue, getLEDSaturation(), val);
  }
  show();
}

void Backlights::fill(byte hue, byte sat, byte val) {
	RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);

  for (uint8_t digit=0; digit < NUM_DIGITS; digit++) {
		pixels.SetPixelColor(digit, colorGamma.Correct(color));
  }
}

void Backlights::clear() {
  fill(0, 0, 0);
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
