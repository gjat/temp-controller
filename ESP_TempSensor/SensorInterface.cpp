#include "SensorInterface.h"
#include <Wire.h>
#include <time.h>

void SensorInterface::Setup()
{
  Wire.begin();
  Wire.setClock(400000);

  Reset();
}

void SensorInterface::Reset()
{
  // Assumes Initialise() has been called.
  Wire.beginTransmission(DS1621_ADDRESS_1);
  Wire.write(0xAC);                         // send configuration register address (Access Config)
  Wire.write(0);                            // perform continuous conversion
  Wire.beginTransmission(DS1621_ADDRESS_1); // send repeated start condition
  Wire.write(0xEE);                         // send start temperature conversion command
  Wire.endTransmission();                   // stop transmission and release the I2C 
}

void SensorInterface::RecordTemperature(SampleBuffer& samples)
{
  float now_temp = ReadSensor();
  if(now_temp < MIN_EXPECTED_TEMP || now_temp > MAX_EXPECTED_TEMP)
  {
    Serial.println("Temp outside range.  Ignoring it.");
    // Try and re-initialise the temp sensor.  It could have been disconnected and power-cycled,
    //  so it needs to be configured to start conversions again.
    Reset();
  } else {
    samples.SetSample(now_temp);
  }
}

float SensorInterface::ReadSensor() {
  float temp = 0;
  Wire.beginTransmission(DS1621_ADDRESS_1); // connect to DS1621 (send DS1621 address)
  Wire.write(0xAA);                       // read temperature command
  Wire.endTransmission(false);            // send repeated start condition
  Wire.requestFrom(DS1621_ADDRESS_1, static_cast<size_t>(2));  // request 2 bytes from DS1621 and release I2C bus at end of reading
  uint8_t t_msb = Wire.read();            // read temperature MSB register
  uint8_t t_lsb = Wire.read();            // read temperature LSB register
 
  temp = t_msb;
  if(t_lsb)
    temp += 0.5;
  return temp;
}
