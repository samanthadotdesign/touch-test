#pragma once

#include <stdint.h>

enum AtmosphereTheme : uint8_t {
  ATM_NIGHT,
  ATM_PAPER,
  ATM_COLOUR
};
constexpr uint8_t ATM_COLOURWAY_COUNT = 3;
constexpr float ATM_DEFAULT_THRESHOLD = .10f;
constexpr float ATM_MIN_THRESHOLD = .05f;
constexpr float ATM_MAX_THRESHOLD = .95f;

void atmosphereBegin();

// rawLevel is normalized microphone strength, 0..1. Disabling navigation for
// the hidden menu keeps the sound response running without advancing a poem.
void atmosphereUpdate(float dt, float rawLevel, bool micEnabled,
                      bool allowAdvance = true);
bool atmosphereTakeBlow();
float atmosphereLevel();
float atmosphereMotionScale();
float atmosphereBlowForce();

void atmosphereSetTheme(AtmosphereTheme theme);
AtmosphereTheme atmosphereTheme();
// Offset the three authored colour palettes without resetting their clocks.
// Invalid indices select colourway zero.
void atmosphereSetColourway(int colourway);
uint8_t atmosphereColourway();
void atmosphereSetThreshold(float threshold);
float atmosphereThreshold();

// Both drawing and text blending sample the same cached background. No heap
// allocation or per-pixel transcendental math is needed for these operations.
uint16_t atmospherePixel(int x, int y);
uint16_t atmosphereInk(float opacity, int x = 120, int y = 160);
void atmosphereDraw();
