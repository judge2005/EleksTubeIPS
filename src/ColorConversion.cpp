#include "ColorConversion.h"

uint16_t hsv2rgb565(uint8_t h, uint8_t s, uint8_t v)
{
  uint16_t r = 0;
  uint16_t g = 0;
  uint16_t b = 0;

  // this is the algorithm to convert from RGB to HSV
  h = (((uint16_t)h) * 192) / 256; // 0..191
  unsigned int i = h / 32;         // We want a value of 0 thru 5
  unsigned int f = (h % 32) * 8;   // 'fractional' part of 'i' 0..248 in jumps

  unsigned int sInv = 255 - s; // 0 -> 0xff, 0xff -> 0
  unsigned int fInv = 255 - f; // 0 -> 0xff, 0xff -> 0
  uint8_t pv = v * sInv / 256; // pv will be in range 0 - 255
  uint8_t qv = v * (255 - s * f / 256) / 256;
  uint8_t tv = v * (255 - s * fInv / 256) / 256;

  switch (i)
  {
  case 0:
    r = v;
    g = tv;
    b = pv;
    break;
  case 1:
    r = qv;
    g = v;
    b = pv;
    break;
  case 2:
    r = pv;
    g = v;
    b = tv;
    break;
  case 3:
    r = pv;
    g = qv;
    b = v;
    break;
  case 4:
    r = tv;
    g = pv;
    b = v;
    break;
  case 5:
    r = v;
    g = pv;
    b = qv;
    break;
  }

  g &= 0xFC;
  r &= 0xF8;

  return (r << 8) | (g << 3) | (b >> 3);
}

