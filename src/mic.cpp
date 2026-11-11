#include "mic.h"
#include <Arduino.h>

void micBegin() {
  Serial.println("mic: stub (blow detection off, tap to scatter)");
}

bool blowDetected() { return false; }
