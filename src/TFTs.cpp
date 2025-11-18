#include "TFTs.h"
#include <WiFi.h>
#include "matrix-code-14.h"
#include <byteswap.h>
#define MATRIX_FONT &matrix_code_nfi14pt7b

#define DARKER_GREY 0x18E3

// Clipping macro for pushImage
#define PI_CLIP                                        \
  if (_vpOoB) return;                                  \
  x+= _xDatum;                                         \
  y+= _yDatum;                                         \
                                                       \
  if ((x >= _vpW) || (y >= _vpH)) return;              \
                                                       \
  int32_t dx = 0;                                      \
  int32_t dy = 0;                                      \
  int32_t dw = w;                                      \
  int32_t dh = h;                                      \
                                                       \
  if (x < _vpX) { dx = _vpX - x; dw -= dx; x = _vpX; } \
  if (y < _vpY) { dy = _vpY - y; dh -= dy; y = _vpY; } \
                                                       \
  if ((x + dw) > _vpW ) dw = _vpW - x;                 \
  if ((y + dh) > _vpH ) dh = _vpH - y;                 \
                                                       \
  if (dw < 1 || dh < 1) return;

SemaphoreHandle_t TFTs::tftMutex = 0;

uint8_t StaticSprite::output_buffer[(TFT_HEIGHT * TFT_WIDTH + 1) * sizeof(uint16_t)];

void StaticSprite::pushImageWithTransparency(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t transparentColor)
{
  if (data == nullptr) return;

  PI_CLIP;

  // Pointer within original image
  uint16_t *ptro = data + dx + dy * w;
  // Pointer within sprite image
  uint16_t *ptrs = _img + x + y * _iwidth;

  uint16_t oPixel;

  if(_swapBytes)
  {
    while (dh--)
    {
      for (int pixelX = 0; pixelX < dw; pixelX++) {
        oPixel = ptro[pixelX];
        if (oPixel != transparentColor) {
          ptrs[pixelX] = __bswap_16(oPixel);
        }
      }
      ptro += w;
      ptrs += _iwidth;
    }
  }
  else
  {
    while (dh--)
    {
      for (int pixelX = 0; pixelX < dw; pixelX++) {
        oPixel = ptro[pixelX];
        if (oPixel != transparentColor) {
          ptrs[pixelX] = oPixel;
        }
      }
      ptro += w;
      ptrs += _iwidth;
    }
  }
}

void StaticSprite::pushImageWithAlpha(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint8_t *alpha, uint8_t opaque)
{
  if (data == nullptr) return;

  PI_CLIP;

  // Pointer within original image
  uint16_t *ptro = data + dx + dy * w;
  uint8_t *ptra = alpha + dx + dy * w;
  // Pointer within sprite image
  uint16_t *ptrs = _img + x + y * _iwidth;

  uint16_t oPixel;
  uint8_t alphaValue;

  if(_swapBytes)
  {
    while (dh--)
    {
      for (int pixelX = 0; pixelX < dw; pixelX++) {
        oPixel = ptro[pixelX];
        alphaValue = ptra[pixelX];
        if (alphaValue == opaque) {
          ptrs[pixelX] = __bswap_16(oPixel);
        } else if (alphaValue != 0) {
          fastBlend(alphaValue, __bswap_16(oPixel), ptrs[pixelX]);
        }
      }
      ptro += w;
      ptrs += _iwidth;
    }
  }
  else
  {
    while (dh--)
    {
      for (int pixelX = 0; pixelX < dw; pixelX++) {
        oPixel = ptro[pixelX];
        alphaValue = ptra[pixelX];
        if (alphaValue == opaque) {
          ptrs[pixelX] = oPixel;
        } else if (alphaValue != 0) {
          fastBlend(alphaValue, oPixel, ptrs[pixelX]);
        }
      }
      ptro += w;
      ptrs += _iwidth;
    }
  }
}

