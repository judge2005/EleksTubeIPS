#include <ArduinoJson.h>

#include "TFTs.h"
#include "weather.h"

Weather::Weather(WeatherService *weatherService) {
    this->weatherService = weatherService;
    oldIcons = getIconPack().value;
	displayTimer.init(millis(), 0);
}

void drawCross(int x, int y, unsigned int color)
{
}

const int tzOffset = -18000;

void Weather::loop(uint8_t dimming) {
    if (getIconPack().value != oldIcons) {
        oldIcons = getIconPack().value;
        imageUnpacker->unpackImages("/ips/weather/" + getIconPack().value, "/ips/weather_cache");
        tfts->claim();
        tfts->invalidateAllDigits();
        tfts->release();
    }
    
    unsigned long nowMs = millis();

    if (displayTimer.expired(nowMs)) {
        static char txt[10];

        tfts->claim();
        tfts->setDimming(dimming);
        tfts->invalidateAllDigits();
        tfts->checkStatus();
        tfts->enableAllDisplays();
        tfts->setImageJustification(TFTs::TOP_CENTER);
        tfts->setBox(128, 128);

        uint16_t TEMP_COLOR = tfts->dimColor(TFT_WHITE);
        uint16_t HILO_COLOR = tfts->dimColor(TFT_SILVER);
        uint16_t DAY_FG_COLOR = tfts->dimColor(TFT_GOLD);
        uint16_t DAY_BG_COLOR = tfts->dimColor(TFT_RED);

        for (int i=0; i<6; i++) {
            tfts->setDigit(i, weatherService->getIconName(i).c_str(), TFTs::no);
            TFT_eSprite &sprite = tfts->drawImage(i);
        
            if (i == 5) {
                sprite.setTextColor(TEMP_COLOR, TFT_BLACK, false);
                sprite.setTextFont(6);
                sprite.setTextDatum(BC_DATUM);
                sprintf(txt, "%.0f", round(weatherService->getNowTemp()));
                sprite.drawString(txt, sprite.width()/2, sprite.height()*3/4);
            }

            sprite.setTextColor(HILO_COLOR, TFT_BLACK, false);

            sprite.setTextFont(4);
            sprite.setTextDatum(BL_DATUM);
            sprintf(txt, "H: %.0f", round(weatherService->getHigh(i)));
            sprite.drawString(txt, 0, sprite.height()-28);

            sprite.setTextDatum(BR_DATUM);
            sprintf(txt, "L: %.0f", weatherService->getLow(i));
            sprite.drawString(txt, sprite.width(), sprite.height()-28);

            sprite.setTextDatum(BC_DATUM);
            sprite.fillRect(0, sprite.height() - 26, sprite.width(), 26, DAY_BG_COLOR);
            sprite.setTextColor(DAY_FG_COLOR, DAY_BG_COLOR, true);
            if (i == 5)     // today
                sprite.drawString("Today", sprite.width()/2, sprite.height() + 2);
            else {
                int dow = weatherService->getDayOfWeek(i);
                if (dow >= 0) {
                    sprite.drawString(daysOfWeek[weatherService->getDayOfWeek(i)], sprite.width()/2, sprite.height() + 2);
                } else {
                    sprite.drawString("Unknown", sprite.width()/2, sprite.height() + 2);
                }
            }

            sprite.pushSprite(0, 0);
        }
        tfts->release();
 
        displayTimer.init(nowMs, 10000);
   }
}

