/*
 * Author: Aljaz Ogrin
 * Project: Alternative firmware for EleksTube IPS clock
 * Hardware: ESP32
 * File description: Hardware Global configuration for the complete project
 * Should only need editing for a new version of the clock
 * User defines are located in "USER_DEFINES.h"
 */
 
#ifndef GLOBAL_DEFINES_H_
#define GLOBAL_DEFINES_H_

#include <stdint.h>
#include <Arduino.h>

// ************ Hardware definitions *********************

// Common indexing scheme, used to identify the digit
#define NUM_DIGITS   (6)
#ifdef HARDWARE_PunkCyber_CLOCK
  #define SECONDS_ONES (5)
  #define SECONDS_TENS (4)
  #define MINUTES_ONES (3)
  #define MINUTES_TENS (2)
  #define HOURS_ONES   (1)
  #define HOURS_TENS   (0)
#else
  #define SECONDS_ONES (0)
  #define SECONDS_TENS (1)
  #define MINUTES_ONES (2)
  #define MINUTES_TENS (3)
  #define HOURS_ONES   (4)
  #define HOURS_TENS   (5)
#endif

#define SECONDS_ONES_MAP (0x01 << SECONDS_ONES)
#define SECONDS_TENS_MAP (0x01 << SECONDS_TENS)
#define MINUTES_ONES_MAP (0x01 << MINUTES_ONES)
#define MINUTES_TENS_MAP (0x01 << MINUTES_TENS)
#define HOURS_ONES_MAP   (0x01 << HOURS_ONES)
#define HOURS_TENS_MAP   (0x01 << HOURS_TENS)

#if defined(HARDWARE_IPSTube_CLOCK) || defined(HARDWARE_IPSTube_DIM_CLOCK)  // IPSTube clock pinout XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  // WS2812 (or compatible) LEDs on the back of the display modules.
  #define BACKLIGHTS_PIN (5)

  // Buttons, active low, externally pulled up (with actual resistors!)
  #define BUTTON_POWER_PIN (0)

  // 3-wire to DS1302 RTC
  #define DS1302_SCLK  (22)
  #define DS1302_IO    (19)
  #define DS1302_CE    (21)
#endif
  
#ifdef HARDWARE_SI_HAI_CLOCK  // fake chinese clock pinout XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  // WS2812 (or compatible) LEDs on the back of the display modules.
  #define BACKLIGHTS_PIN (32)

  // Buttons, active low, externally pulled up (with actual resistors!)
  #define BUTTON_LEFT_PIN (35)
  #define BUTTON_MODE_PIN (34)
  #define BUTTON_RIGHT_PIN (39)
  #define BUTTON_POWER_PIN (36)

  // 3-wire to DS1302 RTC
  #define DS1302_SCLK  (33)
  #define DS1302_IO    (25)
  #define DS1302_CE    (26)

  // Chip Select shift register, to select the display
  #define CSSR_DATA_PIN (4)
  #define CSSR_CLOCK_PIN (22)
  #define CSSR_LATCH_PIN (21)
#endif
  
#ifdef HARDWARE_NovelLife_SE_CLOCK // NovelLife_SE Clone XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  // WS2812 (or compatible) LEDs on the back of the display modules.
  #define BACKLIGHTS_PIN (12)

  // No Buttons on SE verion -- gesture sensor not included in code 

  // I2C to DS3231 RTC.
  #define RTC_SCL_PIN (22)
  #define RTC_SDA_PIN (21)

  // Chip Select shift register, to select the display
  #define CSSR_DATA_PIN (14)  
  #define CSSR_CLOCK_PIN (13) // SHcp changed from IO16 in original Elekstube
  #define CSSR_LATCH_PIN (15) // STcp was IO17 in original Elekstube
#endif

#ifdef HARDWARE_Elekstube_CLOCK  //  original EleksTube IPS clock XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
  // WS2812 (or compatible) LEDs on the back of the display modules.
  #define BACKLIGHTS_PIN (12)

  // Buttons, active low, externally pulled up (with actual resistors!)
  #define BUTTON_LEFT_PIN (33)
  #define BUTTON_MODE_PIN (32)
  #define BUTTON_RIGHT_PIN (35)
  #define BUTTON_POWER_PIN (34)

  // I2C to DS3231 RTC.
  #define RTC_SCL_PIN (22)
  #define RTC_SDA_PIN (21)

  // Chip Select shift register, to select the display
  #define CSSR_DATA_PIN (14)
  #define CSSR_CLOCK_PIN (16)
  #define CSSR_LATCH_PIN (17)
#endif

#ifdef HARDWARE_Elekstube_CLOCK_V2  //  EleksTube IPS clock V2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
// Please note that the V2 hardware is locked. You can't install new firmware short of replacing the ESP32 Pico with a new one
  // WS2812 (or compatible) LEDs on the back of the display modules.
  #define BACKLIGHTS_PIN (12)

  // Buttons, active low, externally pulled up (with actual resistors!)
  #define BUTTON_LEFT_PIN (33)
  #define BUTTON_MODE_PIN (32)
  #define BUTTON_RIGHT_PIN (35)
  #define BUTTON_POWER_PIN (34)

  // I2C to DS3231 RTC.
  #define RTC_SCL_PIN (22)
  #define RTC_SDA_PIN (21)

  // Chip Select shift register, to select the display
  #define CSSR_DATA_PIN (14)
  #define CSSR_CLOCK_PIN (9) // SD2
  #define CSSR_LATCH_PIN (10) // SD3
#endif

#endif /* GLOBAL_DEFINES_H_ */
