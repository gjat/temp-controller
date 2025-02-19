#include "SampleBuffer.h"
#include "CsvHelpers.h"
#include <LittleFS.h>

const char DATA_FILE[] = "/avgs.csv";
const char DATA_FILE_BACKUP[] = "/avgs.bkp";

void SampleBuffer::ClearAll() {
    int i;
    for(i=0; i<NUM_SAMPLES; i++) {
    sample_average_counts[i] = 0;
    sample_average_temps[i] = 0;
    }
    ResetMinMaxTemps();
}

void SampleBuffer::ResetMinMaxTemps() {
    min_temp = 50.0;
    max_temp = 0.0;
}

void SampleBuffer::WriteToFS() {

  if(LittleFS.exists(DATA_FILE)) {
    LittleFS.remove(DATA_FILE_BACKUP);
    LittleFS.rename(DATA_FILE, DATA_FILE_BACKUP);
  }

  File file = LittleFS.open(DATA_FILE, "w");
  CsvHelpers::writeFloat(file, current_temp);
  CsvHelpers::writeFloat(file, min_temp);
  CsvHelpers::writeFloat(file, max_temp);
  
  int i = 0;
  for(i = 0; i < NUM_SAMPLES; i++) {
    CsvHelpers::writeFloat(file, sample_average_temps[i]);
    CsvHelpers::writeInt(file, sample_average_counts[i]);
  }

  file.flush();
  file.close(); 

}

void SampleBuffer::ReadFromFS() { 

  File file;
  
  if(LittleFS.exists(DATA_FILE)) {
    file = LittleFS.open(DATA_FILE, "r");
  } else if(LittleFS.exists(DATA_FILE_BACKUP)) {
    file = LittleFS.open(DATA_FILE_BACKUP, "r");
  } 

  if(file) {
    current_temp = file.parseFloat();  
    min_temp = file.parseFloat();
    max_temp = file.parseFloat();
    
    int i = 0;
    for(i = 0; i < NUM_SAMPLES; i++) {
      sample_average_temps[i] = file.parseFloat();
      sample_average_counts[i] = file.parseInt();
    }
    file.close();
  }
}

String SampleBuffer::GetTempSummary() {
  char tempBuf[20];
  sprintf_P(tempBuf, PSTR("Now: %0.1f C,  Min: %0.1f C,  Max: %0.1f C"), current_temp, min_temp, max_temp);
  return String(tempBuf);
}

void SampleBuffer::SetSample(float value) 
{
  current_temp = value;

  // Update min and max
  if (current_temp < min_temp) 
    min_temp = current_temp;

  if (current_temp > max_temp) 
    max_temp = current_temp;

  // Calculate which sample timeslot we're in  
  time_t timeNow = time(NULL);
  struct tm *nowTime = localtime(&timeNow);
  int nowSample = (nowTime->tm_hour * 60 + nowTime->tm_min) / MINUTES_PER_SAMPLE;
  
  // Update the sample record
  if(nowSample != current_index)
  {
    current_index = nowSample;
    sample_average_temps[current_index] = current_temp;
    sample_average_counts[current_index] = 1;
    WriteToFS();
    if(onSampleIndexChanged)
      onSampleIndexChanged();
  }
  else
  {
    sample_average_temps[current_index] = ((sample_average_temps[current_index] * sample_average_counts[current_index]) + current_temp) / (sample_average_counts[current_index] + 1);
    sample_average_counts[current_index]++;
  }
}