void StaticSprite::init() {
  _iwidth  = _dwidth  = _bitwidth = TFT_WIDTH;
  _iheight = _dheight = TFT_HEIGHT;

  cursor_x = 0;
  cursor_y = 0;
  setTextWrap(true, true);

  // Default scroll rectangle and gap fill colour
  _sx = 0;
  _sy = 0;
  _sw = TFT_WIDTH;
  _sh = TFT_HEIGHT;
  _scolor = TFT_BLACK;

  _img8   = output_buffer;
  _img8_1 = _img8;
  _img8_2 = _img8;
  _img    = (uint16_t*) _img8;
  _img4   = _img8;

  _created = true;

  rotation = 0;
  setViewport(0, 0, _dwidth, _dheight);
  setPivot(_iwidth/2, _iheight/2);
}

void TFTs::claim() {
  xSemaphoreTake(tftMutex, portMAX_DELAY);
}

void TFTs::release(){
  xSemaphoreGive(tftMutex);
}

#ifdef SMOOTH_FONT
DigitalRainAnim& TFTs::getMatrixAnimator() {
  static DigitalRainAnim *animator = NULL;

  if (animator == NULL) {
    animator = new DigitalRainAnim();
    animator->init(this);
  }

  return *animator;
}
#else
DigitalRainAnimation& TFTs::getMatrixAnimator() {
  static DigitalRainAnimation *animator = NULL;

  if (animator == NULL) {
    animator = new DigitalRainAnimation();
    animator->init(&getSprite());
  }

  return *animator;
}
#endif

StaticSprite& TFTs::getSprite() {
  static bool initialized = false;
  static StaticSprite *sprite;

  if (!initialized) {
    initialized = true;
    sprite = new StaticSprite(tfts);
    sprite->init();
  }

  return *sprite;
}

TFT_eSprite& TFTs::getStatusSprite() {
  static bool initialized = false;
  static TFT_eSprite *sprite;

  if (!initialized) {
    initialized = true;

    sprite = new StaticSprite(tfts);
    sprite->createSprite(width(), 16);

    sprite->setTextDatum(BC_DATUM);
    sprite->setTextColor(TFT_GOLD, TFT_BLACK, true);
    sprite->setTextFont(2);
  }

  return *sprite;
}

void TFTs::checkStatus() {
  if (millis() - statusTime > 5000) {
    if (statusSet)  {
      statusSet = false;
      invalidateAllDigits();
    }
  }
}

void TFTs::setBrightness(uint8_t lvl) {
  claim();
  uint8_t saved = chip_select.getDigitMap();
  chip_select.setAll();

  // Doesn't work
  tfts->writecommand(0x51);
  tfts->writedata(lvl);


  chip_select.setDigitMap(saved, true);
  release();
}

void TFTs::setStatus(const char* s) {
  claim();

  statusTime = millis();
  statusSet = true;
  TFT_eSprite& sprite = getStatusSprite();
  int16_t h = sprite.height();
  sprite.fillRect(0, 0, width(), h, TFT_BLACK);
  sprite.drawString(s, width()/2, h);

  uint8_t saved = chip_select.getDigitMap();
  chip_select.setAll();

  drawStatus();

  chip_select.setDigitMap(saved, true);

  release();
}

void TFTs::printlnAll(const char* s) {
  claim();

  uint8_t saved = chip_select.getDigitMap();
  chip_select.setAll();

  if (getCursorY() >= height() - 10) {
    setCursor(0,0);
    fillRect(0, 0, width(), height(), TFT_BLACK);
  }
  println(s);

  chip_select.setDigitMap(saved, true);

  release();
}

void TFTs::printAll(const char* s) {
  claim();

  uint8_t saved = chip_select.getDigitMap();
  chip_select.setAll();

  if (getCursorY() >= height() - 10) {
    setCursor(0,0);
    fillRect(0, 0, width(), height(), TFT_BLACK);
  }
  print(s);

  chip_select.setDigitMap(saved, true);

  release();
}

void TFTs::drawStatus() {
  if (!enabled) {
    return;
  }
  
  if (statusSet) {
#ifdef USE_DMA
  while(dmaBusy()) {
    delay(1);
  }
#endif
    TFT_eSprite& sprite = getStatusSprite();
    sprite.pushSprite(0, height() - sprite.height());
  }
}

