#pragma once

#include <Arduino.h>

void signEnter();
void signTouch(bool down, int16_t x, int16_t y);
bool signIdleDone();
void signDraw();

void summaryEnter();
void summaryDraw();
