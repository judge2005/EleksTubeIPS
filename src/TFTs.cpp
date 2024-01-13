#include "TFTs.h"
#include <WiFi.h>
#define DARKER_GREY 0x18E3

SemaphoreHandle_t TFTs::tftMutex = 0;

uint8_t StaticSprite::output_buffer[(TFT_HEIGHT * TFT_WIDTH + 1) * sizeof(uint16_t)];

void StaticSprite::init() {
  _iwidth  = _dwidth  = _bitwidth = TFT_WIDTH;
  _iheight = _dheight = TFT_HEIGHT;

  cursor_x = 0;
  cursor_y = 0;

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

TFT_eSprite& TFTs::getSprite() {
  static bool initialized = false;
  static StaticSprite sprite(&tfts);

  if (!initialized) {
    initialized = true;
    sprite.init();
  }

  return sprite;
}

TFT_eSprite& TFTs::getStatusSprite() {
  static bool initialized = false;
  static TFT_eSprite sprite(&tfts);

  if (!initialized) {
    initialized = true;

    sprite.createSprite(width(), 16);

    sprite.setTextDatum(BC_DATUM);
    sprite.setTextColor(TFT_GOLD, TFT_BLACK, true);
    sprite.setTextFont(2);
  }

  return sprite;
}

void TFTs::checkStatus() {
  if (millis() - statusTime > 5000) {
    if (statusSet)  {
      statusSet = false;
      invalidateAllDigits();
    }
  }
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

void TFTs::drawStatus() {
  if (millis() - statusTime < 5000) {
    TFT_eSprite& sprite = getStatusSprite();
    sprite.pushSprite(0, height() - sprite.height());
  }
}

void TFTs::drawMeter(int val, bool first, const char *legend) {
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

size_t TFTs::printlnon(uint8_t display, const char s[]){
  claim();
  uint8_t saved = chip_select.getDigitMap();

  chip_select.setDigit(display, true);
  size_t ret = println(s);

  chip_select.setDigitMap(saved, true);

  release();

  return ret;
}

size_t TFTs::printlnall(const String &s){
  return printlnall(s.c_str());
}

size_t TFTs::printlnall(const char s[]){
  claim();
  uint8_t saved = chip_select.getDigitMap();

  chip_select.setAll();
  size_t ret = println(s);

  chip_select.setDigitMap(saved, true);

  release();

  return ret;
}
  
void TFTs::setDimming(uint8_t dimming) {
  if (dimming != this->dimming) {
    invalidateAllDigits();
    this->dimming = dimming;
  }
}

void TFTs::begin(fs::FS& fs, const char *imageRoot) {
  if (tftMutex == 0) {
    tftMutex = xSemaphoreCreateMutex();
  }

  this->fs = &fs;
  this->imageRoot = imageRoot;
  
  // Start with all displays selected.
  chip_select.begin();
  chip_select.setAll();

  // Turn power on to displays.
  pinMode(TFT_ENABLE_PIN, OUTPUT);
  enableAllDisplays();
  InvalidateImageInBuffer();

  // Initialize the super class.
  init();

#ifdef USE_DMA
  initDMA();
#endif
}

void TFTs::clear() {
  // Start with all displays selected.
  chip_select.setAll();
  enableAllDisplays();
}

void TFTs::setDigit(uint8_t digit, uint8_t value, show_t show) {
  uint8_t old_value = digits[digit];
  digits[digit] = value;
  
  if (show != no && (old_value != value || show == force)) {
    showDigit(digit);
  }
}

/* 
 * Displays the bitmap for the value to the given digit. 
 */
 
void TFTs::showDigit(uint8_t digit) {
  chip_select.setDigit(digit);

  if (digits[digit] == blanked) {
    fillScreen(TFT_BLACK);
  }
  else {
    uint8_t file_index = digits[digit];
    DrawImage(file_index);
    
    uint8_t NextNumber = digits[SECONDS_ONES] + 1;
    if (NextNumber > 9) NextNumber = 0; // pre-load only seconds, because they are drawn first
    NextFileRequired = NextNumber;
  }

  drawStatus();
}

void TFTs::LoadNextImage() {
  if (NextFileRequired != FileInBuffer) {
#ifdef DEBUG_OUTPUT
    Serial.println("Preload img");
#endif
    LoadImageIntoBuffer(NextFileRequired);
  }
}

void TFTs::InvalidateImageInBuffer() { // force reload from Flash with new dimming settings
  FileInBuffer=255; // invalid, always load first image
}

// These BMP functions are stolen directly from the TFT_SPIFFS_BMP example in the TFT_eSPI library.
// Unfortunately, they aren't part of the library itself, so I had to copy them.
// I've modified DrawImage to buffer the whole image at once instead of doing it line-by-line.
bool TFTs::LoadBMPImageIntoBuffer(fs::File &bmpFile) {
  uint32_t seekOffset, headerSize, paletteSize = 0;
  int16_t w, h, row, col;
  uint16_t  r, g, b, bitDepth;
  
  uint16_t magic = read16(bmpFile);
  if (magic != 0x4D42) {
    Serial.print("File not a BMP. Magic: ");
    Serial.println(magic);
    return false;
  }

  read32(bmpFile); // filesize in bytes
  read32(bmpFile); // reserved
  seekOffset = read32(bmpFile); // start of bitmap
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
  if (bitDepth != 24 && bitDepth != 16 && bitDepth != 1 && bitDepth != 4 && bitDepth != 8) {
    Serial.print("Bad bit depth: ");
    Serial.println(bitDepth);
    return false;
  }

  int32_t compression = read32(bmpFile);

  if (compression != 0) {
    Serial.print("Bad compression: ");
    Serial.println(compression);
    // return false;
  }
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
  if (bitDepth <= 8) // 1,4,8 bit bitmap: read color palette
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
  }

  bmpFile.seek(seekOffset);

  return LoadImageBytesIntoSprite(w, abs(h), bitDepth, ((bitDepth * w +31) >> 5) * 4, h > 0, palette, bmpFile);
}

bool TFTs::LoadCLKImageIntoBuffer(fs::File &clkFile) {
  int16_t w, h;

  uint16_t magic = read16(clkFile);
 
  if (magic != 0x4B43) { // look for "CK" header
    Serial.print("File not a CLK. Magic: ");
    Serial.println(magic);
    return false;
  }

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

  return LoadImageBytesIntoSprite(w, h, 16, w * 2, false, 0, clkFile);
}

bool TFTs::LoadImageBytesIntoSprite(int16_t w, int16_t h, uint8_t bitDepth, int16_t rowSize, bool reversed, uint32_t *palette, fs::File &file) {
  // center image on the display
  int16_t x = (TFT_WIDTH - w) / 2;
  int16_t y = (TFT_HEIGHT - h) / 2;

  uint32_t outputBufferSize = w * 2;
  if (outputBufferSize < rowSize) {
    outputBufferSize = rowSize; // So that input and output buffer can be the same. Basically for bitDepth >= 16
  }
  uint8_t outputBuffer[outputBufferSize];

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

  TFT_eSprite& sprite = getSprite();
  sprite.fillSprite(0);
  
  bool oldSwapBytes = sprite.getSwapBytes();
  sprite.setSwapBytes(true);

  // 0,0 coordinates are top left
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
    if (dimming != 255 || bitDepth != 16) {
      uint8_t*  inputPtr = inputBuffer;

      for (int col = 0; col < w; col++)
      {
        uint16_t r, g, b;
        uint32_t c = 0;

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
              // 16 BPP pixel format: R5, G6, B5 ; bin: RRRR RGGG GGGB BBBB
              uint8_t PixM = inputBuffer[col*2+1];
              uint8_t PixL = inputBuffer[col*2];
              // align to 8-bit value (MSB left aligned)
              r = (PixM) & 0xF8;
              g = ((PixM << 5) | (PixL >> 3)) & 0xFC;
              b = (PixL << 3) & 0xF8;
            }
            break;
          case 8:
            c = palette[*inputPtr++];
            b = c & 0xff; g = (c >> 8) & 0xff; r = (c >> 16) & 0xff;
            break;
          case 4:
            c = palette[(*inputPtr >> ((col & 0x01)?0:4)) & 0x0F];
            if (col & 0x01) inputPtr++;
            b = c; g = c >> 8; r = c >> 16;
            break;
          case 1:
            c = palette[(*inputPtr >> (7 - (col & 0x07))) & 0x01];
            if ((col & 0x07) == 0x07) inputPtr++;
            b = c; g = c >> 8; r = c >> 16;
            break;
        }
        
        if (dimming != 255) {
          r *= dimming;
          g *= dimming;
          b *= dimming;
          r = r >> 8;
          g = g >> 8;
          b = b >> 8;
        }

        outputBuffer[col*2+1] = (r & 0xF8) | ((g & 0xFC) >> 5);
        outputBuffer[col*2] = ((g & 0x1C) << 3) | ((b & 0xF8) >> 3);
      }
    }
    sprite.pushImage(x, reversed ? (h-row-1) + y : row + y, w, 1, (uint16_t*)outputBuffer);
  }

  sprite.setSwapBytes(oldSwapBytes);

  if (bitDepth < 16) {
    free(inputBuffer);
  }

  return true;
}

