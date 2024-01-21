#include "OpenWeatherMapWeatherService.h"

#define SECONDS_IN_DAY 86400
#define SECONDS_IN_PERIOD 10800

OpenWeatherMapWeatherService::OpenWeatherMapWeatherService() : WeatherService() {
/*
 * Only want these fields for current conditions:
 *
 {
    "weather": [
      {
        "main": "Rain",
        "icon": "10d"
      }
    ],
    "main": {
      "temp": 298.48,
      "temp_min": 297.56,
      "temp_max": 300.05,
      "humidity": 64
    }
 }
 *
 * See https://arduinojson.org/v6/assistant/#/step1 for sizing details
 */
    todayFilter["weather"][0]["main"] = true;
    todayFilter["weather"][0]["icon"] = true;

    todayFilter["main"]["temp"] = true;
    todayFilter["main"]["temp_min"] = true;
    todayFilter["main"]["temp_max"] = true;
    todayFilter["main"]["humidity"] = true;

/*
 * And these for 5 day forecast:
 *
 {
    "list": [
        {
            "dt": true,
            "main": {
                "temp": true
            },
            "weather": [
                {
                    "main": true,
                    "icon": true
                }
            ]
        }
    ],
    "city": {
        "timezone": true
    }
}
*/
    forecastFilter["list"][0]["dt"] = true;
    forecastFilter["list"][0]["main"]["temp"] = true;
    
    forecastFilter["list"][0]["weather"][0]["main"] = true;
    forecastFilter["list"][0]["weather"][0]["icon"] = true;
    forecastFilter["city"]["timezone"] = true;
}

const String& OpenWeatherMapWeatherService::getIconName(int day) {
    return iconNames[day];
}

float OpenWeatherMapWeatherService::getHigh(int day) {
    return temp_max[day];
}

float OpenWeatherMapWeatherService::getLow(int day) {
    return temp_min[day];
}

int OpenWeatherMapWeatherService::getDayOfWeek(int day) {
    return days[day];
}

float OpenWeatherMapWeatherService::getNowTemp() {
    return temp;
}

#if  defined(USE_SYNC_CLIENT) || defined(USE_HTTPCLIENT)
#ifndef USE_HTTPCLIENT
bool OpenWeatherMapWeatherService::sendRequest(WiFiClientSecure &client, const char *request) {
    const char *token = getWeatherToken().value.c_str();

    if (strlen(token) == 0)
    {
        Serial.println("No token defined");
        return false;
    }

    char url[255];
    char *units = "imperial";
    if (getUnits()) {
        units = "metric";
    }
    sprintf(url, "GET /data/2.5/%s?lat=%s&lon=%s&appid=%s&units=%s HTTP/1.1", request, getLatitude().value, getLongitude().value, token, units);

    Serial.print("requesting URL: ");
    Serial.println(url);
    client.println(url);
    client.println("Host: api.openweathermap.org");
    client.println("Connection: keep-alive");
    client.println();

    uint32_t StartTime = millis();
    bool response_ok = false;
    uint8_t c;
    size_t n = 0;
    bool haveChar = false;
    while (client.connected())
    {
        delay(1);

        while ((n = client.readBytes(&c, 1)) == 1) {
            if (c == '\n') {
                Serial.println();
                break;
            }
            if (c != '\r') {
                Serial.print((char)c);
                haveChar = true;    // a non-empty line
            }
        }

        // Either we failed to read a byte (so we want to try again), or we read '\n'
        if (n != 1) {
            if (millis() - StartTime > 15 * 1000)
            {
                response_ok = false;
                break;
            }
        }

        // If we got here, we read '\n'
        if (!haveChar) {
            response_ok = true;
            break;
        } else {
            haveChar = false;   // Read a non-empty line, go around again
        }
    }

    if (!response_ok)
    {
        Serial.println("Error reading header data from server.");
        return false;
    }

    return true;
}

bool OpenWeatherMapWeatherService::getCurrentWeatherInfo(WiFiClientSecure &client)
{
    if (!sendRequest(client, "weather")) {
        return false;
    }

	StaticJsonDocument<256> doc;

    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

    bool parsingError = false;

	DeserializationError deserializeError = deserializeJson(doc, client, DeserializationOption::Filter(todayFilter));
	if (!deserializeError) {
        JsonObject weather = doc["weather"][0];
        conditions = weather["main"] | "Unknown";
        iconNames[5] = weather["icon"] | "unknown";
        Serial.printf("Conditions: %s\n", conditions);
        Serial.printf("Icon name: %s\n", iconNames[5].c_str());

        JsonObject main = doc["main"];
        temp = main.containsKey("temp") ? main["temp"] : -1;
        humidity = main.containsKey("humidity") ? main["humidity"] : -1;
        Serial.printf("Temp: %f\n", temp);
	} else {
        parsingError = true;
		Serial.print(F("Deserialize error while parsing weather: "));
		Serial.println(deserializeError.c_str());
	}

    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

    return !parsingError;
}

