#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <WiFiUdp.h>
// Library NTPClient
//   https://github.com/arduino-libraries/NTPClient
#include <NTPClient.h>
#include <LittleFS.h>
// Library WifiManager (tzapu)
//   https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>  

#define WIFI_CONFIG_NAME "BrewBeerSensor"
#define MDNS_NAME "BrewBeer"  // BrewBeer.local
#define OTA_HOSTNAME MDNS_NAME
#define OTA_PASSWORD MDNS_NAME "1"  

// Because I'm in NZ, this is UTC+12 hours.  Change for your timezone
//  Can be entered via the WifiManager.
const long DEFAULT_TIMEZONE_OFFSET = 12;
long timezone_offset = DEFAULT_TIMEZONE_OFFSET;
const long NTP_UPDATE_INTERVAL_MS = 600000; // 10 minutes in ms


const long SECONDS_PER_MINUTE = 60;
const long SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
const long SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;

#define DS1621_ADDRESS_1 0x48  // All address pins connect to ground
#define RELAY_OUTPUT 16        // ??
#define SERIAL_PORT_BPS 115200

// Files used to store previous data and config over a reboot
#define DATA_FILE "/avgs.csv"
#define CONFIG_FILE "/config.csv"

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
WiFiManager wifiManager;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void handleRoot();              // function prototypes for HTTP handlers
void handleJson();
void handleNotFound();

// Immediate values, min and max.
const float MIN_ACCEPTED_TEMP = 0.0;
const float MAX_ACCEPTED_TEMP = 100;
float min_temp = MAX_ACCEPTED_TEMP;
float max_temp = MIN_ACCEPTED_TEMP;
float current_temp = 0.0;

// Temps to turn the relay on and off at.
const float DEFAULT_RELAY_ON_BELOW_TEMP = 22.0;
const float DEFAULT_RELAY_OFF_ABOVE_TEMP = 23.0;
float relay_on_below_temp = DEFAULT_RELAY_ON_BELOW_TEMP;
float relay_off_above_temp = DEFAULT_RELAY_OFF_ABOVE_TEMP;

// Sample configuration.  Samples per day (per hour, per half hour), determining
//   minutes per sample.
const int SAMPLES_PER_DAY = 48;
const int MINUTES_PER_DAY = 24 * 60;
const int MINUTES_PER_SAMPLE = MINUTES_PER_DAY / SAMPLES_PER_DAY;

// Sample storage
const int NUM_SAMPLES = SAMPLES_PER_DAY;
int sample_average_counts[NUM_SAMPLES];
float sample_average_temps[NUM_SAMPLES];

// ----------------------------------------------------------------------

void setup()
{
  initSampleRecords();
  setupSerial();
  
  if(!LittleFS.begin()) {
    Serial.println(F("Failed to start filesystem!"));
  }

  readConfigFromFS();

  setupI2C();
  initialiseTempSensor();
  setupRelay();
  setupWifi();
  setupWebServer();

  // The wifi manager prompts for this.
  timeClient.setTimeOffset(timezone_offset * SECONDS_PER_HOUR);
  timeClient.setUpdateInterval(NTP_UPDATE_INTERVAL_MS);
  timeClient.begin();

  if(!MDNS.begin(MDNS_NAME)) {
    Serial.println(F("Failed to setup MDNS responder!"));
  }
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
}

void resetMinMaxTemps(bool saveChanges = false) {
  min_temp = 50.0;
  max_temp = 0.0;

  if(saveChanges)
    writeDataToFS();
}

void initSampleRecords() {
  int i;
  for(i=0; i<NUM_SAMPLES; i++) {
    sample_average_counts[i] = 0;
    sample_average_temps[i] = 0;
  }

  resetMinMaxTemps();
}

void setupSerial() {
  Serial.begin(SERIAL_PORT_BPS);
  Serial.println();
}

//flag for saving data
bool wifiManagerConfigChanged = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  wifiManagerConfigChanged = true;
}

void setupWifi() {
  wifiManager.setConfigPortalTimeout(300);  // 5 minutes for configuration
  
  char str_tz_offset[6];
  sprintf(str_tz_offset, "%d", timezone_offset);
  WiFiManagerParameter timezoneParam("timezone_offset", "Timezone Offset", str_tz_offset, 3);
  wifiManager.addParameter(&timezoneParam);
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  wifiManager.autoConnect(WIFI_CONFIG_NAME);
  
  timezone_offset = atoi(timezoneParam.getValue());
  if(timezone_offset < -12 || timezone_offset > 12)
    timezone_offset = DEFAULT_TIMEZONE_OFFSET;
  
  if(wifiManagerConfigChanged)
    writeConfigToFS();
}

