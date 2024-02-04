#include "DigitalRainAnimation.h"
#include "ColorConversion.h"

unsigned long DigitalRainAnimation::getMsDelay() {
  return 1000UL / getMatrixSpeed();
}

void DigitalRainAnimation::prepareAnim()
{
  if (_gfx == NULL)
    return;

  unsigned long timeFrame = getMsDelay();
  lastDrawTime = millis() - timeFrame;
  width = _gfx->width();
  height = _gfx->height();
  _gfx->fillRect(0, 0, width, height, 0);
  _gfx->setTextColor(hsv2rgb565(getMatrixHue(), getMatrixSaturation(), getMatrixValue()), 0);
  numOfline = (width + lineWidth - 1) / lineWidth;
  numOfRows = (_gfx->height() + letterHeight - 1) / letterHeight + 2; // 2 greater than fits on the display

  line_length = std::vector<int8_t>(numOfline*2);
  line_pos = std::vector<int8_t>(numOfline*2);
  line_chars = std::vector<std::vector<char>>(numOfline);

  for (int line = 0; line < numOfline * 2; line += 2)
  {
    // First drop in line
    line_length[line] = getRandomNum(line_len_min, line_len_max);
    line_pos[line] = random(0, numOfRows + line_length[line]) - line_length[line];
    // Second drop in line
    line_length[line + 1] = getRandomNum(line_len_min, line_len_max);
    line_pos[line + 1] = line_pos[line] - line_length[line] - random(line_len_min, line_len_max);
    for (int row = 0; row < numOfRows; row++)
    {
      line_chars[line >> 1] = std::vector<char>(numOfRows);
      line_chars[line >> 1][row] = isAlphabetOnly ? getAbcASCIIChar() : getASCIIChar();
    }
  }

  isPlaying = true;
  lastUpdatedKeyTime = millis() - timeFrame;
}

// updating each line with a new length, Y position, and speed.
void DigitalRainAnimation::lineUpdate(int lineNum, int lineNumPrev)
{
  line_length[lineNum] = getRandomNum(line_len_min, line_len_max);
  int prevStart = line_pos[lineNumPrev] - line_length[lineNumPrev];
  line_pos[lineNum] = prevStart - random(line_len_min, line_len_max);
  if (line_pos[lineNum] >= 0)
  {
    line_pos[lineNum] = -1;
  }
}

void DigitalRainAnimation::mutateCharAt(int lineNum, int row)
{
  if (random(0, 10) == 0)
  {
    line_chars[lineNum][row] = isAlphabetOnly ? getAbcASCIIChar() : getASCIIChar();
  }
}

void DigitalRainAnimation::lineAnimation2(int startX, int lineNum, int dropIndex)
{
  uint8_t hue = getMatrixHue();
  uint8_t val = getMatrixValue();
  uint8_t sat = getMatrixSaturation();

  bool isKeyMode = keyString.length() > 0;
  for (int row = line_pos[dropIndex] - line_length[dropIndex]; row <= line_pos[dropIndex]; row++)
  {
    int yPos = row * letterHeight;
    if (row >= 0 && yPos < _gfx->height())
    { // On screen
      mutateCharAt(lineNum, row);
      char charToPrint = line_chars[lineNum][row];
      if (row == line_pos[dropIndex])
      {
        _gfx->setTextColor(hsv2rgb565(hue, 60, val), 0);
        if (keyString.length() > dropIndex)
        {
          charToPrint = keyString.at(dropIndex);
        }
      }
      else
      {
        int colorVal = map(row - (line_pos[dropIndex] - line_length[dropIndex]), 0, line_length[dropIndex], 10, val);
        uint16_t lumColor = hsv2rgb565(hue, sat, colorVal);
        _gfx->setTextColor(isKeyMode ? _gfx->color565(colorVal, 0, 0) : lumColor, 0);
      }
      _gfx->setCursor(startX, yPos);
      _gfx->setTextSize(fontSize);
      _gfx->print(charToPrint);
    }
  }
}

