#ifndef _IPSCLOCK_WEATHER_SERVICE_OWM
#define _IPSCLOCK_WEATHER_SERVICE_OWM

#if defined(USE_SYNC_CLIENT) || defined(USE_HTTPCLIENT)
#include <WiFiClientSecure.h>
#ifdef USE_HTTPCLIENT
#include <HTTPClient.h>
#endif
#else
#include <ESPAsyncHTTPClient.h>
#endif
#include <ConfigItem.h>
#include <ArduinoJson.h>

#include "WeatherService.h"

class OpenWeatherMapWeatherService : public WeatherService {
public:
    OpenWeatherMapWeatherService();
    virtual ~OpenWeatherMapWeatherService() {}

    virtual bool            getWeatherInfo();
    virtual const String&   getIconName(int day);
    virtual float           getHigh(int day);
    virtual float           getLow(int day);
    virtual int             getDayOfWeek(int day);
    virtual float           getNowTemp();

private:
    bool sendRequest(WiFiClientSecure &client, const char *request);
    bool getCurrentWeatherInfo(WiFiClientSecure &client);
    bool getForecastWeatherInfo(WiFiClientSecure &client);

    JsonDocument todayFilter;
    JsonDocument forecastFilter;

    String conditions = "unknown";
    String iconNames[6] {
        String("unknown"),
        String("unknown"),
        String("unknown"),
        String("unknown"),
        String("unknown"),
        String("unknown")
    };

    int days[6] {
        -1, -1, -1, -1, -1, -1
    };

    float temp_min[6] {
        NAN, NAN, NAN, NAN, NAN, NAN
    };

    float temp_max[6] {
        NAN, NAN, NAN, NAN, NAN, NAN
    };

    float temp = NAN;

    int humidity;
#ifndef USE_SYNC_CLIENT
	AsyncHTTPClient httpClient;
#endif
};
#endif