void TFTs::animateRain() {
#ifdef SMOOTH_FONT
  DigitalRainAnim animator = getMatrixAnimator();
#else
  DigitalRainAnimation& animator = getMatrixAnimator();
#endif

  if (animator.loop()) {
    claim();

    uint8_t saved = chip_select.getDigitMap();
    chip_select.setAll();

  #ifndef SMOOTH_FONT
    TFT_eSprite& sprite = getSprite();
    sprite.setFreeFont(MATRIX_FONT);                   // Select the font
  #endif
    animator.animate(dimming);
  #ifndef SMOOTH_FONT
    sprite.pushSprite(0,0);
  #endif
    drawStatus();

    chip_select.setDigitMap(saved, true);

    release();
  }
}

void TFTs::drawMeter(int val, bool first, const char *legend) {
  if (!enabled) {
    return;
  }

  static uint16_t last_angle = 30;

  int x = width() / 2;
  int y = height() / 2;
  int r = width() / 2;

  claim();
  uint8_t saved = chip_select.getDigitMap();

  chip_select.setAll();

  if (first) {
    last_angle = 30;
    fillCircle(x, y, r, DARKER_GREY);
    drawSmoothCircle(x, y, r, TFT_SILVER, DARKER_GREY);
    uint16_t tmp = r - 3;
    drawArc(x, y, tmp, tmp - tmp / 5, last_angle, 330, TFT_BLACK, DARKER_GREY);
  }

  r -= 3;

  // Range here is 0-100 so value is scaled to an angle 30-330
  int val_angle = map(val, 0, 100, 30, 330);


  if (last_angle != val_angle) {
    char str_buf[8];         // Buffed for string
    itoa (val, str_buf, 10); // Convert value to string (null terminated)

    setTextDatum(MC_DATUM);
    setTextColor(TFT_WHITE, DARKER_GREY, true);
    setTextFont(4);
    drawString(str_buf, x, y);

    setTextDatum(TC_DATUM);
    setTextColor(TFT_GOLD, DARKER_GREY, true);
    setTextFont(2);
    drawString(legend, x, y + (r * 0.4));

    setTextColor(defaultTextColor, defaultTextBackground, true);

    // Allocate a value to the arc thickness dependant of radius
    uint8_t thickness = r / 5;
    if ( r < 25 ) thickness = r / 3;

    // Update the arc, only the zone between last_angle and new val_angle is updated
    if (val_angle > last_angle) {
      drawArc(x, y, r, r - thickness, last_angle, val_angle, TFT_SKYBLUE, TFT_BLACK); // TFT_SKYBLUE random(0x10000)
    }
    else {
      drawArc(x, y, r, r - thickness, val_angle, last_angle, TFT_BLACK, DARKER_GREY);
    }
    last_angle = val_angle; // Store meter arc position for next redraw
  }

  chip_select.setDigitMap(saved, true);

  release();
}

void TFTs::setShowDigits(bool show) {
  showDigits = show;
}

void TFTs::setDimming(uint8_t dimming) {
  if (dimming != this->dimming) {
#ifdef DIM_WITH_ENABLE_PIN_PWM
#if (TFT_ENABLE_VALUE == 1)
  ledcWrite(TFT_PWM_CHANNEL, dimming);
#else
  ledcWrite(TFT_PWM_CHANNEL, 255 - dimming);
#endif
#else
    invalidateAllDigits();
#endif
    this->dimming = dimming;
  }
}

void TFTs::invalidateAllDigits() {
  loadedFilename[0] = 0;
  for (uint8_t digit=0; digit < NUM_DIGITS; digit++) {
    setDigit(digit, "nosuchfile", TFTs::no);
  }
}

