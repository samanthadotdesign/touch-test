#pragma once
#include <stddef.h>
#include <stdint.h>
#define OUTPUT 1
#define HIGH 1
#define LOW 0
uint32_t millis();
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