bool TFTs::LoadImageIntoBuffer(uint8_t file_index) {
  uint32_t StartTime = millis();

  fs::File file;
  char filename[255];
  sprintf(filename, "%s/%d.clk", imageRoot, file_index);
  
  // Open requested file on SD card
  if (fs->exists(filename)) {
    file = fs->open(filename, "r");
    if (file) {
      bool loaded = LoadCLKImageIntoBuffer(file);
      if (loaded) {
        FileInBuffer = file_index;
      }
      file.close();
      return loaded;
    }
  }

  sprintf(filename, "%s/%d.bmp", imageRoot, file_index);
  if (fs->exists(filename)) {
    file = fs->open(filename, "r");
    if (file) {
      bool loaded = LoadBMPImageIntoBuffer(file);
      if (loaded) {
        FileInBuffer = file_index;
      }
      file.close();
      return loaded;
    }
  }

  Serial.print("File not found: ");
  Serial.println(filename);
  return false;
}

void TFTs::DrawImage(uint8_t file_index) {

  uint32_t StartTime = millis();
  
  // check if file is already loaded into buffer; skip loading if it is. Saves 50 to 150 msec of time.
  if (file_index != FileInBuffer) {
    LoadImageIntoBuffer(file_index);
  }
  
  // bool oldSwapBytes = getSwapBytes();
  // setSwapBytes(true);
#ifdef USE_DMA
  startWrite();
  pushImageDMA(0,0, TFT_WIDTH, TFT_HEIGHT, (uint16_t *)output_buffer);
  endWrite();
#else
  TFT_eSprite& sprite = getSprite();
  sprite.pushSprite(0, 0);

  // pushImage(0,0, TFT_WIDTH, TFT_HEIGHT, (uint16_t *)output_buffer);
#endif
  // setSwapBytes(oldSwapBytes);

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
