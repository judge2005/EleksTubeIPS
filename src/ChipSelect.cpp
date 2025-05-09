#include "ChipSelect.h"

#ifndef HARDWARE_IPSTube_CLOCK
void ChipSelect::begin() {
  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);

  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);
  update();
}

void ChipSelect::update() {
  // Documented in README.md.  Q7 and Q6 are unused. Q5 is Seconds Ones, Q0 is Hours Tens.
  // Q7 is the first bit written, Q0 is the last.  So we push two dummy bits, then start with
  // Seconds Ones and end with Hours Tens.
  // CS is Active Low, but digits_map is 1 for enable, 0 for disable.  So we bit-wise NOT first.

  uint8_t to_shift = (~digits_map) << 2;

  digitalWrite(CSSR_LATCH_PIN, LOW);
  shiftOut(CSSR_DATA_PIN, CSSR_CLOCK_PIN, LSBFIRST, to_shift);
  digitalWrite(CSSR_LATCH_PIN, HIGH);
}
#else
const int ChipSelect::lcdEnablePins[NUM_DIGITS] = {GPIO_NUM_15, GPIO_NUM_2, GPIO_NUM_27, GPIO_NUM_14, GPIO_NUM_12, GPIO_NUM_13};

void ChipSelect::begin() {
  // Initialize all six different pins for the CS of each LCD as OUTPUT and set it to HIGH (disabled)
  for (int i = 0; i < NUM_DIGITS; ++i)
  {
    pinMode(lcdEnablePins[i], OUTPUT);
  }

  update();
}

void ChipSelect::update() {
  for (int i = 0; i < NUM_DIGITS; i++)
  {
    digitalWrite(lcdEnablePins[i], (digits_map & (1 << i)) ? LOW : HIGH);
  }
}
#endif