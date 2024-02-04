#ifndef _DIGITAL_RAIN_ANIMATION_H
#define _DIGITAL_RAIN_ANIMATION_H
#define FONT_SIZE 1               //set font size 1
#define LINE_WIDTH 15             //width for font size 2
#define LETTER_HEIGHT 16          //height for font size 2
#define KEY_RESET_TIME 60 * 1000  //60 seconds reset time
#include <vector>
#include <string>
#include <ConfigItem.h>
#include <TFT_eSPI.h>

class DigitalRainAnimation {
public:
  DigitalRainAnimation();

  static ByteConfigItem& getMatrixSpeed() { static ByteConfigItem matrix_speed("matrix_speed", 16); return matrix_speed; }
  static IntConfigItem& getMatrixHueCycleTime() { static IntConfigItem matrix_hue_cycle_time("matrix_hue_cycle_time", 139); return matrix_hue_cycle_time; }
  static BooleanConfigItem& getMatrixHueCycling() { static BooleanConfigItem matrix_hue_cycling("matrix_hue_cycling", false); return matrix_hue_cycling; }
  static IntConfigItem& getMatrixHue() { static IntConfigItem matrix_hue("matrix_hue", 85); return matrix_hue; }
  static ByteConfigItem& getMatrixSaturation() { static ByteConfigItem matrix_saturation("matrix_saturation", 255); return matrix_saturation; }
  static ByteConfigItem& getMatrixValue() { static ByteConfigItem matrix_value("matrix_value", 255); return matrix_value; }

  //initialization
  void init(TFT_eSPI* gfx ,bool biggerText = false, bool alphabetOnly = false);
  //setup for Matrix
  void setup(int new_line_len_min, int new_line_len_max, int matrix_timeFrame);
  //updating screen
  void loop();
  //a function to stop animation.
  void pause();
  //a function to resume animation.
  void resume();

private:
  TFT_eSPI* _gfx = NULL;
  int line_len_min;              //minimum length of characters
  int line_len_max;              //maximum length of characters
  int width, height;             //width, height of display
  int numOfline;                 //number of calculated line
  int numOfRows;                 //number of calculated row
  uint8_t fontSize;              //default font size 1
  uint8_t lineWidth;             //default line width
  uint8_t letterHeight;          //default letter height
  bool isPlaying;                //boolean for play or pause
  bool isAlphabetOnly;           //boolean for showing Alphabet only
  uint32_t lastDrawTime;         //checking last drawing time
  uint32_t lastUpdatedKeyTime;   //checking last generating key time
  uint32_t lastCycleTime;
  uint32_t cycleMs;
  std::vector<int8_t> line_length;  //dynamic array for each line of vertical length
  std::vector<int8_t> line_pos;     //dynamic array for each line Y position
  std::vector<std::vector<char>> line_chars;
  std::string keyString;         //storing generated key

  void prepareAnim();
  //updating each line with a new length, Y position, and speed.
  void lineUpdate(int lineNum, int lineNumPrev);
  void mutateCharAt(int lineNum, int row);
  void lineAnimation2(int startX, int lineNum, int dropIndex);
  void lineAnimation(int lineNum);
  //a function that gets randomly from ASCII codes 33 to 63 inclusive and 91 to 126 inclusive. (For MatrixCodeNFI)
  char getASCIIChar();
  //a function that gets only alphabets from ASCII code.
  char getAbcASCIIChar();
  //the function is to get the random number (including max)
  int getRandomNum(int min, int max);
  //the function is to generate a random key with length with a length
  std::string getKey(int key_length);
  //the function is to remove the generated key
  void resetKey();
  //set Text Bigger
  void setBigText(bool isOn);
  unsigned long getMsDelay();
};

#endif