#include "audio.h"
#include <Arduino.h>

bool audioBegin() {
  Serial.println("audio: stub (rings and music skipped)");
  return true;
}

void playRing(int n) { (void)n; }
void musicStart() {}
void musicPause() {}
void musicResume() {}
void musicStop() {}