bool OpenWeatherMapWeatherService::getForecastWeatherInfo(WiFiClientSecure &client) {
    if (!sendRequest(client, "forecast")) {
        return false;
    }

	DynamicJsonDocument doc(6144);

    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

    bool parsingError = false;

	DeserializationError deserializeError = deserializeJson(doc, client, DeserializationOption::Filter(forecastFilter));
	if (!deserializeError) {
        bool missing = false;
/*
{
    "list": [
        {
            "dt": true,
            "main": {
                "temp": true
            },
            "weather": [
                {
                    "main": true,
                    "icon": true
                }
            ]
        }
    ],
    "city": {
        "timezone": true
    }
}

*/
        int tzOffset = doc["city"]["timezone"] | 0;
        Serial.printf("TZ Offset: %d\n", tzOffset);

        time_t now;
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        time(&now);

        // Truncate to midnight in current timezone:
        time_t midnightLocal = now - (now + tzOffset) % SECONDS_IN_DAY;
        midnightLocal += SECONDS_IN_DAY;   // Advance to midnight of next day
        time_t nextNoon = midnightLocal + SECONDS_IN_DAY / 2;   // Calculate noon of next day too

        Serial.printf("Next noon: %d\n", nextNoon);

        time_t ftime = now + tzOffset;
        struct tm ftm;
        gmtime_r(&ftime, &ftm);
        Serial.printf("Day %d, hour %d, minute %d\n", ftm.tm_wday, ftm.tm_hour, ftm.tm_min);
        int dIndex = ftm.tm_wday;

        JsonArray list = doc["list"];
        int iconNameIndex = 4;
        const char *iName = "unknown";
        int hiLoIndex = 5;
        float maxTemp = -200;
        float minTemp = 200;
        temp_min[hiLoIndex] = -1;
        temp_max[hiLoIndex] = -1;
        float temp = -500;

        for (int i=0; i<list.size(); i++) {
            JsonObject forecast = list[i];
            long dt = forecast.containsKey("dt") ? forecast["dt"] : -1;

            JsonObject main = forecast["main"];
            temp = main.containsKey("temp") ? main["temp"] : -500;
            if (temp != -500) {
                maxTemp = max(maxTemp, temp);
                minTemp = min(minTemp, temp);
                temp_min[hiLoIndex] = minTemp;
                temp_max[hiLoIndex] = maxTemp;
            }

            JsonObject weather = forecast["weather"][0];
            iName = weather.containsKey("icon") ? (const char*)weather["icon"] : "unknown";
            if (dt >= nextNoon) {
                nextNoon += SECONDS_IN_DAY;
                iconNames[iconNameIndex] = iName;
                Serial.printf("DT: %d, Weather Icon: %s\n", dt, iName);
                iconNameIndex--;
            }

            if (dt > midnightLocal) {
                midnightLocal += SECONDS_IN_DAY;
                hiLoIndex--;
                temp_min[hiLoIndex] = -1;
                temp_max[hiLoIndex] = -1;
                maxTemp = -200;
                minTemp = 200;
                dIndex = (dIndex + 1) % 7;
                days[hiLoIndex] = dIndex;
            }
        }

        if (hiLoIndex == 1) {
            hiLoIndex--;
            dIndex = (dIndex + 1) % 7;
            days[hiLoIndex] = dIndex;
            if (temp != 500) {
                temp_min[hiLoIndex] = temp_max[hiLoIndex] = temp;
            }
        }

        // Sometimes the last forecast entry is before noon on that day
        if (iconNameIndex >= 0) {
            iconNames[iconNameIndex--] = iName;
        }

        // Just in case
        if (iconNameIndex >= 0) {
            for (int i=iconNameIndex; i--; i>=0) {
                iconNames[i] = "unknown";
            }
        }

        if(missing) {
            parsingError = true;
			serializeJson(doc, Serial);
			Serial.println("");
		}
	} else {
        parsingError = true;
		Serial.printf("Deserialize error while parsing weather: %s. Capacity=%d. Usage=%d\n", deserializeError.c_str(), doc.capacity(), doc.memoryUsage());
	}

    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

    return !parsingError;
}

bool OpenWeatherMapWeatherService::getWeatherInfo() {
    WiFiClientSecure client;

    const char *host = "api.openweathermap.org";
    const int httpsPort = 443; // HTTPS= 443 and HTTP = 80
    client.setInsecure();         // skip verification
    client.setTimeout(15 * 1000); // 15 Seconds

    Serial.println("HTTPS Connecting");
    int r = 0; // retry counter
    while ((!client.connect(host, httpsPort)) && (r < 10))
    {
        delay(200);
        r++;
    }

    if (!client.connected())
    {
        Serial.println("Failed to connect");
        return false;
    }

    return getCurrentWeatherInfo(client) && getForecastWeatherInfo(client);
}

#else
bool OpenWeatherMapWeatherService::getCurrentWeatherInfo()
{
    bool success = false;

    String token = getWeatherToken();

    if (token.isEmpty())
    {
        return false;
    }

    WiFiClientSecure *client = new WiFiClientSecure;
    if (client)
    {
        client->setInsecure();
        {
            // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
            HTTPClient https;

            const char *host = "api.openweathermap.org";

            String url = String("https://") + host + "/data/2.5/weather?lat=42.608089&lon=-71.571152&appid=" + token;
            Serial.print("[HTTPS] begin...\n");
            if (https.begin(*client, url))
            { // HTTPS
                Serial.print("[HTTPS] GET...\n");
                // start connection and send HTTP header
                int httpCode = https.GET();

                // httpCode will be negative on error
                if (httpCode > 0)
                {
                    // HTTP header has been send and Server response header has been handled
                    Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

                    // file found at server
                    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
                    {
                        success = true;
                        String payload = https.getString();
                        Serial.println(payload);
                    }
                }
                else
                {
                    Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
                }

                https.end();
            }
            else
            {
                Serial.printf("[HTTPS] Unable to connect\n");
            }

            // End extra scoping block
        }

        delete client;
    }
    else
    {
        Serial.println("Unable to create client");
    }

    return success;
}
#endif
#else
bool OpenWeatherMapWeatherService::getCurrentWeatherInfo() {
    bool success = false;

    String token = getWeatherToken();

    if (!token.isEmpty()) {
        String url = "https://api.openweathermap.org/data/2.5/weather?lat=42.608089&lon=-71.571152&appid=";
        url += token;

        httpClient.initialize(url);
    }

    return success;
}
#endif