#pragma once

#include <Arduino_GFX_Library.h>
#include "config.h"

extern Arduino_GFX *gfx;

void displayBegin();
void displayFlush();
bool displayUsingCanvas();

void displayCenter(const char *s, int16_t y, uint8_t size, uint16_t fg, uint16_t bg);
