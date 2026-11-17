#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define OUTPUT 1
#define HIGH 1
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
struct TestSerial {
  void println(const char *) {}
};
extern TestSerial Serial;
