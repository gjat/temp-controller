#include <Arduino.h>
#include <LittleFS.h>

#ifndef __DEVICE_CONFIG__
#define __DEVICE_CONFIG__

class DeviceConfig 
{
public:
    // Because I'm in NZ, this is UTC+12 hours.  Feel free to change the default for your timezone
    //  This can also be entered via the WifiManager, when connecting to your wifi network.
    static const long DEFAULT_TIMEZONE_OFFSET = 12;
    long timezone_offset = DEFAULT_TIMEZONE_OFFSET;

    String cloudLoggingUrl;
    String cloudLoggingApiKey;
    String cloudInstanceId = "1";

    const float DEFAULT_RELAY_ON_BELOW_TEMP = 21.0;
    const float DEFAULT_RELAY_OFF_ABOVE_TEMP = 21.5;

    float relay_on_below_temp = DEFAULT_RELAY_ON_BELOW_TEMP;
    float relay_off_above_temp = DEFAULT_RELAY_OFF_ABOVE_TEMP;

public:
    void SetTimezoneOffset(int timezoneOffset);

    bool ReadFromFS();
    void WriteToFS();
};

#endif // __DEVICE_CONFIG__
