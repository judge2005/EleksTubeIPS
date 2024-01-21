#ifndef _IPSCLOCK_WEATHER_SERVICE
#define _IPSCLOCK_WEATHER_SERVICE
#include <WString.h>

class WeatherService {
public:
    WeatherService() {}
    virtual ~WeatherService() {}

    static StringConfigItem& getWeatherToken() { static StringConfigItem weather_token("weather_token", 63, ""); return weather_token; }	// openweathermap.org API token
    static StringConfigItem& getUnits() { static StringConfigItem units("units", 10, "imperial"); return units; }
    static StringConfigItem& getLongitude() { static StringConfigItem longitude("weather_longitude", 10, "23.7275"); return longitude; }
    static StringConfigItem& getLatitude() { static StringConfigItem latitude("weather_latitude", 10, "37.9838"); return latitude; }

    virtual bool            getWeatherInfo() = 0;
    virtual const String&   getIconName(int day) = 0;
    virtual float           getHigh(int day) = 0;
    virtual float           getLow(int day) = 0;
    virtual int             getDayOfWeek(int day) = 0;
    virtual float           getNowTemp() = 0;
};
#endif