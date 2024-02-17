#ifndef TFTS_H
#define TFTS_H

// Call up the LittleFS FLASH filing system this is part of the ESP Core
#define FS_NO_GLOBALS
#include <FS.h>
#include <TFT_eSPI.h>
#include "ChipSelect.h"
#include "DigitalRainAnimation.h"

class StaticSprite : public TFT_eSprite {
public:
  StaticSprite(TFT_eSPI *tft) : TFT_eSprite(tft) {}
  void pushImageWithTransparency(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t transparentColor = 0);
  void init();

  static uint8_t output_buffer[];
};

class TFTs : public TFT_eSPI {
public:
  TFTs() : TFT_eSPI(), chip_select(), enabled(false)
    {}

  // no == Do not send to TFT. yes == Send to TFT if changed. force == Send to TFT.
  enum show_t { no, yes, force };
  enum image_justification_t { TOP_LEFT, TOP_CENTER, TOP_RIGHT, MIDDLE_LEFT, MIDDLE_CENTER, MIDDLE_RIGHT, BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT };

  void claim();
  void release();

  void begin(fs::FS& fs);
  void clear();
  void setShowDigits(bool);

  void setDigit(uint8_t digit, const char* name, show_t show=yes);

  void invalidateAllDigits();

  void showAllDigits()               { for (uint8_t digit=0; digit < NUM_DIGITS; digit++) showDigit(digit); }
  void showDigit(uint8_t digit);
  TFT_eSprite& drawImage(uint8_t digit);
  StaticSprite& getSprite();

  void animateRain();

  void setImageJustification(image_justification_t value) { imageJustification = value; }
  void setBox(uint16_t w, uint16_t h) { boxWidth = w; boxHeight = h; }
  // Controls the power to all displays
  void enableAllDisplays()           { digitalWrite(TFT_ENABLE_PIN, HIGH); enabled = true; }
  void disableAllDisplays()          { digitalWrite(TFT_ENABLE_PIN, LOW); enabled = false; }
  void toggleAllDisplays()           { if (enabled) disableAllDisplays(); else enableAllDisplays(); }
  bool isEnabled()                   { return enabled; }
  void setDimming(uint8_t dimming);
  void setBrightness(uint8_t lvl);

  // Making chip_select public so we don't have to proxy all methods, and the caller can just use it directly.
  ChipSelect chip_select;

  void setStatus(const char*);
  void setStatus(const String& s) { setStatus(s.c_str()); }
  void checkStatus();
  void printlnAll(const char* s);
  void printAll(const char* s);
  void drawMeter(int val, bool first, const char *legend);

  uint16_t dimColor(uint16_t pixel);
  void setMonochromeColor(int color);

private:
  static SemaphoreHandle_t tftMutex;

  TFT_eSprite& getStatusSprite();
#ifdef SMOOTH_FONT
  DigitalRainAnim& getMatrixAnimator();
#else
  DigitalRainAnimation& getMatrixAnimator();
#endif
  void drawStatus();

  bool showDigits = true;
  image_justification_t imageJustification = MIDDLE_CENTER;
  uint16_t boxWidth = TFT_WIDTH;
  uint16_t boxHeight = TFT_HEIGHT;
  uint8_t dimming = 255; // amount of dimming graphics
  int monochromeColor = -1;

  unsigned long statusTime = 0;
  bool statusSet = false;

  uint16_t defaultTextColor = TFT_WHITE;
  uint16_t defaultTextBackground = TFT_BLACK;
  uint8_t current_graphic = 1;
  char icons[NUM_DIGITS][16];
  char loadedFilename[255];

  bool enabled;
  fs::FS* fs;

  bool LoadImageIntoBuffer(const char* filename);
  bool LoadBMPImageIntoBuffer(fs::File &file);
  bool LoadCLKImageIntoBuffer(fs::File &file);
  bool LoadImageBytesIntoSprite(int16_t w, int16_t h, uint8_t bpp, int16_t rowSize, bool reversed, uint32_t *palette, fs::File &file);

  uint16_t read16(fs::File &f);
  uint32_t read32(fs::File &f);

  uint8_t FileInBuffer=255; // invalid, always load first image
  uint8_t NextFileRequired = 0; 
};

extern TFTs *tfts;


#endif // TFTS_H
