#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
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

// Because I'm in NZ, this is UTC+12 hours.  Change for your timezone
//  one day, I'll make this a config option.
const long UTC_OFFSET_SECONDS = 12 * 60 * 60;

#define DS1621_ADDRESS_1 0x48  // All address pins connect to ground
#define RELAY_OUTPUT 16        // (4th from top on left, on a "bare board").   
#define SERIAL_PORT_BPS 115200

// NTP update time.  Doesn't need to be often.
const long NTP_UPDATE_INTERVAL_MS = 600000; // 10 minutes in ms

// The file used to store previous data over a reboot
#define DATA_FILE "/avgs.csv"

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
WiFiManager wifiManager;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS, NTP_UPDATE_INTERVAL_MS);

void handleRoot();              // function prototypes for HTTP handlers
void handleNotFound();

// Immediate values, min and max.
float min_temp = 50.0;
float max_temp = 0.0;
float current_temp = 0.0;

// Temps to turn the relay on and off at.
float relay_on_below_temp = 21.5;
float relay_off_above_temp = 23.0;

// Hourly values
int hourly_average_counts[24];
float hourly_average_temps[24];

// ----------------------------------------------------------------------

void setup()
{
  initHourlyValues();
  setupSerial();
  wifiManager.setConfigPortalTimeout(300);  // 5 minutes for configuration
  wifiManager.autoConnect(WIFI_CONFIG_NAME);
  setupI2C();
  setupRelay();
  setupWebServer();
  timeClient.begin();
  if(!MDNS.begin(MDNS_NAME)) {
    Serial.println("Failed to setup MDNS responder!");
  }
  
  if(!LittleFS.begin()) {
    Serial.println("Failed to start filesystem!");
  }
}

void initHourlyValues() {
  int i;
  for(i=0; i<24; i++) {
    hourly_average_counts[i] = 0;
    hourly_average_temps[i] = 0;
  }
}

void resetMinMaxTemps() {
  min_temp = 50.0;
  max_temp = 0.0;
}

void setupSerial() {
  Serial.begin(SERIAL_PORT_BPS);
  Serial.println();
}

void setupWebServer() {
  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
  server.onNotFound(handleNotFound);        // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  server.begin();                           // Actually start the server
}