void setupWebServer() {
  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/json", handleJson);
  server.onNotFound(handleNotFound);        // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  server.begin();                           // Actually start the server
}

void setupI2C() {
  Wire.begin();
  Wire.setClock(400000);
}

void initialiseTempSensor() {
  // Assumes setupI2C has been called
  Wire.beginTransmission(DS1621_ADDRESS_1);
  Wire.write(0xAC);                         // send configuration register address (Access Config)
  Wire.write(0);                            // perform continuous conversion
  Wire.beginTransmission(DS1621_ADDRESS_1); // send repeated start condition
  Wire.write(0xEE);                         // send start temperature conversion command
  Wire.endTransmission();                   // stop transmission and release the I2C 
}

void setupRelay() {
  pinMode(RELAY_OUTPUT, OUTPUT);

  // Start with the relay off.
  digitalWrite(RELAY_OUTPUT, LOW);
}

// ----------------------------------------------------------------------

int current_sample = 0;
bool inStartup = true;
unsigned long startup_time = 0;

const int LOOP_DELAY = 1000;
int loopCount = LOOP_DELAY;

void loop() {
  MDNS.update();                       // Some tutorials leave this out.  Ugh.
  server.handleClient();               // Listen for HTTP requests from clients
  ArduinoOTA.handle();
  timeClient.update();
  
  delay(10);    
  
  loopCount++;
  if(loopCount > LOOP_DELAY)
  {
    if(timeClient.isTimeSet())
    {
      // Read any values stored in the filesystem, if this is the first time around.
      if(inStartup) {
        startup_time = timeClient.getEpochTime();
        readDataFromFS();
        inStartup = false;
      }
      readTemperature();
      switchRelay();
    }
    loopCount = 0;
  }
}

// ----------------------------------------------------------------------
// File stuff

void writeFloat(File file, float value) {
  file.print(String(value, 1));
  file.print(",");
}

void writeInt(File file, int value) {
  file.print(String(value));
  file.print(",");
}

void readConfigFromFS() {
  if(LittleFS.exists(CONFIG_FILE)) {
    File file = LittleFS.open(CONFIG_FILE, "r");

    // Read from file
    timezone_offset = file.parseInt();
    if(timezone_offset < -12 || timezone_offset > 12)
      timezone_offset = DEFAULT_TIMEZONE_OFFSET;
    file.close();
  }
}

void writeConfigToFS() {
  File file = LittleFS.open(CONFIG_FILE, "w");
  writeInt(file, timezone_offset);
  delay(1);
  file.close();  
}

void writeDataToFS() {

  File file = LittleFS.open(DATA_FILE, "w");
  writeFloat(file, current_temp);
  writeFloat(file, min_temp);
  writeFloat(file, max_temp);
  writeFloat(file, relay_on_below_temp);
  writeFloat(file, relay_off_above_temp);
  
  int i = 0;
  for(i = 0; i < NUM_SAMPLES; i++) {
    writeFloat(file, sample_average_temps[i]);
    writeInt(file, sample_average_counts[i]);
  }

  // Not sure why, but I think it helps the FS.
  delay(1);

  //Close the file
  file.close(); 
}

void readDataFromFS() {
  
  if(LittleFS.exists(DATA_FILE)) {
    File file = LittleFS.open(DATA_FILE, "r");
    current_temp = file.parseFloat();  
    min_temp = file.parseFloat();
    max_temp = file.parseFloat();
    relay_on_below_temp = file.parseFloat();
    relay_off_above_temp = file.parseFloat();

    // Sanity check these values, because the relay logic
    //  wouldn't behave well if they were upside down.
    if(relay_on_below_temp >= relay_off_above_temp)
    {
      relay_on_below_temp = DEFAULT_RELAY_ON_BELOW_TEMP;
      relay_off_above_temp = DEFAULT_RELAY_OFF_ABOVE_TEMP;
    }
    
    int i = 0;
    for(i = 0; i < NUM_SAMPLES; i++) {
      sample_average_temps[i] = file.parseFloat();
      sample_average_counts[i] = file.parseInt();
    }
    file.close();
  }
}

// ----------------------------------------------------------------------
//  Read from the sensor

