// Useful docs:
//   https://arduino-esp8266.readthedocs.io/en/latest/
//
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
//
// Library WifiManager (tzapu)
//   https://github.com/tzapu/WiFiManager
//
//  Default IP to connect to on first start is 192.168.4.1.
//  Captive portal should work, but isn't happening for me.
#include <WiFiManager.h>  
#include <ESP8266HTTPClient.h>
#include "SensorInterface.h"
#include "DeviceConfig.h"
#include "SampleBuffer.h"
#include "CloudInterface.h"
#include "DeviceWebServer.h"

#define WIFI_CONFIG_NAME "BrewBeerSensor"
#define MDNS_NAME "BrewBeer"  // BrewBeer.local
#define OTA_HOSTNAME MDNS_NAME
#define OTA_PASSWORD MDNS_NAME "1"  

#define RELAY_OUTPUT 16        // GPIO16
#define SERIAL_PORT_BPS 115200

WiFiManager wifiManager;

DeviceConfig config;
SensorInterface sensor;
CloudInterface cloudInterface;
SampleBuffer samples;
DeviceWebServer webServer(config, samples);

// ----------------------------------------------------------------------

void setup()
{
  Serial.begin(SERIAL_PORT_BPS);
  Serial.println();
  
  if(!LittleFS.begin()) {
    Serial.println(F("Failed to start filesystem!"));
  }

  config.ReadFromFS(); // Read any stored configuration.
  samples.ReadFromFS();  // Read any existing data.

  samples.OnSampleIndexChange( []() {
    String result = "CloudInterface: ";
    result += cloudInterface.WriteDataToCloud(samples, config);
    Serial.println(result);
  });
  
  sensor.Setup();

  setupRelay();
  setupWifi();
  
  webServer.Setup();

  webServer.OnRootCertChanged( []() {
    cloudInterface.LoadRootCert();
  });

  webServer.OnTestCall( []() {
    return cloudInterface.WriteDataToCloud(samples, config);
  });
  
  webServer.OnResetWiFiSettings( []() { 
    wifiManager.erase();
    ESP.restart();
  });

  // Setup the inbuilt NTP server with config loaded or obtained via the WiFi manager.
  String timezone = String("UTC+") + String(config.timezone_offset);
  configTime(timezone.c_str(), "pool.ntp.org");

  if(!MDNS.begin(MDNS_NAME)) {
    Serial.println(F("Failed to setup MDNS responder!"));
  }
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
}

// Used by the WiFi manager when configuring the timezone.
bool wifiManagerConfigChanged = false;

void saveConfigCallback () {
  wifiManagerConfigChanged = true;
}

void setupWifi() {
  wifiManager.setConfigPortalTimeout(300);  // 5 minutes for configuration
  
  char str_tz_offset[6];
  sprintf(str_tz_offset, "%d", config.timezone_offset);
  WiFiManagerParameter timezoneParam("timezone_offset", "Timezone Offset", str_tz_offset, 3);
  wifiManager.addParameter(&timezoneParam);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.autoConnect(WIFI_CONFIG_NAME);
  
  config.SetTimezoneOffset(atoi(timezoneParam.getValue()));
  
  // If the timezone was set via the WiFi setup, then save it.
  if(wifiManagerConfigChanged) {
    config.WriteToFS();
    wifiManagerConfigChanged = false;
  }
}

void setupRelay() {
  pinMode(RELAY_OUTPUT, OUTPUT);

  // Start with the relay off.
  digitalWrite(RELAY_OUTPUT, LOW);
}

// ----------------------------------------------------------------------

const int LOOP_DELAY = 1000;
int loopCount = LOOP_DELAY;

void loop() {
  MDNS.update();                       // Some tutorials leave this out, but it doesn't work without it.
  webServer.handleClient();            // Listen for HTTP requests from clients
  ArduinoOTA.handle();
  
  delay(10);    
  
  loopCount++;
  if(loopCount > LOOP_DELAY)
  {
    // If the webserver has successfully recorded the startup time, then the NTP time looks valid,
    //  and processing can start.
    if(webServer.RecordStartupTime())
    {
      sensor.RecordTemperature(samples);
      switchRelay();
    }
    loopCount = 0;
  }
}


void switchRelay() {
  if(samples.current_temp < config.relay_on_below_temp) {
    digitalWrite(RELAY_OUTPUT, HIGH);
  }

  if(samples.current_temp > config.relay_off_above_temp) {
    digitalWrite(RELAY_OUTPUT, LOW);
  }
}
