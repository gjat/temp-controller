#include "DeviceConfig.h"
#include "CsvHelpers.h"

const char CONFIG_FILE[] =  "/config.csv";
const char CONFIG_FILE_BACKUP[] = "/config.bkp";

void DeviceConfig::SetTimezoneOffset(int timezoneOffset) {
  if(timezone_offset >= -12 && timezone_offset <= 12)
    timezone_offset = timezoneOffset;
}

bool DeviceConfig::ReadFromFS() {
  File file;
  
  if(LittleFS.exists(CONFIG_FILE)) {
    file = LittleFS.open(CONFIG_FILE, "r");
  } else if(LittleFS.exists(CONFIG_FILE_BACKUP)) {
    file = LittleFS.open(CONFIG_FILE_BACKUP, "r");
  }

  if(file) {
    // Read from file
    timezone_offset = file.parseInt();
    if(timezone_offset < -12 || timezone_offset > 12)
      timezone_offset = DEFAULT_TIMEZONE_OFFSET;

    relay_on_below_temp = file.parseFloat();
    relay_off_above_temp = file.parseFloat();

    // Sanity check these values, because the relay logic
    //  wouldn't behave well if they were upside down.
    if(relay_on_below_temp >= relay_off_above_temp)
    {
      relay_on_below_temp = DEFAULT_RELAY_ON_BELOW_TEMP;
      relay_off_above_temp = DEFAULT_RELAY_OFF_ABOVE_TEMP;
    }

    file.readStringUntil(',');  // Throw away the delimiter, that a "parse" would have skipped.

    cloudLoggingUrl = file.readStringUntil(',');
    cloudLoggingApiKey = file.readStringUntil(',');
    cloudInstanceId = file.readStringUntil(',');
    file.close();
    return true;
  }
  return false;
}

void DeviceConfig::WriteToFS() {

  if(LittleFS.exists(CONFIG_FILE)) {
    LittleFS.remove(CONFIG_FILE_BACKUP);
    LittleFS.rename(CONFIG_FILE, CONFIG_FILE_BACKUP);
  }

  File file = LittleFS.open(CONFIG_FILE, "w");
  CsvHelpers::writeInt(file, timezone_offset);

  CsvHelpers::writeFloat(file, relay_on_below_temp);
  CsvHelpers::writeFloat(file, relay_off_above_temp);

  CsvHelpers::writeString(file, cloudLoggingUrl);
  CsvHelpers::writeString(file, cloudLoggingApiKey);
  CsvHelpers::writeString(file, cloudInstanceId);
  file.flush();
  file.close();
  
  // The root cert is handled separately, stored in
  //  a separate file, written by the web server via 
  //  a web upload, and loaded directly by the CloudInterface
}
