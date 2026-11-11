#pragma once

#include <Arduino.h>

void quoteEnter();
void quoteOnTap();
void scatterEnter();
void scatterUpdate(float dt);
bool scatterDone();
void regroupEnter();
void regroupUpdate(float dt);
bool regroupReadyForPicker();
void quoteDraw();   // QUOTE / SCATTER / REGROUP