void readTemperature() {

  float now_temp = get_temperature();
  if(now_temp < MIN_ACCEPTED_TEMP || now_temp > MAX_ACCEPTED_TEMP)
  {
    Serial.println("Temp outside range.  Ignoring it.");
    // Try and re-initialise the temp sensor.  Perhaps it has power-cycled, and needs to 
    //  be told to start conversions again.
    initialiseTempSensor();
    return;
  }
  
  current_temp = now_temp;

  // Update min and max
  if (current_temp < min_temp) 
    min_temp = current_temp;

  if (current_temp > max_temp) 
    max_temp = current_temp;

  // Calculate which sample timeslot we're in
  int nowSample = (timeClient.getHours() * 60 + timeClient.getMinutes()) / MINUTES_PER_SAMPLE;
  
  // Update the sample record
  if(nowSample != current_sample)
  {
    current_sample = nowSample;
    sample_average_temps[nowSample] = current_temp;
    sample_average_counts[nowSample] = 1;
    writeDataToFS();
  }
  else
  {
    sample_average_temps[nowSample] = ((sample_average_temps[nowSample] * sample_average_counts[nowSample]) + current_temp) / (sample_average_counts[nowSample] + 1);
    sample_average_counts[nowSample]++;
  }

  Serial.println(getTempReport());
  Serial.println(timeClient.getFormattedTime());
}

float get_temperature() {
  float temp = 0;
  Wire.beginTransmission(DS1621_ADDRESS_1); // connect to DS1621 (send DS1621 address)
  Wire.write(0xAA);                       // read temperature command
  Wire.endTransmission(false);            // send repeated start condition
  Wire.requestFrom(DS1621_ADDRESS_1, 2);  // request 2 bytes from DS1621 and release I2C bus at end of reading
  uint8_t t_msb = Wire.read();            // read temperature MSB register
  uint8_t t_lsb = Wire.read();            // read temperature LSB register
 
  temp = t_msb;
  if(t_lsb)
    temp += 0.5;
  return temp;
}


void switchRelay() {
  if(current_temp < relay_on_below_temp) {
    digitalWrite(RELAY_OUTPUT, HIGH);
  }

  if(current_temp > relay_off_above_temp) {
    digitalWrite(RELAY_OUTPUT, LOW);
  }
}


// ----------------------------------------------------------------------
//  Make "reports"
String getTempReport() {
  char tempBuf[20];
  sprintf_P(tempBuf, PSTR("Now: %0.1f C,  Min: %0.1f C,  Max: %0.1f C"), current_temp, min_temp, max_temp);
  return String(tempBuf);
}

String getTimeFromSampleIndex(int sampleIdx) {
  int sample_time_minutes = sampleIdx * MINUTES_PER_SAMPLE;
  char tempBuf[6];
  sprintf(tempBuf, "%02d:%02d", sample_time_minutes / 60, sample_time_minutes % 60);
  return String(tempBuf);
}


String getChart(int startAtSample) {
  String labels;
  String data_values;
  int sample;
  for(sample = startAtSample; sample < NUM_SAMPLES; sample++) {
    if(sample > startAtSample) {
      labels += ",";  data_values += ",";
    }
    labels += "\"" + getTimeFromSampleIndex(sample) + "\"";
    data_values += String(sample_average_temps[sample], 1);
  }
  
  for(sample = 0; sample < startAtSample; sample++) {
    labels += ",\"" + getTimeFromSampleIndex(sample) + "\"";
    data_values += "," + String(sample_average_temps[sample], 1);  
  }
  
  String chartHtml = F("<div><canvas id=\"tempChart\" style=\"max-height=300px\"></canvas></div>"
    "<script>const ctx = document.getElementById('tempChart');"
    "new Chart(ctx, {"
      "type: 'line',"
      "data: {"
       "labels: [");
  chartHtml += labels;
  chartHtml += F(
       "],"
       "datasets: [{"
         "label: 'temperature (C)',"
         "data: [");
  chartHtml += data_values;
  chartHtml += F(
         "],"
         "borderWidth: 1"
       "}]"
      "},"
     "options: {"
     " scales: {"
      " y: { "
         " beginAtZero: true, "
         " max: 40"
       " } "
     " } "
   " } "
  " }); "
  "</script>");
  return chartHtml;
}

// ----------------------------------------------------------------------
//  Web Server

