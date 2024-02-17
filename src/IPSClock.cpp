#include "TFTs.h"
#include "IPSClock.h"

char* IPSClock::digitToName[] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9"
};

IPSClock::IPSClock() {
}

void IPSClock::init() {
	displayTimer.init(millis(), 0);
    oldClockFace = getClockFace().value;
}

bool IPSClock::clockOn() {
    if (millis() - onOverride <= 10000) {
        return true;
    }
    
	struct tm now;
	suseconds_t uSec;
    bool scheduledOn = false;

	if (getDisplayOn().value == getDisplayOff().value) {
		scheduledOn = true;
	} else if (pTimeSync) {
        pTimeSync->getLocalTime(&now, &uSec);

        if (getDisplayOn().value < getDisplayOff().value) {
            scheduledOn = now.tm_hour >= getDisplayOn().value && now.tm_hour < getDisplayOff().value;
        } else if (getDisplayOn().value > getDisplayOff().value) {
            scheduledOn = !(now.tm_hour >= getDisplayOff().value && now.tm_hour < getDisplayOn().value);
        }
    }

    if (temporaryOverride) {
        if (prevScheduleOn == scheduledOn) {
            scheduledOn = !scheduledOn;
        } else {
            temporaryOverride = false;
        }
    }

	return scheduledOn;
}

void IPSClock::loop() {
    if (getClockFace().value != oldClockFace) {
        oldClockFace = getClockFace().value;
        imageUnpacker->unpackImages("/ips/faces/" + getClockFace().value, "/ips/cache");
        tfts->claim();
        tfts->invalidateAllDigits();
        tfts->release();
    }
    
    unsigned long nowMs = millis();

    if (displayTimer.expired(nowMs)) {
        struct tm now;
        suseconds_t uSec;
        pTimeSync->getLocalTime(&now, &uSec);
        suseconds_t realms = uSec / 1000;
        if (realms > 1000) {
            realms = realms % 1000;	// Something went wrong so pick a safe number for 1000 - realms...
        }
        unsigned long tDelay = 1000 - realms;

        if (clockOn() || (getDimming() == 1)) {
            tfts->claim();
            tfts->setDimming(dimming());
            tfts->setImageJustification(TFTs::MIDDLE_CENTER);
            tfts->setBox(tfts->width(), tfts->height());
            tfts->checkStatus();
            tfts->enableAllDisplays();
            // tfts->invalidateAllDigits();

            if (getTimeOrDate().value == 0) {
                uint8_t hour = now.tm_hour;

                // refresh starting on seconds
                if (getFourDigitDisplay() == 0) {
                    tfts->setDigit(SECONDS_ONES, digitToName[now.tm_sec % 10], TFTs::yes);
                    tfts->setDigit(SECONDS_TENS, digitToName[now.tm_sec / 10], TFTs::yes);
                    tfts->setDigit(MINUTES_ONES, digitToName[now.tm_min % 10], TFTs::yes);
                    tfts->setDigit(MINUTES_TENS, digitToName[now.tm_min / 10], TFTs::yes);
                } else {
                    if (getFourDigitDisplay() == 1) {
                        if (getHourFormat()) {  // true == show am/pm indicator
                            tfts->setDigit(SECONDS_ONES, hour < 12 ? "am" : "pm", TFTs::yes);
                        } else {
                            tfts->setDigit(SECONDS_ONES, "space", TFTs::yes);
                        }
                    }
                    tfts->setDigit(SECONDS_TENS, digitToName[now.tm_min % 10], TFTs::yes);
                    tfts->setDigit(MINUTES_ONES, digitToName[now.tm_min / 10], TFTs::yes);
                    if (now.tm_sec % 2 == 0) {
                        tfts->setDigit(MINUTES_TENS, "space", TFTs::yes);
                    } else {
                        tfts->setDigit(MINUTES_TENS, "colon", TFTs::yes);
                    }
                }

                if (getHourFormat()) {  // true = 12 hour display
                    if (now.tm_hour > 12) {
                        hour = now.tm_hour - 12;
                    } else if (now.tm_hour == 0) {
                        hour = 12;
                    }
                }

                tfts->setDigit(HOURS_ONES, digitToName[hour % 10], TFTs::yes);
                tfts->setDigit(HOURS_ONES, digitToName[hour % 10], TFTs::yes);
                if (hour < 10 && !getLeadingZero().value) {
                    tfts->setDigit(HOURS_TENS, "space", TFTs::yes);
                } else {
                    tfts->setDigit(HOURS_TENS, digitToName[hour / 10], TFTs::yes);
                }
            } else if (getTimeOrDate().value == 1) {
                uint8_t day = now.tm_mday;
                uint8_t month = now.tm_mon;
                uint8_t year = now.tm_year;

                switch (getDateFormat().value) {
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

                // refresh starting on 'seconds'
                tfts->setDigit(SECONDS_ONES, digitToName[year % 10], TFTs::yes);
                tfts->setDigit(SECONDS_TENS, digitToName[year / 10], TFTs::yes);
                tfts->setDigit(MINUTES_ONES, digitToName[month % 10], TFTs::yes);
                tfts->setDigit(MINUTES_TENS, digitToName[month / 10], TFTs::yes);
                tfts->setDigit(HOURS_ONES, digitToName[day % 10], TFTs::yes);
                tfts->setDigit(HOURS_TENS, digitToName[day / 10], TFTs::yes);
            } else {
                Serial.println("Bad display state for clock");
            }
        } else {
            tfts->disableAllDisplays();
        }

        tfts->release();

        displayTimer.init(nowMs, tDelay);
    }
}
