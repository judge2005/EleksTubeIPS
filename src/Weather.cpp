#include <ArduinoJson.h>

#include "TFTs.h"
#include "weather.h"
#include "ColorConversion.h"
#include "IPSClock.h"

#include <math.h>

Weather::Weather(WeatherService *weatherService) {
    this->weatherService = weatherService;
    oldIcons = getIconPack().value;
	displayTimer.init(millis(), 0);
}

void drawCross(int x, int y, unsigned int color)
{
}

const int tzOffset = -18000;

void Weather::drawDisplay(int index, int display, bool showDay) {
    if (!tfts->isEnabled()) {
        return;
    }
    
    char txt[10];

    // Load 'space' glyph if any
    tfts->setShowDigits(true);
    tfts->setImageJustification(TFTs::MIDDLE_CENTER);
    tfts->setDigit(SECONDS_ONES, "space", TFTs::no);
    TFT_eSprite &sprite = tfts->drawImage(SECONDS_ONES);
    tfts->setShowDigits(false);

    tfts->setImageJustification(TFTs::TOP_CENTER);
    tfts->setBox(128, 128);

    uint16_t rgb565 = hsv2rgb565(getWeatherHue(), getWeatherSaturation(), getWeatherValue());

    uint16_t TEMP_COLOR = tfts->dimColor(rgb565);
    uint16_t HILO_COLOR = TEMP_COLOR;
    uint16_t DAY_FG_COLOR = tfts->dimColor(TFT_GOLD);
    uint16_t DAY_BG_COLOR = tfts->dimColor(TFT_RED);
	tfts->setMonochromeColor(rgb565);

    tfts->setDigit(indexToScreen[display], weatherService->getIconName(index).c_str(), TFTs::no);
    tfts->drawImage(indexToScreen[display]);

    float val = NAN;

    if (index == 5) {
        sprite.setTextColor(TEMP_COLOR);
        sprite.setTextFont(6);
        sprite.setTextDatum(BC_DATUM);
        val = weatherService->getNowTemp();
        if (isnan(val)) {
            strcpy(txt, "--");
        } else {
            sprintf(txt, "%.0f", round(val));
        }
        sprite.drawString(txt, sprite.width()/2, sprite.height()*3/4 - 2);
    }

    sprite.setTextFont(4);

    int baseline = sprite.height()-30;
    int arrowHeight = 10, arrowWidth = 10;
    int arrowPadding = 2;
    int tempInset=10;
    int textHeight = sprite.fontHeight();
    int arrowTop = baseline - textHeight + 2;

    sprite.setTextColor(HILO_COLOR);

    sprite.setTextDatum(BL_DATUM);
    val = round(weatherService->getHigh(index));
    if (isnan(val)) {
        strcpy(txt, "--");
    } else {
        sprintf(txt, "%.0f", round(val));
    }
    sprite.drawString(txt, tempInset + arrowWidth + arrowPadding, baseline);

    // Draw up-arrow
    sprite.fillTriangle(tempInset, arrowTop+arrowHeight,     // bottom left
                tempInset+arrowWidth, arrowTop+arrowHeight,  // bottom right
                tempInset+arrowWidth/2, arrowTop,            // top-center
                TEMP_COLOR);


    val = round(weatherService->getLow(index));
    if (isnan(val)) {
        strcpy(txt, "--");
    } else {
        sprintf(txt, "%.0f", round(val));
    }
    int textWidth=sprite.textWidth(txt, 4);
    sprite.setTextDatum(BL_DATUM);
    sprite.drawString(txt, sprite.width() - tempInset - textWidth, baseline);

    // Draw down-arrow
    int arrowLeft = sprite.width() - tempInset - textWidth - arrowPadding - arrowWidth;

    sprite.fillTriangle(arrowLeft, arrowTop,        // top left
            arrowLeft + arrowWidth, arrowTop,       // top right
            arrowLeft + arrowWidth/2, arrowTop+arrowHeight, // bottom center
            TEMP_COLOR);


    if (showDay) {
        sprite.setTextDatum(BC_DATUM);
        sprite.fillRect(0, sprite.height() - 26, sprite.width(), 26, DAY_BG_COLOR);
        sprite.setTextColor(DAY_FG_COLOR);
        if (index == 5)     // today
            sprite.drawString("Today", sprite.width()/2, sprite.height() + 2);
        else {
            int dow = weatherService->getDayOfWeek(index);
            if (dow >= 0) {
                sprite.drawString(daysOfWeek[weatherService->getDayOfWeek(index)], sprite.width()/2, sprite.height() + 2);
            } else {
                sprite.drawString("Unknown", sprite.width()/2, sprite.height() + 2);
            }
        }
    } else {
        struct tm now;
        suseconds_t uSec;
        pTimeSync->getLocalTime(&now, &uSec);

        uint8_t day = now.tm_mday;
        uint8_t month = now.tm_mon;
        uint8_t year = now.tm_year % 100;

        switch (IPSClock::getDateFormat().value) {
        case 0:	// DD-MM-YY
            break;
        case 1: // MM-DD-YY
            day = now.tm_mon;
            month = now.tm_mday;
            break;
        default: // YY-MM-DD
            day = now.tm_year;
            year = now.tm_mday;
            break;
        }

        sprintf(txt, "%02d-%02d-%02d", day, month, year);

        sprite.setTextDatum(BC_DATUM);

        sprite.drawString(txt, sprite.width()/2, sprite.height()-4);
    }

    sprite.pushSprite(0, 0);
}

void Weather::drawSingleDay(uint8_t dimming, int day, int display) {
    checkIconPack();
    unsigned long nowMs = millis();

    if (preDraw(dimming)) {
        // day is indexed from 0 thru 5 with 0 being today
        drawDisplay(5-day, display, false);

        postDraw();
    }
}

void Weather::checkIconPack() {
    if (getIconPack().value != oldIcons) {
        oldIcons = getIconPack().value;
        imageUnpacker->unpackImages("/ips/weather/" + getIconPack().value, "/ips/weather_cache");
        tfts->claim();
        tfts->invalidateAllDigits();
        tfts->release();
        _redraw = true;
    }
}

bool Weather::preDraw(uint8_t dimming) {
    unsigned long nowMs = millis();

    if (_redraw || displayTimer.expired(nowMs)) {
        _redraw = false;

        tfts->claim();
    	tfts->setShowDigits(false);
        // tfts->invalidateAllDigits();
        tfts->setDimming(dimming);

        displayTimer.init(nowMs, 10000);

        return true;
    }

    return false;
}

void Weather::postDraw() {
    tfts->release();
}

void Weather::loop(uint8_t dimming) {
    checkIconPack();

    if (preDraw(dimming)) {
        tfts->checkStatus();
        tfts->enableAllDisplays();

        for (int i=0; i<6; i++) {
            drawDisplay(i, i);
        }

        postDraw();
   }
}

