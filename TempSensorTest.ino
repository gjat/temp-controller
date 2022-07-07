// From: https://simple-circuit.com/arduino-ds1621-digital-temperature-sensor/
#include <Wire.h>

// A2, A1, A0 connected to GND = 1001000 = 0x48
#define DS1621_ADDRESS 0x48

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000);
  Wire.beginTransmission(DS1621_ADDRESS);
  Wire.write(0xAC);                       // send configuration register address (Access Config)
  Wire.write(0);                          // perform continuous conversion
  Wire.beginTransmission(DS1621_ADDRESS); // send repeated start condition
  Wire.write(0xEE);                       // send start temperature conversion command
  Wire.endTransmission();                 // stop transmission and release the I2C 
}

// variables
char c_buffer[8];

void loop() {
  
  delay(1000);    // wait a second
  
  float temp = get_temperature();
  Serial.print(temp);
  Serial.println(" deg C");
}
 
float get_temperature() {
  float temp = 0;
  Wire.beginTransmission(DS1621_ADDRESS); // connect to DS1621 (send DS1621 address)
  Wire.write(0xAA);                       // read temperature command
  Wire.endTransmission(false);            // send repeated start condition
  Wire.requestFrom(DS1621_ADDRESS, 2);    // request 2 bytes from DS1621 and release I2C bus at end of reading
  uint8_t t_msb = Wire.read();            // read temperature MSB register
  uint8_t t_lsb = Wire.read();            // read temperature LSB register
 
  temp = t_msb;
  if(t_lsb)
    temp += 0.5;
  return temp;
}
