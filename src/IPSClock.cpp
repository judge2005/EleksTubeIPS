#include <LittleFS.h>

#include "IPSClock.h"

IPSClock::IPSClock() {
}

void IPSClock::init() {
	displayTimer.init(millis(), 0);
}

bool IPSClock::clockOn() {
	struct tm now;
	suseconds_t uSec;

	if (getDisplayOn().value == getDisplayOff().value) {
		return true;
	}

    if (pTimeSync) {
        pTimeSync->getLocalTime(&now, &uSec);

        if (getDisplayOn().value < getDisplayOff().value) {
            return now.tm_hour >= getDisplayOn().value && now.tm_hour < getDisplayOff().value;
        }

        if (getDisplayOn().value > getDisplayOff().value) {
            return !(now.tm_hour >= getDisplayOff().value && now.tm_hour < getDisplayOn().value);
        }
    }

	return false;
}

void IPSClock::loop() {
    if (getClockFace().value != oldClockFace) {
        oldClockFace = getClockFace().value;
        cacheClockFace(getClockFace().value);
        tfts.claim();
        tfts.invalidateAllDigits();
        tfts.release();
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

        if (clockOn() || dimming) {
            tfts.claim();
            tfts.setDimming(dimming ? 40 : 255);
            tfts.checkStatus();
            tfts.enableAllDisplays();
            if (getTimeOrDate().value) {
                uint8_t hour = now.tm_hour;
                if (getHourFormat().value) {
                    if (now.tm_hour > 12) {
                        hour = now.tm_hour - 12;
                    } else if (now.tm_hour == 0) {
                        hour = 12;
                    }
                }

                // refresh starting on seconds
                tfts.setDigit(SECONDS_ONES, now.tm_sec % 10, TFTs::yes);
                tfts.setDigit(SECONDS_TENS, now.tm_sec / 10, TFTs::yes);
                tfts.setDigit(MINUTES_ONES, now.tm_min % 10, TFTs::yes);
                tfts.setDigit(MINUTES_TENS, now.tm_min / 10, TFTs::yes);
                tfts.setDigit(HOURS_ONES, hour % 10, TFTs::yes);
                if (hour < 10 && !getLeadingZero().value) {
                    tfts.setDigit(HOURS_TENS, TFTs::blanked, TFTs::yes);
                } else {
                    tfts.setDigit(HOURS_TENS, hour / 10, TFTs::yes);
                }
            } else {
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
                tfts.setDigit(SECONDS_ONES, year % 10, TFTs::yes);
                tfts.setDigit(SECONDS_TENS, year / 10, TFTs::yes);
                tfts.setDigit(MINUTES_ONES, month % 10, TFTs::yes);
                tfts.setDigit(MINUTES_TENS, month / 10, TFTs::yes);
                tfts.setDigit(HOURS_ONES, day % 10, TFTs::yes);
                tfts.setDigit(HOURS_TENS, day / 10, TFTs::yes);
            }
        } else {
            tfts.disableAllDisplays();
        }

        tfts.release();

        displayTimer.init(nowMs, tDelay);
    }
}

bool IPSClock::newUnpack = true;
const char* IPSClock::unpackName = "";

TarGzUnpacker &IPSClock::getUnpacker() {
    static TarGzUnpacker *unpacker = 0;

    if (unpacker == 0) {
        unpacker = new TarGzUnpacker();
        unpacker->setTarVerify( true ); // true = enables health checks but slows down the overall process
        unpacker->setupFSCallbacks( targzTotalBytesFn, targzFreeBytesFn ); // prevent the partition from exploding, recommended
        unpacker->setGzProgressCallback( unpackProgressCallback ); // targzNullProgressCallback or defaultProgressCallback
        unpacker->setLoggerCallback( BaseUnpacker::targzPrintLoggerCallback  );    // gz log verbosity
        unpacker->setTarStatusProgressCallback( statusProgressCallback ); // print the filenames as they're expanded
        unpacker->setTarMessageCallback( BaseUnpacker::targzPrintLoggerCallback ); // tar log verbosity
    }

    return *unpacker;
}

void IPSClock::statusProgressCallback(const char* name, size_t size, size_t total_unpacked) {
	unpackName = name;
}

void IPSClock::unpackProgressCallback(uint8_t progress) {
	tfts.drawMeter(progress, newUnpack, unpackName);
	newUnpack = false;
}

void IPSClock::cacheClockFace(const String &faceName) {
	String fileName("/ips/faces/" + faceName + ".tar.gz");

    if (LittleFS.exists(fileName)) {
        newUnpack = true;

        fs::File dir = LittleFS.open("/ips/cache");
        String name = dir.getNextFileName();
        while(name.length() > 0){
            LittleFS.remove(name);
            name = dir.getNextFileName();
        }
        dir.close();

        TarGzUnpacker &unpacker = getUnpacker();

        if( !unpacker.tarGzExpander(tarGzFS, fileName.c_str(), tarGzFS, "/ips/cache", nullptr ) ) {
            Serial.printf("tarGzExpander+intermediate file failed with return code #%d\n", unpacker.tarGzGetError() );
        } else {
            Serial.println("File unzipped");
        }

        dir = LittleFS.open("/ips/cache");
        File file = dir.openNextFile();
        while(file){
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");

    #ifdef CONFIG_LITTLEFS_FOR_IDF_3_2
            Serial.println(file.size());
    #else
            Serial.print(file.size());
            time_t t= file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n",(tmstruct->tm_year)+1900,( tmstruct->tm_mon)+1, tmstruct->tm_mday,tmstruct->tm_hour , tmstruct->tm_min, tmstruct->tm_sec);
    #endif
            file.close();
            file = dir.openNextFile();
        }
        dir.close();
    }
}