void setupI2C() {
  Wire.begin();
  Wire.setClock(400000);
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

int current_hour = 0;
bool inStartup = true;

const int LOOP_DELAY = 1000;
int loopCount = LOOP_DELAY;

void loop() {
  server.handleClient();                    // Listen for HTTP requests from clients
  
  delay(10);    
  
  loopCount++;
  if(loopCount > LOOP_DELAY)
  {
    timeClient.update();

    // Read any values stored in the filesystem, if this is the first time around.
    if(inStartup) {
      readFromFS();
      inStartup = false;
    }
    readTemperature();

    if(current_temp < relay_on_below_temp) {
      digitalWrite(RELAY_OUTPUT, HIGH);
    }

    if(current_temp > relay_off_above_temp) {
      digitalWrite(RELAY_OUTPUT, LOW);
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

void writeToFS() {

  File file = LittleFS.open(DATA_FILE, "w");
  
  //Write to the file
  file.print(String(current_temp,1));  file.print(",");
  file.print(String(min_temp, 1)); file.print(",");
  file.print(String(max_temp, 1)); file.print(",");
  
  int i = 0;
  for(i = 0; i < 24; i++) {
    writeFloat(file, hourly_average_temps[i]);
    file.print(hourly_average_counts[i]);
    file.print(",");
  }
  
  delay(1);

  //Close the file
  file.close(); 
}

void readFromFS() {
  
  if(LittleFS.exists(DATA_FILE)) {
    File file = LittleFS.open(DATA_FILE, "r");

    // Read from file
    current_temp = file.parseFloat();
    min_temp = file.parseFloat();
    max_temp = file.parseFloat();
    int i = 0;
    for(i = 0; i < 24; i++) {
      hourly_average_temps[i] = file.parseFloat();
      hourly_average_counts[i] = file.parseInt();
    }
    file.close();
  }
}

// ----------------------------------------------------------------------
//  Read from the sensor

void readTemperature() {

  current_temp = get_temperature();

  if (current_temp < min_temp) min_temp = current_temp;
  if (current_temp > max_temp) max_temp = current_temp;

  int nowHour = timeClient.getHours();
  if(nowHour != current_hour)
  {
    current_hour = nowHour;
    hourly_average_temps[nowHour] = current_temp;
    hourly_average_counts[nowHour] = 1;
    writeToFS();
  }
  else
  {
    hourly_average_temps[nowHour] = ((hourly_average_temps[nowHour] * hourly_average_counts[nowHour]) + current_temp) / (hourly_average_counts[nowHour] + 1);
    hourly_average_counts[nowHour]++;
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

// ----------------------------------------------------------------------
//  Make "reports"

String getLineBreak(bool inHtml) {
  if(inHtml)
    return "<br>";
  else
    return "\n";
}

String getTempReport() {
  String report = "Now = ";
  report += String(current_temp, 1);
  report += " deg C, Min = ";
  report += String(min_temp, 1);
  report += " deg C, Max = ";
  report += String(max_temp, 1);
  report += " deg C";
  return report;
}

String getAveragesReport(int startAtHour, bool inHtml) {
  String report = "Hour    Average (deg C)";
  report += getLineBreak(inHtml);
  report += "-----------------------" + getLineBreak(inHtml);
  
  int hour;
  for(hour = startAtHour; hour < 24; hour++) {
    report += String(hour) + ":00     " + 
      String(hourly_average_temps[hour], 1) + 
      getLineBreak(inHtml); 
  }
  
  for(hour = 0; hour < startAtHour; hour++) {
    report += String(hour) + ":00     " + 
      String(hourly_average_temps[hour], 1) + 
      getLineBreak(inHtml); 
  }

  return report;
}

// ----------------------------------------------------------------------
//  Web Server

const char INDEX_HEADER[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>Beer Brew Monitor</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body><h1>Beer Brew Monitor</h1>";

const char INDEX_FOOTER[] = 
"</body>"
"</html>";

String prepareSetPointForm() {
  String formContent = "<form action=\"/\" method=\"post\">"
    "<label for=\"minset\">Heater on below:</label>"
    "<input type=\"text\" id=\"minset\" name=\"minset\" value=\"";
  formContent += String(relay_on_below_temp,1);
  formContent += "\"><p>"
    "<label for=\"maxset\">Heater off above:</label>"
    "<input type=\"text\" id=\"maxset\" name=\"maxset\" value=\"";
  formContent += String(relay_off_above_temp, 1);
  formContent += "\"><p>"
    "<input type=\"submit\" value=\"Set\"></form>";
  return formContent;
}

String prepareResetTempForm() {
  String formContent = "<form action=\"/\" method=\"post\">"
  "<input type=\"submit\" name=\"resetminmax\" value=\"Reset min and max\">"
  "</form>";
  return formContent;
}

void handleRoot() {
  if(server.hasArg("minset"))
    handleSetPoints();

  if(server.hasArg("resetminmax"))
    resetMinMaxTemps();
  
  String response = INDEX_HEADER;
  response += "<code>";
  response += "Time: " + timeClient.getFormattedTime() + "<p>";
  response += getTempReport() + "<p>";
  
  int startAt = current_hour + 1;
  if(startAt > 23) startAt = 0;
  response += getAveragesReport(current_hour + 1, true);
  response += "</code><p>";
  response += prepareSetPointForm() + "<p>";
  response += prepareResetTempForm();
  response += INDEX_FOOTER;
  server.send(200, "text/html", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleSetPoints() {
  String minsetvalue = server.arg("minset");
  String maxsetvalue = server.arg("maxset");
  float fminsetvalue = minsetvalue.toFloat();
  float fmaxsetvalue = maxsetvalue.toFloat();

  if(minsetvalue < maxsetvalue) {
    relay_on_below_temp = fminsetvalue;
    relay_off_above_temp = fmaxsetvalue;
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
