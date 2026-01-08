#include "ChipSelect.h"

#if !defined(HARDWARE_IPSTube_CLOCK) && !defined(HARDWARE_IPSTube_DIM_CLOCK)
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