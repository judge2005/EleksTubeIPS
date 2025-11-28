#include "TFTs.h"
#include "IPSClock.h"

extern void broadcastUpdate(const BaseConfigItem& item);
extern void broadcastFSChange();

IRAMPtrArray<const char*> IPSClock::digitToName {
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

void IPSClock::checkIconPack() {
    if (getClockFace().value != oldClockFace) {
        getClockFace() = imageUnpacker->unpackImages("/ips/faces/", "/ips/cache", getClockFace(), oldClockFace);
        getClockFace().put();
        broadcastUpdate(getClockFace());
        broadcastFSChange();
        oldClockFace = getClockFace();
        tfts->claim();
        tfts->invalidateAllDigits();
        tfts->release();
    }
}

void IPSClock::loop() {
    unsigned long nowMs = millis();

    // display refresh
    if (displayTimer.expired(nowMs)) {
        struct tm now;
        suseconds_t uSec;
        pTimeSync->getLocalTime(&now, &uSec);
        suseconds_t realms = uSec / 1000;
        if (realms > 1000) {
            realms = realms % 1000;	// Something went wrong so pick a safe number for 1000 - realms...
        }
        unsigned long tDelay = 1000 - realms;

        if (clockOn() || (getDimming() == DIM)) {
            tfts->claim();
            tfts->setDimming(getBrightness());
            tfts->setImageJustification(TFTs::MIDDLE_CENTER);
            tfts->setBox(tfts->width(), tfts->height());
            tfts->checkStatus();
            tfts->enableAllDisplays();
            // tfts->invalidateAllDigits();

            // Display custom data if available: 
            uint8_t customDataLength = getCustomData().value.length();
            if (customDataLength > 0) {
                for (uint8_t i = 0; i < NUM_DIGITS; i++) {
                    char name[10];
                    // no letter found for this digit -> use space
                    if (i >= customDataLength) {
                        strcpy(name, "space");
                    }
                    else 
                    {
                        char value = getCustomData().value[i];
                        if (value == '_' or value == ' ') {
                            strcpy(name, "space");
                        } else if (value == ':') {
                            strcpy(name, "colon");
                        } else if (value == 'a') {
                            strcpy(name, "am");
                        } else if (value == 'p') {
                            strcpy(name, "pm"); 
                        } else if (value >= '0' && value <= '9') {
                            name[0] = value;
                            name[1] = 0;
                        } 
                        // not a digit colon or space -> show as space
                        else {
                            strcpy(name, "space");
                        }

                    }
                    uint8_t DIGITS[NUM_DIGITS] = {
                        HOURS_TENS,
                        HOURS_ONES,
                        MINUTES_TENS,
                        MINUTES_ONES,
                        SECONDS_TENS,
                        SECONDS_ONES
                    };
                    tfts->setDigit(DIGITS[i], name, TFTs::yes);
                }
            }
            // Display time: 
            else if (getTimeOrDate().value == TIME) {
                uint8_t hour = now.tm_hour;

                // refresh starting on seconds
                if (getFourDigitDisplay() == SIX) {
                    tfts->setDigit(SECONDS_ONES, digitToName[now.tm_sec % 10], TFTs::yes);
                    tfts->setDigit(SECONDS_TENS, digitToName[now.tm_sec / 10], TFTs::yes);
                    tfts->setDigit(MINUTES_ONES, digitToName[now.tm_min % 10], TFTs::yes);
                    tfts->setDigit(MINUTES_TENS, digitToName[now.tm_min / 10], TFTs::yes);
                } else {
                    if (getFourDigitDisplay() == FOUR) {
                        if (getHourFormat()) {  // true == show am/pm indicator
                            tfts->setDigit(SECONDS_ONES, hour < 12 ? "am" : "pm", TFTs::yes);
                        } else {
                            tfts->setDigit(SECONDS_ONES, "space", TFTs::yes);
                        }
                    } else if (getFourDigitDisplay() == FOUR_WITH_SLIDESHOW && now.tm_sec % 10 == 3) {
                        tfts->setShowDigits(SLIDE_SHOW);
                        tfts->setDigit(SECONDS_ONES, digitToName[random(10)], TFTs::yes);
                        tfts->setShowDigits(TIME);
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
            } 
            // Display Date: 
            else if (getTimeOrDate().value == DATE) {
                uint8_t day = now.tm_mday;
                uint8_t month = now.tm_mon;
                uint8_t year = now.tm_year;

                switch (getDateFormat().value) {
                case EURO:	// DD-MM-YY
                    break;
                case USA: // MM-DD-YY
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
            } else if (getTimeOrDate().value == SLIDE_SHOW) {
                if (strcmp(TFTs::INVALID_DIGIT, tfts->getDigitName(SECONDS_ONES)) == 0) {
                    tfts->setDigit(SECONDS_ONES, digitToName[0], TFTs::yes);
                    tfts->setDigit(SECONDS_TENS, digitToName[1], TFTs::yes);
                    tfts->setDigit(MINUTES_ONES, digitToName[2], TFTs::yes);
                    tfts->setDigit(MINUTES_TENS, digitToName[3], TFTs::yes);
                    tfts->setDigit(HOURS_ONES, digitToName[4], TFTs::yes);
                    tfts->setDigit(HOURS_TENS, digitToName[5], TFTs::yes);
                }
                if (now.tm_sec % 10 == 0) {
                    tfts->setDigit(random(6), digitToName[random(10)], TFTs::yes);
                }
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
