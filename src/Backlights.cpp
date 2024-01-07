#include "Backlights.h"
#include <math.h>

#define BACKLIGHT_DIMMED_INTENSITY  1

void Backlights::begin()  {
	pixels.Begin(); // This initializes the NeoPixel library.
	pixels.Show();
}

void Backlights::adjustColorPhase(int16_t adj) {
  int16_t new_phase = (int16_t(getColorPhase().value%max_phase) + adj) % max_phase;
  while (new_phase < 0) {
    new_phase += max_phase;
  }
  setColorPhase(new_phase); 
}

void Backlights::adjustIntensity(int16_t adj) {
  int16_t new_intensity = (int16_t(getLEDIntensity().value) + adj) % max_intensity;
  while (new_intensity < 0) {
    new_intensity += max_intensity;
  }
  setIntensity(new_intensity);
}

void Backlights::setIntensity(uint8_t intensity) {
  getLEDIntensity().value = intensity;
  setBrightness(0xFF >> max_intensity - getLEDIntensity().value - 1);
  pattern_needs_init = true;
}

void Backlights::loop() {
  //   enum patterns { dark, test, constant, rainbow, pulse, breath, num_patterns };
  uint8_t current_pattern = getLEDPattern().value;
  pattern_needs_init = old_pattern != current_pattern;

  if (off || current_pattern == dark) {
    clear();
    show();
  }
  else if (current_pattern == test) {
    testPattern();
  }
  else if (current_pattern == constant) {
    fill(phaseToColor(getColorPhase().value));
    if (dimming) {
        setBrightness(0xFF >> max_intensity - (BACKLIGHT_DIMMED_INTENSITY) - 1);
      } else {
        setBrightness(0xFF >> max_intensity - getLEDIntensity().value - 1);
      }
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

  old_pattern = current_pattern;
  pattern_needs_init = false;
}

void Backlights::pulsePattern() {
  fill(phaseToColor(getColorPhase().value));

  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float val = 1 + abs(sin(2 * M_PI * millis() / pulse_length_millis)) * 254;
  if (dimming) {
    val = val * BACKLIGHT_DIMMED_INTENSITY / 7;
  }  
  setBrightness((uint8_t)val);

  show();
}

void Backlights::breathPattern() {
  fill(phaseToColor(getColorPhase().value));

  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  // Avoid a 0 value as it shuts off the LEDs and we have to re-initialize.
  float pulse_length_millis = (60.0f * 1000) / getBreathPerMin().value;
  float val = (exp(sin(2 * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;

  if (dimming) {
    val = val * BACKLIGHT_DIMMED_INTENSITY / 7;
  }  

  uint8_t brightness = (uint8_t)val;
  if (brightness < 1) { brightness = 1; }
  setBrightness(brightness);

  show();
}

void Backlights::testPattern() {
  const uint8_t num_colors = 4;  // or 3 if you don't want black
  uint8_t num_states = NUM_DIGITS * num_colors;
  uint8_t state = (millis()/test_ms_delay) % num_states;

  uint8_t digit = state/num_colors;
  uint32_t color = 0xFF0000 >> (state%num_colors)*8;
  
  clear();
  setPixelColor(digit, color);
  show();

}

uint8_t Backlights::phaseToIntensity(uint16_t phase) {
  uint16_t color = 0;
  if (phase <= 255) {
    // Ramping up
    color = phase;
  }
  else if (phase <= 511) {
    // Ramping down
    color = 511-phase;
  }
  else {
    // Off
    color = 0;
  }
  if (color > 255) {
    // TODO: Trigger ERROR STATE, bug in code.
  }
  return uint8_t(color % 256);
}

uint32_t Backlights::phaseToColor(uint16_t phase) {
  uint8_t red = phaseToIntensity(phase);
  uint8_t green = phaseToIntensity((phase + 256)%max_phase);
  uint8_t blue = phaseToIntensity((phase + 512)%max_phase);
  return(uint32_t(red) << 16 | uint32_t(green) << 8 | uint32_t(blue));
}

void Backlights::rainbowPattern() {
  // Divide by 3 to spread it out some, so the whole rainbow isn't displayed at once.
  // TODO Make this /3 a parameter
  const uint16_t phase_per_digit = (max_phase/NUM_DIGITS)/3;

  // Divide by 10 to slow down the rainbow rotation rate.
  // TODO Make this /10 a parameter
  uint16_t phase = millis()/16 % max_phase;  
  
  for (uint8_t digit=0; digit < NUM_DIGITS; digit++) {
    // Shift the phase for this LED.
    uint16_t my_phase = (phase + digit*phase_per_digit) % max_phase;
 		setPixelColor(digit, phaseToColor(my_phase));
  }
  if (dimming) {
    setBrightness((uint8_t) BACKLIGHT_DIMMED_INTENSITY);
  }  else {
    setBrightness(0xFF >> max_intensity - getLEDIntensity().value - 1);  
  }
  show();
}

void Backlights::fill(uint32_t color) {
  RgbColor rgbColor = RgbColor(color >> 16, (color >> 8) & 0xff, color & 0xff);

  for (uint8_t digit=0; digit < NUM_DIGITS; digit++) {
    pixels.SetPixelColor(digit, rgbColor);
  }
}

void Backlights::clear() {
  fill(0);
}

void Backlights::show() {
  pixels.Show();
}

void Backlights::setBrightness(uint8_t level) {
  for (uint8_t digit=0; digit < NUM_DIGITS; digit++) {
    pixels.SetPixelColor(digit, pixels.GetPixelColor(digit).Dim(level));
  }
}

void Backlights::setPixelColor(uint8_t digit, uint32_t color) {
  RgbColor rgbColor = RgbColor(color >> 16, (color >> 8) & 0xff, color & 0xff);
  pixels.SetPixelColor(digit, rgbColor);
}

const String Backlights::patterns_str[Backlights::num_patterns] = 
  { "Dark", "Test", "Constant", "Rainbow", "Pulse", "Breath" };