void TFTs::begin(fs::FS& fs) {
  if (tftMutex == 0) {
    tftMutex = xSemaphoreCreateMutex();
  }

  this->fs = &fs;
  
  // Start with all displays selected.
  chip_select.begin();
  chip_select.setAll();
  writecommand(1);

  // Initialize the super class.
  init();

#ifdef USE_DMA
  initDMA();
#endif
  
  // Clear all displays
  fillScreen(TFT_BLACK);

  invalidateAllDigits();

  // Turn power on to displays.
#ifdef DIM_WITH_ENABLE_PIN_PWM
  // If hardware dimming is used, init ledc, set the pin and channel for PWM and set frequency and resolution
  ledcSetup(TFT_ENABLE_PIN, TFT_PWM_FREQ, TFT_PWM_RESOLUTION);            // PWM, globally defined
  ledcAttachPin(TFT_ENABLE_PIN, TFT_PWM_CHANNEL);                         // Attach the pin to the PWM channel
  ledcChangeFrequency(TFT_PWM_CHANNEL, TFT_PWM_FREQ, TFT_PWM_RESOLUTION); // need to set the frequency and resolution again to have the hardware dimming working properly
#else
  pinMode(TFT_ENABLE_PIN, OUTPUT); // Set pin for turning display power on and off.
#endif
  enableAllDisplays();
}

void TFTs::enableAllDisplays() {
#ifdef DIM_WITH_ENABLE_PIN_PWM
#if (TFT_ENABLE_VALUE == 1)
  ledcWrite(TFT_PWM_CHANNEL, dimming);
#else
  ledcWrite(TFT_PWM_CHANNEL, 255 - dimming);
#endif
#else
#if defined(HARDWARE_IPSTube_CLOCK)
  invalidateAllDigits();
#endif
  digitalWrite(TFT_ENABLE_PIN, TFT_ENABLE_VALUE);
#endif
  enabled = true;
}

void TFTs::disableAllDisplays() {
#if defined(DIM_WITH_ENABLE_PIN_PWM)
#if (TFT_ENABLE_VALUE == 1)
  ledcWrite(TFT_PWM_CHANNEL, 0);
#else
  ledcWrite(TFT_PWM_CHANNEL, 255);
#endif
#else
  digitalWrite(TFT_ENABLE_PIN, TFT_DISABLE_VALUE);
#if defined(HARDWARE_IPSTube_CLOCK)
// On some models of IPSTube clock the display can't be turned off
  chip_select.setAll();
  fillScreen(TFT_BLACK);
#endif
#endif
  enabled = false;
}

void TFTs::clear() {
  // Start with all displays selected.
  chip_select.setAll();
  enableAllDisplays();
}

void TFTs::setDigit(uint8_t digit, const char *name, show_t show) {
  bool changed = strcmp(icons[digit], name) != 0;
  strcpy(icons[digit], name);
  
  if (show != no && (changed || show == force)) {
    showDigit(digit);
  }
}

/* 
 * Displays the bitmap for the value to the given digit. 
 */
void TFTs::showDigit(uint8_t digit) {
  if (!enabled) {
    return;
  }

  if (*icons[digit] == 0) {
#ifdef USE_DMA
    while(dmaBusy()) {
      delay(1);
    }
#endif
    chip_select.setDigit(digit);
    fillScreen(TFT_BLACK);
    drawStatus();
  } else {
    unsigned long start = millis();
    TFT_eSprite& sprite = drawImage(digit);
    // Serial.printf("Draw image took %d ms\n", millis() - start);
#ifndef USE_DMA
    sprite.pushSprite(0,0);
#endif
    drawStatus();
  }
}

uint8_t calc_shift(uint16_t mask) {
    uint8_t count = 0;
    if ((mask & 0xFFFF) == 0) {
        count += 16;
        mask >>= 16;
    }
    if ((mask & 0xFF) == 0) {
        count += 8;
        mask >>= 8;
    }
    if ((mask & 0xF) == 0) {
        count += 4;
        mask >>= 4;
    }
    if ((mask & 0x3) == 0) {
        count += 2;
        mask >>= 2;
    }
    if ((mask & 0x1) == 0) {
        count += 1;
    }
    return count;
}

