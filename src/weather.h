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

#include "ClockTimer.h"
#include "WeatherService.h"
#include "ImageUnpacker.h"

class Weather {
public:
    Weather(WeatherService *weatherService);

    static StringConfigItem& getIconPack() { static StringConfigItem weather_icons("weather_icons", 25, "yahoo"); return weather_icons; }	// <weather_icons>.tar.gz, max length is 31

    void setImageUnpacker(ImageUnpacker *imageUnpacker) { this->imageUnpacker = imageUnpacker; }

    void loop(uint8_t dimming);
    void redraw() { _redraw = true; }
private:
    const char *daysOfWeek[7] = {
        "Sunday",
        "Monday",
        "Tuesday",
        "Wednesday",
        "Thursday",
        "Friday",
        "Saturday"
    };

    WeatherService *weatherService;
    String oldIcons;
    ClockTimer::Timer displayTimer;
    ImageUnpacker *imageUnpacker;
    bool _redraw = false;
};
#endif