#include "CsvHelpers.h"

// ----------------------------------------------------------------------
// File reading and writing helper functions

void CsvHelpers::writeFloat(File file, float value) {
  file.print(String(value, 1));
  file.print(",");
}

void CsvHelpers::writeInt(File file, int value) {
  file.print(String(value));
  file.print(",");
}

void CsvHelpers::writeString(File file, String value) {
  file.print(value);  // Perhaps should replace "," with something else.
  file.print(",");
}
