#pragma once

#include <Arduino.h>

struct StrokePt {
  int16_t x, y;
  uint8_t start;
};

bool storageBegin();
int storageNextNumber();
bool storageSaveSession(int sessionN, int blocks, int minutes, const StrokePt *pts, int n);
