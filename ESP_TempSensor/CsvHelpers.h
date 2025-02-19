#include <Arduino.h>
#include <LittleFS.h>

// ----------------------------------------------------------------------
// File reading and writing helper functions

class CsvHelpers {
public:
  static void writeFloat(File file, float value);
  static void writeInt(File file, int value);
  static void writeString(File file, String value);
};
