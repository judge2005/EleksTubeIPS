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
    static ByteConfigItem& getFourDigitDisplay() { static ByteConfigItem four_digit_display("four_digit_display", 2); return four_digit_display; }	// 6 or 4 digit clock or 4 digits + weather
    static BooleanConfigItem& getLeadingZero() { static BooleanConfigItem leading_zero("leading_zero", true); return leading_zero; }	//
    static ByteConfigItem& getDisplayOn() { static ByteConfigItem display_on("display_on", 0); return display_on; }
    static ByteConfigItem& getDisplayOff() { static ByteConfigItem display_off("display_off", 24); return display_off; }
    static StringConfigItem& getClockFace() { static StringConfigItem clock_face("clock_face", 25, "original"); return clock_face; }	// <clock_face>.tar.gz, max length is 31
    static StringConfigItem& getTimeZone() { static StringConfigItem time_zone("time_zone", 63, "EST5EDT,M3.2.0,M11.1.0"); return time_zone; }	// POSIX timezone format
    static IntConfigItem& getDimming() { static IntConfigItem dimming("dimming", 2); return dimming; }
    static ByteConfigItem& getBrightnessConfig() { static ByteConfigItem brightness_config("brightness_config", 255); return brightness_config; }
    static StringConfigItem& getCustomData() { static StringConfigItem custom_data("custom_data", 10, ""); return custom_data; }	// Custom data for MQTT

    void init();
    void loop();
    void setTimeSync(TimeSync *pTimeSync) { this->pTimeSync = pTimeSync; }
    void setImageUnpacker(ImageUnpacker *imageUnpacker) { this->imageUnpacker = imageUnpacker; }

    bool clockOn();
    void setOnOverride() { onOverride = millis(); };
    void overrideUntilNextChange() { prevScheduleOn = clockOn(); temporaryOverride = true; }
    void setBrightness(byte brightness) { this->brightness = brightness; }
    uint8_t getBrightness() { return getDimming() == 1 && brightness == 255 && !clockOn() ? 40 : brightness; }
private:
    static char* digitToName[10];

    byte brightness = 255;
    ClockTimer::Timer displayTimer;
    String oldClockFace;
	TimeSync *pTimeSync = 0;
    ImageUnpacker *imageUnpacker;
    unsigned long onOverride = 0;
    bool temporaryOverride = false;
    bool prevScheduleOn = false;
};

#endif