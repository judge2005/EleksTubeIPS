#ifndef _IPS_CLOCK_H
#define _IPS_CLOCK_H

#include <ConfigItem.h>
#include <TimeSync.h>

#include "ClockTimer.h"
#include "ImageUnpacker.h"

class IPSClock {
public:
    IPSClock();

    static IntConfigItem& getTimeOrDate() { static IntConfigItem time_or_date("time_or_date", 0); return time_or_date; }	// time
    static ByteConfigItem& getDateFormat() { static ByteConfigItem date_format("date_format", 1); return date_format; }			// mm-dd-yy, dd-mm-yy, yy-mm-dd
    static BooleanConfigItem& getHourFormat() { static BooleanConfigItem hour_format("hour_format", true); return hour_format; }	// 12/24 hour
    static BooleanConfigItem& getShowSeconds() { static BooleanConfigItem show_seconds("show_seconds", true); return show_seconds; }	// 6 or 4 digit clock
    static BooleanConfigItem& getLeadingZero() { static BooleanConfigItem leading_zero("leading_zero", false); return leading_zero; }	//
    static ByteConfigItem& getDisplayOn() { static ByteConfigItem display_on("display_on", 6); return display_on; }
    static ByteConfigItem& getDisplayOff() { static ByteConfigItem display_off("display_off", 24); return display_off; }
    static StringConfigItem& getClockFace() { static StringConfigItem clock_face("clock_face", 25, "divergence"); return clock_face; }	// <clock_face>.tar.gz, max length is 31
    static StringConfigItem& getTimeZone() { static StringConfigItem time_zone("time_zone", 63, "EST5EDT,M3.2.0,M11.1.0"); return time_zone; }	// POSIX timezone format
    static IntConfigItem& getDimming() { static IntConfigItem dimming("dimming", 0); return dimming; }

    void init();
    void loop();
    void setTimeSync(TimeSync *pTimeSync) { this->pTimeSync = pTimeSync; }
    void setImageUnpacker(ImageUnpacker *imageUnpacker) { this->imageUnpacker = imageUnpacker; }

    bool clockOn();
    void setOnOverride() { onOverride = millis(); };
    void overrideUntilNextChange() { temporaryOverride = true; }

    uint8_t dimming() { return clockOn() ? 255 : 40; }
private:
    static char* digitToName[10];

    ClockTimer::Timer displayTimer;
    String oldClockFace;
	TimeSync *pTimeSync = 0;
    ImageUnpacker *imageUnpacker;
    unsigned long onOverride = 0;
    bool temporaryOverride = false;
    bool prevScheduleOn = false;
};

#endif