uint16_t rotate_right(uint16_t value, int count) {
  return (value >> count) | (value << (16 - count));
}

// These BMP functions are stolen directly from the TFT_SPIFFS_BMP example in the TFT_eSPI library.
// Unfortunately, they aren't part of the library itself, so I had to copy them.
// I've modified DrawImage to buffer the whole image at once instead of doing it line-by-line.
bool TFTs::LoadBMPImageIntoBuffer(fs::File &bmpFile) {
  uint32_t bmpStart, headerSize, paletteSize = 0;
  int16_t w, h, row, col;
  uint16_t  r, g, b, bitDepth;
  
  // First two bytes should already have been read
  bmpFile.seek(8, fs::SeekCur); // Skip file size and a reserved word
  bmpStart = read32(bmpFile); // start of bitmap
  headerSize = read32(bmpFile); // header size
  w = read32(bmpFile); // width
  h = read32(bmpFile); // height
  uint16_t planes = read16(bmpFile); // color planes (must be 1)
  if (planes != 1) {
    Serial.print("Bad color planes: ");
    Serial.println(planes);
    return false;
  }

  bitDepth = read16(bmpFile);
  if (bitDepth != 24 && bitDepth != 16 && bitDepth != 1 && bitDepth != 4 && bitDepth != 2 && bitDepth != 8) {
    Serial.print("Bad bit depth: ");
    Serial.println(bitDepth);
    return false;
  }

  int32_t compression = read32(bmpFile);

#ifdef DEBUG_OUTPUT
  Serial.print("image W, H, BPP: ");
  Serial.print(w); 
  Serial.print(", "); 
  Serial.print(h);
  Serial.print(", "); 
  Serial.println(bitDepth);
  Serial.print("dimming: ");
  Serial.println(dimming);
#endif
  uint32_t palette[256];
  MaskData maskData;
  if (bitDepth <= 8) // 1,2,4,8 bit bitmap: read color palette
  {
    read32(bmpFile); read32(bmpFile); read32(bmpFile); // size, w resolution, h resolution
    paletteSize = read32(bmpFile);
    if (paletteSize == 0) paletteSize = 1 << bitDepth; // if 0, size is 2^bitDepth
    if (compression == 3) {
      bmpFile.seek(14 + 12 + headerSize); // start of color palette
    } else if (compression == 6) {
      bmpFile.seek(14 + 16 + headerSize); // start of color palette
    } else {
      bmpFile.seek(14 + headerSize); // start of color palette
    }
    for (uint16_t i = 0; i < paletteSize; i++) {
      palette[i] = read32(bmpFile);
    }
  } else if (bitDepth == 16) {
    // Seek past some data
    bmpFile.seek(20, fs::SeekCur);
    if (bmpFile.position() != bmpStart) {
      maskData.rMask = read32(bmpFile);
      maskData.rShift = (calc_shift(maskData.rMask) - (5 - __builtin_popcount(maskData.rMask))) % 16;
      maskData.gMask = read32(bmpFile);
      maskData.gShift = (calc_shift(maskData.gMask) - (6 - __builtin_popcount(maskData.gMask))) % 16;
      maskData.bMask = read32(bmpFile);
      maskData.bShift = (calc_shift(maskData.bMask) - (5 - __builtin_popcount(maskData.rMask))) % 16;
      if (maskData.gMask != 0x07e0) {  // 0x07e0 is the green mask for RGB565
        // We have either ARGB4444 or ARGB1555
        maskData.aMask = read32(bmpFile);
        maskData.aShift = calc_shift(maskData.aMask);
      }
#ifdef notdef
      Serial.printf("rMask=%d, rShift=%d, gMask=%d, gShift=%d, bMask=%d, bShift=%d, aMask=%d, aShift=%d\n",
        maskData.rMask, maskData.rShift, maskData.gMask, maskData.gShift, maskData.bMask, maskData.bShift, maskData.aMask, maskData.aShift
      );
#endif
    }
  }

  bmpFile.seek(bmpStart);

  return LoadImageBytesIntoSprite(w, abs(h), bitDepth, ((bitDepth * w +31) >> 5) * 4, h > 0, &maskData, palette, bmpFile);
}

