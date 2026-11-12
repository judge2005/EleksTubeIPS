#ifndef _IPSCLOCK_WEATHER
#define _IPSCLOCK_WEATHER

#if defined(USE_SYNC_CLIENT) || defined(USE_HTTPCLIENT)
#include <WiFiClientSecure.h>
#ifdef USE_HTTPCLIENT
#include <HTTPClient.h>
#endif
#else
#include <ESPAsyncHTTPClient.h>
#endif
#include <ConfigItem.h>
#include <TimeSync.h>

#include "ClockTimer.h"
#include "WeatherService.h"
#include "ImageUnpacker.h"

class Weather {
public:
    Weather(WeatherService *weatherService);

    static StringConfigItem& getIconPack() { static StringConfigItem weather_icons("weather_icons", 25, "monochrome"); return weather_icons; }	// <weather_icons>.tar.gz, max length is 31
    static IntConfigItem& getWeatherHue() { static IntConfigItem weather_hue("weather_hue", 20); return weather_hue; }
    static ByteConfigItem& getWeatherSaturation() { static ByteConfigItem weather_saturation("weather_saturation", 166); return weather_saturation; }
    static ByteConfigItem& getWeatherValue() { static ByteConfigItem weather_value("weather_value", 250); return weather_value; }

    void setImageUnpacker(ImageUnpacker *imageUnpacker) { this->imageUnpacker = imageUnpacker; }
    void setTimeSync(TimeSync *pTimeSync) { this->pTimeSync = pTimeSync; }
    void checkIconPack();
    void reloadIconPack() { reloadIcons = true; }   // Unpack the icon pack again, e.g. after it was replaced

    void loop(uint8_t dimming);
    void drawSingleDay(uint8_t dimming, int day, int display);
    void redraw() { _redraw = true; }
private:
    const int indexToScreen[NUM_DIGITS] = {
        SECONDS_ONES,
        SECONDS_TENS,
        MINUTES_ONES,
        MINUTES_TENS,
        HOURS_ONES,
        HOURS_TENS
    };
    
    const char *daysOfWeek[7] = {
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday"
    };

    void drawDisplay(int index, int display, bool showDay = true);
    bool preDraw(uint8_t dimming, unsigned long redrawMs);
    void postDraw();

    WeatherService *weatherService;
    String oldIcons;
    bool reloadIcons = false;
    ClockTimer::Timer displayTimer;
    ClockTimer::Timer dateTimer;
    int shownDay = -1;      // Day of the month drawSingleDay() last saw
    ImageUnpacker *imageUnpacker;
    bool _redraw = false;
    uint8_t lastDimming = 0;
	TimeSync *pTimeSync = 0;
};
#endif