const char INDEX_HEADER[] PROGMEM = 
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>Beer Brew Monitor</title>"
"<style>"
"body { font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }"
"</style>"
"</head>"
"<body><h1>Beer Brew Monitor</h1>"
"<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>";  // May move this locally later

const char INDEX_FOOTER[] PROGMEM = 
"</body>"
"</html>";

String prepareSetPointForm() {
  String formContent = F("<form action=\"/\" method=\"post\">"
    "<label for=\"minset\">Heater on below:</label>"
    "<input type=\"text\" id=\"minset\" name=\"minset\" value=\"");
  formContent += String(relay_on_below_temp,1);
  formContent += F("\" size=\"4\"><p>"
    "<label for=\"maxset\">Heater off above:</label>"
    "<input type=\"text\" id=\"maxset\" name=\"maxset\" value=\"");
  formContent += String(relay_off_above_temp, 1);
  formContent += F("\" size=\"4\"> "
    "<input type=\"submit\" value=\"Set\"></form>");
  return formContent;
}

String prepareControlButtonsForm() {
  String formContent = F("<form action=\"/\" method=\"post\">"
  "<input type=\"submit\" name=\"resetminmax\" value=\"Reset min and max\"> "
  "<input type=\"submit\" name=\"resetall\" value=\"Reset All Values\"> "
  "<input type=\"submit\" name=\"resetwifi\" value=\"Reset Wifi\"> "
  "</form>");
  return formContent;
}

String getUpTime() {
  unsigned long uptime_total_seconds = timeClient.getEpochTime() - startup_time;
  int days = uptime_total_seconds / 86400;
  uptime_total_seconds -= days * 86400;
  int hours = uptime_total_seconds / 3600;
  uptime_total_seconds -= hours * 3600;
  int minutes = uptime_total_seconds / 60;
  uptime_total_seconds -= minutes * 60;

  char tempBuf[20];
  sprintf_P(tempBuf, PSTR("%d days %02d:%02d:%02d"), days, hours, minutes, uptime_total_seconds);
  return String(tempBuf);
}

void handleRoot() {
  if(server.hasArg("minset")) {
    handleSetPoints();
    return redirectBackToRoot();
  }

  if(server.hasArg("resetminmax")) {
    resetMinMaxTemps(true);
    return redirectBackToRoot();
  }

  if(server.hasArg("resetall")) {
    handleResetAll();
    return redirectBackToRoot();
  }

  if(server.hasArg("resetwifi")) {
    wifiManager.erase();
    ESP.restart();
  }
  
  String response = FPSTR(INDEX_HEADER);
  response += "Time: " + timeClient.getFormattedTime() + "<p>";
  response += "Uptime: " + getUpTime() + "<p>";
  response += getTempReport() + "<p>";
  
  int startAt = current_sample + 1;
  if(startAt >= NUM_SAMPLES) startAt = 0;
  response += getChart(startAt);
  response += "<p>";
  response += prepareSetPointForm() + "<p>";
  response += prepareControlButtonsForm();
  response += FPSTR(INDEX_FOOTER);
  server.send(200, "text/html", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void redirectBackToRoot() {
  // An attempt to clear form data, so that page refreshes don't re-submit
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleResetAll() {
  initSampleRecords();
  writeDataToFS();
}

void handleSetPoints() {
  String minsetvalue = server.arg("minset");
  String maxsetvalue = server.arg("maxset");
  float fminsetvalue = minsetvalue.toFloat();
  float fmaxsetvalue = maxsetvalue.toFloat();

  if(minsetvalue < maxsetvalue) {
    relay_on_below_temp = fminsetvalue;
    relay_off_above_temp = fmaxsetvalue;

    // Store the new values (along with everything else)
    writeDataToFS();  // Probably should be moved to config, but was in the first file
  }
}

void handleJson() {
  String jsonData = "{\n";
  jsonData += "\"current\": \"" + String(current_temp, 1) + "\",\n";
  jsonData += "\"minimum\": \"" + String(min_temp, 1) + "\",\n";
  jsonData += "\"maximum\": \"" + String(max_temp, 1) + "\",\n";  
  jsonData += "\"samples\": [";
  for(int sample = 0; sample < NUM_SAMPLES; sample++)
  {
    if(sample > 0)
      jsonData += ",";
    jsonData += "\"" + String(sample_average_temps[sample], 1) + "\"";
  }
  jsonData += "]\n";
  jsonData += "}";
  
  server.send(200, "application/json", jsonData);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
