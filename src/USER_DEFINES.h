/*
 * Project: Alternative firmware for EleksTube IPS clock
 * Hardware: ESP32
 * File description: User preferences for the complete project
 * User defines are located in "USER_DEFINES.h"
 */


#ifndef USER_DEFINES_H_
#define USER_DEFINES_H_

//#define DEBUG_OUTPUT

// ************* Type of clock  *************
#define HARDWARE_Elekstube_CLOCK  // uncomment for the original Elekstube clock
//#define HARDWARE_SI_HAI_CLOCK  // uncomment for the SI HAI copy of the clock
//#define HARDWARE_NovelLife_SE_CLOCK  // uncomment for the NovelLife SE version; non-SE not tested
      
#define USE_CLK_FILES   // select between .CLK and .BMP images

#endif