bool TFTs::LoadCLKImageIntoBuffer(fs::File &clkFile) {
  int16_t w, h;

  // First two bytes should already have been read
  w = read16(clkFile);
  h = read16(clkFile);
#ifdef DEBUG_OUTPUT
  Serial.print("image W, H: ");
  Serial.print(w); 
  Serial.print(", "); 
  Serial.println(h);
  Serial.print("dimming: ");
  Serial.println(dimming);
#endif

  MaskData maskData;

  return LoadImageBytesIntoSprite(w, h, 16, w * 2, false, &maskData, 0, clkFile);
}

uint16_t TFTs::dimColor(uint16_t color) {
#ifdef DIM_WITH_ENABLE_PIN_PWM
  return color;
#else
  if (dimming == 255) {
    return color;
  }

#define TFT_RED         0xF800      /* 255,   0,   0 */

  // 16 BPP pixel format: R5, G6, B5 ; bin: RRRR RGGG GGGB BBBB
  // align to 8-bit value (MSB left aligned)
  uint16_t r, g, b;

  r = color >> 11;
  g = (color >> 5) & 0x3f;
  b = color & 0x1f;

  r *= dimming;
  g *= dimming;
  b *= dimming;
  r = r >> 8;
  g = g >> 8;
  b = b >> 8;

  return (r << 11) | (g << 5) | b;
#endif
}

void TFTs::setMonochromeColor(int color) {
  monochromeColor = color;
}

