#ifndef _IPS_CLOCK_H
#define _IPS_CLOCK_H

#include <ConfigItem.h>
#include <TimeSync.h>
#include <ESP32-targz.h>

#include "TFTs.h"
#include "ClockTimer.h"

class IPSClock {
public:
    IPSClock();

    static BooleanConfigItem& getTimeOrDate() { static BooleanConfigItem time_or_date("time_or_date", true); return time_or_date; }	// time
    static ByteConfigItem& getDateFormat() { static ByteConfigItem date_format("date_format", 1); return date_format; }			// mm-dd-yy, dd-mm-yy, yy-mm-dd
    static BooleanConfigItem& getHourFormat() { static BooleanConfigItem hour_format("hour_format", true); return hour_format; }	// 12/24 hour
    static BooleanConfigItem& getLeadingZero() { static BooleanConfigItem leading_zero("leading_zero", false); return leading_zero; }	//
    static ByteConfigItem& getDisplayOn() { static ByteConfigItem display_on("display_on", 6); return display_on; }
    static ByteConfigItem& getDisplayOff() { static ByteConfigItem display_off("display_off", 24); return display_off; }
    static StringConfigItem& getClockFace() { static StringConfigItem clock_face("clock_face", 25, "divergence"); return clock_face; }	// <clock_face>.tar.gz, max length is 31
    static StringConfigItem& getTimeZone() { static StringConfigItem time_zone("time_zone", 63, "EST5EDT,M3.2.0,M11.1.0"); return time_zone; }	// POSIX timezone format

    void init();
    void loop();
    void setDimming(bool dimming) { this->dimming = dimming; }
    void setTimeSync(TimeSync *pTimeSync) { this->pTimeSync = pTimeSync; }

    bool clockOn();
private:
    ClockTimer::Timer displayTimer;
    String oldClockFace;
	TimeSync *pTimeSync = 0;
    bool dimming = false;

    static bool newUnpack;
    static const char* unpackName;

    static TarGzUnpacker& getUnpacker();

    static void statusProgressCallback(const char* name, size_t size, size_t total_unpacked);
    static void unpackProgressCallback(uint8_t progress);
    void cacheClockFace(const String &faceName);
};

#endif