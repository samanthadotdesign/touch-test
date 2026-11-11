#pragma once

#include <Arduino_GFX_Library.h>
#include "config.h"

extern Arduino_GFX *gfx;

void displayBegin();
void displayFlush();
bool displayUsingCanvas();

void displayCenter(const char *s, int16_t y, uint8_t size, uint16_t fg, uint16_t bg);
void displayMMSS(uint32_t ms, char *buf, size_t n);
void displayDrawPausePlay(bool showPlay, uint16_t fg, uint16_t bg);
bool displayInPause(int16_t x, int16_t y);
