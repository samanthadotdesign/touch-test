#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define pgm_read_byte(p) (*(const uint8_t *)(p))
uint32_t millis();
long random(long upper);
void randomSeed(unsigned long seed);
struct TestSerial {
  void println(const char *) {}
  template<typename... Args> void printf(const char *, Args...) {}
};
extern TestSerial Serial;
