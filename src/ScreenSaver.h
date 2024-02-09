#ifndef _IPS_SCREEN_SAVER_H_
#define _IPS_SCREEN_SAVER_H_

#include <Arduino.h>

class ScreenSaver {
public:
	ScreenSaver() : resetTime(0) {
	}

    static ByteConfigItem& getScreenSaver() { static ByteConfigItem screen_saver("screen_saver", 1); return screen_saver; }
    static IntConfigItem& getScreenSaverDelay() { static IntConfigItem screen_saver_delay("screen_saver_delay", 20); return screen_saver_delay; }

	unsigned long getDelayMs() {
		return getScreenSaverDelay() * 60000;
	}

    void start() {
        forceOn = true;
    }

	bool isOn() {
		return !isOff();
	}

	bool isOff() {
		if (forceOn) {
			return false;
		}

		unsigned long nowMs = millis();

        unsigned long delayMs = getDelayMs();
		return (delayMs == 0) || (nowMs - resetTime <= delayMs);	// So zero = always off
	}

	bool reset() {
        bool wasOn = isOn();

		forceOn = false;
		
		resetTime = millis();	// Sensor will stay high while movement is detected

        return wasOn;
	}

private:
	unsigned long resetTime;
	bool forceOn = false;
};

#endif /* _IPS_SCREEN_SAVER_H_ */
