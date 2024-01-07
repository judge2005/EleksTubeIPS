#include "TFTs.h"
#include <WiFi.h>
#define DARKER_GREY 0x18E3

SemaphoreHandle_t TFTs::tftMutex = 0;

void TFTs::claim() {
  xSemaphoreTake(tftMutex, portMAX_DELAY);
}

void TFTs::release(){
  xSemaphoreGive(tftMutex);
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


// Too big to fit on the stack.
uint16_t TFTs::output_buffer[TFT_HEIGHT][TFT_WIDTH];

#ifndef USE_CLK_FILES

bool TFTs::LoadImageIntoBuffer(uint8_t directory, uint8_t file_index) {
  uint32_t StartTime = millis();

  fs::File bmpFS;
  // Filenames are no bigger than "255.bmp\0"
  char filename[255];
  sprintf(filename, "%s/%d/%d.clk", imageRoot, directory, file_index);
  
  // Open requested file on SD card
  bmpFS = fs->open(filename, "r");
  if (!bmpFS)
  {
    Serial.print("File not found: ");
    Serial.println(filename);
    return(false);
  }

  uint32_t seekOffset, headerSize, paletteSize = 0;
  int16_t w, h, row, col;
  uint16_t  r, g, b, bitDepth;

  // black background - clear whole buffer
  memset(output_buffer, '\0', sizeof(output_buffer));
  
  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF) {
    Serial.print("Can't openfile. Make sure you upload the LittleFS image with BMPs. : ");
    Serial.println(filename);
    bmpFS.close();
    return(false);
  }
  
  if (magic != 0x4D42) {
    Serial.print("File not a BMP. Magic: ");
    Serial.println(magic);
    bmpFS.close();
    return(false);
  }

  read32(bmpFS); // filesize in bytes
  read32(bmpFS); // reserved
  seekOffset = read32(bmpFS); // start of bitmap
  headerSize = read32(bmpFS); // header size
  w = read32(bmpFS); // width
  h = read32(bmpFS); // height
  read16(bmpFS); // color planes (must be 1)
  bitDepth = read16(bmpFS);
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
  // center image on the display
  int16_t x = (TFT_WIDTH - w) / 2;
  int16_t y = (TFT_HEIGHT - h) / 2;
  
  if (read32(bmpFS) != 0 || (bitDepth != 24 && bitDepth != 1 && bitDepth != 4 && bitDepth != 8)) {
    Serial.println("BMP format not recognized.");
    bmpFS.close();
    return(false);
  }

  uint32_t palette[256];
  if (bitDepth <= 8) // 1,4,8 bit bitmap: read color palette
  {
    read32(bmpFS); read32(bmpFS); read32(bmpFS); // size, w resolution, h resolution
    paletteSize = read32(bmpFS);
    if (paletteSize == 0) paletteSize = bitDepth * bitDepth; // if 0, size is 2^bitDepth
    bmpFS.seek(14 + headerSize); // start of color palette
    for (uint16_t i = 0; i < paletteSize; i++) {
      palette[i] = read32(bmpFS);
    }
  }

  bmpFS.seek(seekOffset);

  uint32_t lineSize = ((bitDepth * w +31) >> 5) * 4;
  uint8_t lineBuffer[lineSize];
  
  // row is decremented as the BMP image is drawn bottom up
  for (row = h-1; row >= 0; row--) {

    bmpFS.read(lineBuffer, sizeof(lineBuffer));
    uint8_t*  bptr = lineBuffer;
    
    // Convert 24 to 16 bit colours while copying to output buffer.
    for (col = 0; col < w; col++)
    {
      if (bitDepth == 24) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
        } else {
          uint32_t c = 0;
          if (bitDepth == 8) {
            c = palette[*bptr++];
          }
          else if (bitDepth == 4) {
            c = palette[(*bptr >> ((col & 0x01)?0:4)) & 0x0F];
            if (col & 0x01) bptr++;
          }
          else { // bitDepth == 1
            c = palette[(*bptr >> (7 - (col & 0x07))) & 0x01];
            if ((col & 0x07) == 0x07) bptr++;
          }
          b = c; g = c >> 8; r = c >> 16;
        }
        if (dimming < 255) { // only dim when needed
          b *= dimming;
          g *= dimming;
          r *= dimming;
          b = b >> 8;
          g = g >> 8;
          r = r >> 8;
        }
        output_buffer[row][col] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3);
      }
    }
  }
  FileInBuffer = file_index;
  
  bmpFS.close();
