#pragma once

#include <Arduino.h>

void lettersLoadQuote(const char *text);
void lettersStartScatter(float speedPx);
void lettersUpdateScatter(float dt);
bool lettersScatterDone();
void lettersStartClock();
void lettersUpdateRegroup(float dt);
bool lettersRegroupSettled();
void lettersPrepareFocus();
void lettersUpdateFocus(float progress, float motionT, bool paused);
void lettersDraw(uint16_t fg);
int lettersCount();