void DigitalRainAnimation::lineAnimation(int lineNum)
{
  int startX = lineNum * lineWidth;
  _gfx->fillRect(startX, 0, lineWidth, height, 0);

  int dropIndex = lineNum * 2;
  lineAnimation2(startX, lineNum, dropIndex);

  line_pos[dropIndex]++;
  if (line_pos[dropIndex] >= line_length[dropIndex] + numOfRows)
  {
    lineUpdate(dropIndex, dropIndex + 1);
  }

  dropIndex++;
  lineAnimation2(startX, lineNum, dropIndex);

  line_pos[dropIndex]++;
  if (line_pos[dropIndex] >= line_length[dropIndex] + numOfRows)
  {
    lineUpdate(dropIndex, dropIndex - 1);
  }
}
// a function that gets randomly from ASCII codes 33 to 63 inclusive and 91 to 126 inclusive. (For MatrixCodeNFI)
char DigitalRainAnimation::getASCIIChar()
{
  return (char)(random(0, 2) == 0 ? random(33, 64) : random(91, 127));
}

// a function that gets only alphabets from ASCII code.
char DigitalRainAnimation::getAbcASCIIChar()
{
  return (char)(random(0, 2) == 0 ? random(65, 91) : random(97, 123));
}

// the function is to get the random number (including max)
int DigitalRainAnimation::getRandomNum(int min, int max)
{
  return random(min, max + 1);
}

// the function is to generate a random key with length with a length
std::string DigitalRainAnimation::getKey(int key_length)
{
  resetKey();
  int maxKeyLength = (key_length > 0 ? (key_length > numOfline ? numOfline : key_length) : numOfline);

  for (int i = 0; i < maxKeyLength; i++)
  {
    keyString.append(1, getAbcASCIIChar());
  }

  return keyString;
}

// the function is to remove the generated key
void DigitalRainAnimation::resetKey()
{
  keyString = "";
  lastUpdatedKeyTime = millis();
}

// set Text Bigger
void DigitalRainAnimation::setBigText(bool isOn)
{
  fontSize = isOn ? FONT_SIZE * 2 : FONT_SIZE;
  lineWidth = isOn ? LINE_WIDTH * 2 : LINE_WIDTH;
  letterHeight = isOn ? LETTER_HEIGHT * 1.6 : LETTER_HEIGHT;
}

DigitalRainAnimation::DigitalRainAnimation() {}

// initialization
void DigitalRainAnimation::init(TFT_eSPI *gfx, bool biggerText, bool alphabetOnly)
{
  _gfx = gfx;
  setBigText(biggerText);
  line_len_min = 3;
  line_len_max = 10;
  isAlphabetOnly = alphabetOnly;
  prepareAnim();
}

// updating screen
void DigitalRainAnimation::loop()
{
  if (_gfx == NULL)
    return;

  uint32_t currentTime = millis();

  if (getMatrixHueCycling()) {
    cycleMs = getMatrixHueCycleTime() * 1000UL / 255;
    if (currentTime - lastCycleTime >= cycleMs) {
      lastCycleTime = currentTime;
      IntConfigItem& hue = getMatrixHue();
      hue = (hue + 1) % 256;
      hue.notify();
    }
  }

  if (((currentTime - lastUpdatedKeyTime) > KEY_RESET_TIME))
  {
    resetKey();
  }

  if (((currentTime - lastDrawTime) < getMsDelay()))
  {
    return;
  }

  if (isPlaying)
  {
    for (int i = 0; i < numOfline; i++)
      lineAnimation(i);
  }

  lastDrawTime = currentTime;
}

// a function to stop animation.
void DigitalRainAnimation::pause()
{
  isPlaying = false;
}

// a function to resume animation.
void DigitalRainAnimation::resume()
{
  isPlaying = true;
}