#ifdef DEBUG_OUTPUT
  Serial.print("img load : ");
  Serial.println(millis() - StartTime);  
#endif
  return (true);
}
#endif


#ifdef USE_CLK_FILES

bool TFTs::LoadImageIntoBuffer(uint8_t file_index) {
  uint32_t StartTime = millis();

  fs::File bmpFS;
  char filename[255];
  sprintf(filename, "%s/%d.clk", imageRoot, file_index);
  
  // Open requested file on SD card
  bmpFS = fs->open(filename, "r");
  if (!bmpFS)
  {
    Serial.print("File not found: ");
    Serial.println(filename);
    return(false);
  }

  int16_t w, h, row, col;
  uint16_t  r, g, b;

  // black background - clear whole buffer
  memset(output_buffer, '\0', sizeof(output_buffer));
  
  uint16_t magic = read16(bmpFS);
  if (magic == 0xFFFF) {
    Serial.print("Can't openfile. Make sure you upload the LittleFS image with images. : ");
    Serial.println(filename);
    bmpFS.close();
    return(false);
  }
  
  if (magic != 0x4B43) { // look for "CK" header
    Serial.print("File not a CLK. Magic: ");
    Serial.println(magic);
    bmpFS.close();
    return(false);
  }

  w = read16(bmpFS);
  h = read16(bmpFS);
#ifdef DEBUG_OUTPUT
  Serial.print("image W, H: ");
  Serial.print(w); 
  Serial.print(", "); 
  Serial.println(h);
  Serial.print("dimming: ");
  Serial.println(dimming);
#endif  
  // center image on the display
  int16_t x = (TFT_WIDTH - w) / 2;
  int16_t y = (TFT_HEIGHT - h) / 2;
  
  uint8_t lineBuffer[w * 2];
  
  // 0,0 coordinates are top left
  for (row = 0; row < h; row++) {

    bmpFS.read(lineBuffer, sizeof(lineBuffer));
    uint8_t PixM, PixL;
    
    // Colors are already in 16-bit R5, G6, B5 format
    for (col = 0; col < w; col++)
    {
      if (dimming == 255) { // not needed, copy directly
        output_buffer[row+y][col+x] = (lineBuffer[col*2+1] << 8) | (lineBuffer[col*2]);
      } else {
        // 16 BPP pixel format: R5, G6, B5 ; bin: RRRR RGGG GGGB BBBB
        PixM = lineBuffer[col*2+1];
        PixL = lineBuffer[col*2];
        // align to 8-bit value (MSB left aligned)
        r = (PixM) & 0xF8;
        g = ((PixM << 5) | (PixL >> 3)) & 0xFC;
        b = (PixL << 3) & 0xF8;
        r *= dimming;
        g *= dimming;
        b *= dimming;
        r = r >> 8;
        g = g >> 8;
        b = b >> 8;
        output_buffer[row+y][col+x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      }
    }
  }
  FileInBuffer = file_index;
  
  bmpFS.close();
#ifdef DEBUG_OUTPUT
  Serial.print("img load : ");
  Serial.println(millis() - StartTime);  
#endif
  return (true);
}
#endif 

void TFTs::DrawImage(uint8_t file_index) {

  uint32_t StartTime = millis();
  
  // check if file is already loaded into buffer; skip loading if it is. Saves 50 to 150 msec of time.
  if (file_index != FileInBuffer) {
    LoadImageIntoBuffer(file_index);
  }
  
  bool oldSwapBytes = getSwapBytes();
  setSwapBytes(true);
#ifdef USE_DMA
  startWrite();
  pushImageDMA(0,0, TFT_WIDTH, TFT_HEIGHT, (uint16_t *)output_buffer);
  endWrite();
#else
  pushImage(0,0, TFT_WIDTH, TFT_HEIGHT, (uint16_t *)output_buffer);
#endif
  setSwapBytes(oldSwapBytes);

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
