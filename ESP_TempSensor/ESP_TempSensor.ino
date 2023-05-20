#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <LittleFS.h>

#define DS1621_ADDRESS_1 0x48
#define RELAY_OUTPUT 16

#define WIFI_NAME "somenetworkname"
#define WIFI_PASSWORD "somenetworkpassword"

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

void handleRoot();              // function prototypes for HTTP handlers
void handleNotFound();

// Define NTP Client to get time
const long UTC_OFFSET_SECONDS = 12 * 60 * 60;
const long NTP_UPDATE_INTERVAL_MS = 600000; // 10 minutes in ms
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS, NTP_UPDATE_INTERVAL_MS);

// Hourly values
int hourly_average_counts[24];
float hourly_average_temps[24];

void setup()
{
  initHourlyValues();
  setupSerial();
  setupWifi();
  setupI2C();
  setupRelay();
  setupWebServer();
  timeClient.begin();
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

void setupSerial() {
  Serial.begin(115200);
  Serial.println();
}

void setupWifi() {  
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
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

float min_temp = 50.0;
float max_temp = 0.0;
float current_temp = 0.0;
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
    if(inStartup) {
      readFromFS();
      inStartup = false;
    }
    readTemperature();

    if(current_temp < 20.5) {
      digitalWrite(RELAY_OUTPUT, HIGH);
    }

    if(current_temp > 24) {
      digitalWrite(RELAY_OUTPUT, LOW);
    }
    loopCount = 0;
  }
}

void writeFloat(File file, float value) {
  char temp_str[6];
  dtostrf(value, 5, 1, temp_str);
  file.print(temp_str);
  file.print(",");
}

void writeToFS() {
  String filename = "/avgs.csv";
  File file = LittleFS.open(filename, "w");
  
  //Write to the file
  writeFloat(file, current_temp);
  writeFloat(file, min_temp);
  writeFloat(file, max_temp);
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
  String filename = "/avgs.csv";
  if(LittleFS.exists(filename)) {
    File file = LittleFS.open(filename, "r");

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

String getTempReport() {
  char current_temp_str[6];
  char min_temp_str[6];
  char max_temp_str[6];
  dtostrf(current_temp, 5, 1, current_temp_str);
  dtostrf(min_temp, 5, 1, min_temp_str);
  dtostrf(max_temp, 5, 1, max_temp_str);
  String report = "Now = ";
  report += current_temp_str;
  report += " deg C, Min = ";
  report += min_temp_str;
  report += " deg C, Max = ";
  report += max_temp_str;
  report += " deg C";
  return report;
}

String getAveragesReport() {
  String report = "Hour    Average (deg C)\n-----------------------\n";
  int hour = 0;
  char avg_temp_str[6];
  for(hour = 0; hour < 24; hour++) {
    dtostrf(hourly_average_temps[hour], 5, 1, avg_temp_str);
    report = report + hour + ":00     " + avg_temp_str + "\n"; 
  }
  return report;
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

void handleRoot() {
  String response;
  response = "Time: " + timeClient.getFormattedTime() + "\n";
  response = response + getTempReport() + "\n";
  response = response + getAveragesReport();
  server.send(200, "text/plain", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}
