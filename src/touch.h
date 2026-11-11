#pragma once

#include <Arduino.h>

void touchBegin();
bool touchRead(int16_t &x, int16_t &y);
void touchWaitRelease();
bool touchInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh);
