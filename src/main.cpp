#include <Arduino.h>
#include "app.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  appBegin();
}

void loop() {
  appUpdate();
}