bool TFTs::LoadImageBytesIntoSprite(int16_t w, int16_t h, uint8_t bitDepth, int16_t rowSize, bool reversed, MaskData *pMaskData, uint32_t *palette, fs::File &file) {
  // Calculate top left coords of box - default to MIDDLE_CENTER
  int16_t x = (TFT_WIDTH - boxWidth) / 2;
  int16_t y = (TFT_HEIGHT - boxHeight) / 2;

  switch (imageJustification) {
    case TOP_LEFT: x = y = 0; break;
    case TOP_CENTER: y = 0; break;
    case TOP_RIGHT: x = (TFT_WIDTH - boxWidth); y = 0; break;
    case MIDDLE_LEFT: x = 0; break;
    case MIDDLE_RIGHT: x = (TFT_WIDTH - boxWidth); break;
    case BOTTOM_LEFT: x = 0; y = (TFT_HEIGHT - boxHeight); break;
    case BOTTOM_CENTER: y = (TFT_HEIGHT - boxHeight); break;
    case BOTTOM_RIGHT: x = (TFT_WIDTH - boxWidth); y = (TFT_HEIGHT - boxHeight); break;
  }

  // Center image in box
  int16_t imgXOffset = (boxWidth - w) / 2;
  int16_t imgYOffset = (boxHeight - h) / 2;

  x += imgXOffset;
  y += imgYOffset;

  uint32_t outputBufferSize = w * 2;
  if (outputBufferSize < rowSize) {
    outputBufferSize = rowSize; // So that input and output buffer can be the same. Basically for bitDepth >= 16
  }
  uint8_t outputBuffer[outputBufferSize];
  uint8_t alphaBuffer[w];

  uint32_t inputBufferSize = rowSize;
  uint8_t *inputBuffer = outputBuffer;

  if (bitDepth < 16) {
    inputBuffer = (uint8_t*)malloc(inputBufferSize);
  }

#ifdef notdef  
  Serial.print("image W, H: ");
  Serial.print(w); 
  Serial.print(", "); 
  Serial.println(h);
  Serial.print("dimming: ");
  Serial.println(dimming);
  Serial.print("input size, output size: ");
  Serial.print(inputBufferSize); 
  Serial.print(", "); 
  Serial.println(outputBufferSize);
#endif
  uint8_t opaque = rotate_right(pMaskData->aMask, pMaskData->aShift);
  if (bitDepth == 1 && monochromeColor >= 0) {
    opaque = 1;
  }
  StaticSprite& sprite = getSprite();
  bool leaveBackground = opaque != 0;
  leaveBackground = leaveBackground || (w == TFT_WIDTH && h == TFT_HEIGHT);
  if (!leaveBackground) {
    sprite.fillSprite(0);
  }
  
  bool oldSwapBytes = sprite.getSwapBytes();
  sprite.setSwapBytes(true);

  // 0,0 coordinates are top left
  int spriteRow = 0;
#ifdef USE_DMA
  int dmaBufRows = 0;
  spriteRow = h + imgYOffset;
  if (h != sprite.height()) {
      while (dmaBusy()) {
        delay(1);
      }
      pushImageDMA(0, spriteRow, sprite.width(), sprite.height() - spriteRow, (uint16_t const*)(&StaticSprite::output_buffer[sprite.width() * spriteRow * 2]));
  }
#endif
  for (int row = 0; row < h; row++) {
    size_t read = file.read(inputBuffer, inputBufferSize);
    if (read != inputBufferSize) {
      Serial.println("Bytes read, bytes asked for:");
      Serial.print(read);
      Serial.print(", ");
      Serial.println(inputBufferSize);
      break;
    }
    
    // Colors are already in 16-bit R5, G6, B5 format
#ifdef DIM_WITH_ENABLE_PIN_PWM
    if (bitDepth != 16 || pMaskData->aMask != 0) {
#else
    if (dimming != 255 || bitDepth != 16 || pMaskData->aMask != 0) {
#endif
      uint8_t*  inputPtr = inputBuffer;

      for (int col = 0; col < w; col++)
      {
        uint16_t r, g, b;
        uint32_t c = 0;
        int pixel = 0;

        switch (bitDepth) {
          case 32:
            inputPtr++;
          case 24:
            b = *inputPtr++;
            g = *inputPtr++;
            r = *inputPtr++;
            break;
          case 16:
            {
              uint16_t pix;
              ((uint8_t *)&pix)[0] = inputBuffer[col*2]; // LSB
              ((uint8_t *)&pix)[1] = inputBuffer[col*2+1]; // MSB

              // align to 8-bit value (MSB left aligned)
              r = rotate_right((pix & pMaskData->rMask), pMaskData->rShift);
              g = rotate_right((pix & pMaskData->gMask), pMaskData->gShift);
              b = rotate_right((pix & pMaskData->bMask), pMaskData->bShift);
              if (opaque != 0) {
                alphaBuffer[col] = rotate_right((pix & pMaskData->aMask), pMaskData->aShift) * 255 / opaque;
              }
            }
            break;
          case 8:
            c = palette[*inputPtr++];
            b = (c >> 3) & 0x1f; g = (c >> 10) & 0x3f; r = (c >> 19) & 0x1f;
            // b = c & 0xff; g = (c >> 8) & 0xff; r = (c >> 16) & 0xff;
            break;
          case 4:
            c = palette[(*inputPtr >> ((col & 0x01)?0:4)) & 0x0F];
            if (col & 0x01) inputPtr++;
            b = (c >> 3) & 0x1f; g = (c >> 10) & 0x3f; r = (c >> 19) & 0x1f;
            break;
          case 2:
            pixel = (*inputPtr >> ((3 - (col & 0x03))<< 1)) & 0x03;
            if ((col & 0x03) == 0x03) inputPtr++;
            c = palette[pixel];
            b = (c >> 3) & 0x1f; g = (c >> 10) & 0x3f; r = (c >> 19) & 0x1f;
            break;
          case 1:
            pixel = (*inputPtr >> (7 - (col & 0x07))) & 0x01;
            if ((col & 0x07) == 0x07) inputPtr++;
            int oneBitColor = dimColor(monochromeColor);
            alphaBuffer[col] = 255;
            if (oneBitColor >= 0 && pixel == 1) {
              r = (oneBitColor >> 11) & 0x1f;
              b = (oneBitColor) & 0x1f;
              g = (oneBitColor >> 5) & 0x3f;
            } else {
              c = palette[pixel];
              b = (c >> 3) & 0x1f; g = (c >> 10) & 0x3f; r = (c >> 19) & 0x1f;
              if (b == 0 && g == 0 && r == 0) {
                alphaBuffer[col] = 0;
              }
            }
            break;
        }
#ifndef DIM_WITH_ENABLE_PIN_PWM        
        if (dimming != 255 && bitDepth != 1) {
          r *= dimming;
          g *= dimming;
          b *= dimming;
          r = r >> 8;
          g = g >> 8;
          b = b >> 8;
        }
#endif
        // convert to RGB565
        uint16_t finalPixel = (r << 11) | (g << 5) | b;
        outputBuffer[col*2+1] = finalPixel >> 8;
        outputBuffer[col*2] = finalPixel & 0xff;
      }
    }

    int spriteRow = reversed ? (h-row-1) + y : row + y;
    if (opaque != 0) {
      sprite.pushImageWithAlpha(x, spriteRow, w, 1, (uint16_t*)outputBuffer, alphaBuffer, 255);
    } else {
      sprite.pushImage(x, spriteRow, w, 1, (uint16_t*)outputBuffer);
    }
#ifdef USE_DMA
    dmaBufRows++;
    if (dmaBufRows == 10) {
      while (dmaBusy()) {
        delay(1);
      }
      pushImageDMA(0, spriteRow, sprite.width(), dmaBufRows, (uint16_t const*)(&StaticSprite::output_buffer[sprite.width() * spriteRow * 2]));
      dmaBufRows = 0;
    }
#endif
  }

#ifdef USE_DMA
  if (dmaBufRows != 0 || spriteRow != 0) {
      while (dmaBusy()) {
        delay(1);
      }
      pushImageDMA(0, 0, sprite.width(), spriteRow + dmaBufRows, (uint16_t const*)(&StaticSprite::output_buffer[0]));
  }
#endif

  sprite.setSwapBytes(oldSwapBytes);

  if (bitDepth < 16) {
    free(inputBuffer);
  }

  return true;
}

bool TFTs::LoadImageIntoBuffer(const char* filename) {
  bool loaded = false;

  if (fs->exists(filename)) {
    fs::File file;
    file = fs->open(filename, "r");
    if (file) {
      uint16_t magic = read16(file);
 
      if (magic == 0x4B43) { // look for "CK" header
        loaded = LoadCLKImageIntoBuffer(file);
      }

      if (magic == 0x4D42) {
        loaded = LoadBMPImageIntoBuffer(file);
      }

      file.close();

      strcpy(loadedFilename, filename);
    }
  }

  if (!loaded) {
    getSprite().fillSprite(0);
  }
  return false;
}

TFT_eSprite& TFTs::drawImage(uint8_t digit) {
#ifdef DEBUG_OUTPUT
  uint32_t StartTime = millis();
#endif
  char filename[255];

  yield();

#ifdef USE_DMA
  while(dmaBusy()) {
    delay(1);
  }
#endif
  chip_select.setDigit(digit);

  if (showDigits) {
    strcpy(filename, "/ips/cache/");
  } else {
    strcpy(filename, "/ips/weather_cache/");
  }
  strcat(filename, icons[digit]);
  strcat(filename, ".bmp");
  
  // check if file is already loaded into buffer; skip loading if it is. Saves 50 to 150 msec of time.
  // if (strcmp(loadedFilename, filename) != 0 || !showDigits) {
    LoadImageIntoBuffer(filename);
  // }
#ifdef USE_DMA
  else {
    TFT_eSprite& sprite = getSprite();
    pushImageDMA(0, 0, sprite.width(), sprite.height(), (uint16_t const*)(&StaticSprite::output_buffer[0]));
  }
#endif

  return getSprite();
#ifdef DEBUG_OUTPUT
  Serial.print("img transfer: ");  
  Serial.println(millis() - StartTime);  
#endif
}


// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t TFTs::read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t TFTs::read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
//// END STOLEN CODE
