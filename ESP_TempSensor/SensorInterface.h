#include <Arduino.h>
#include "SampleBuffer.h"

const byte DS1621_ADDRESS_1 = 0x48;  // All address pins on the IC connect to ground, results in this being the I2C address.

class SensorInterface
{
public:
    void Setup();
    void Reset();

    void RecordTemperature(SampleBuffer& samples);

private:
    float ReadSensor();
};
