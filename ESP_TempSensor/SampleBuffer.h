#include <Arduino.h>

#ifndef _SENSOR_SAMPLES_
#define _SENSOR_SAMPLES_

// Samples per day determining minutes per sample.
//  Because this is a slow moving temperature, 30 minute recorded "samples"
//  should be enough.  
const int SAMPLES_PER_DAY = 48;
const int MINUTES_PER_DAY = 24 * 60;
const int MINUTES_PER_SAMPLE = MINUTES_PER_DAY / SAMPLES_PER_DAY;

// Used to start the max/minimum recording.
const float MIN_EXPECTED_TEMP = 0.0;
const float MAX_EXPECTED_TEMP = 100;

// Sample storage
const int NUM_SAMPLES = SAMPLES_PER_DAY;

class SampleBuffer
{
public:
    SampleBuffer() { ClearAll(); }

    // The sample index changes every MINUTES_PER_SAMPLE (30 minutes).  When that happens
    //  the samples are written to the filesystem, and this callback fires.
    void OnSampleIndexChange(std::function<void()> sampleIndexChanged) { onSampleIndexChanged = sampleIndexChanged; }
  
private: 
    int current_index = 0;
    std::function<void()> onSampleIndexChanged;

public:
    float min_temp = MAX_EXPECTED_TEMP;
    float max_temp = MIN_EXPECTED_TEMP;
    float current_temp = MIN_EXPECTED_TEMP;

    int sample_average_counts[NUM_SAMPLES];
    float sample_average_temps[NUM_SAMPLES];

public:
    void ClearAll();         // Not persisted
    void ResetMinMaxTemps(); // Not persisted

    // Record a new sensor reading.   If the sample index has moved onto the next "sample period"
    //  then the current array of values is written to the filesystem, and the 
    //  OnSampleIndexChanged callback is called.
    void SetSample(float value);  

    // Reload sample from the filesystem
    void ReadFromFS();

    // The web server calls these, to present data
    String GetTempSummary();
    int GetCurrentSampleIndex() { return current_index; }
    
    // Called after resetting values, or clearing samples.
    void WriteToFS();
};

#endif // _SENSOR_SAMPLES_
