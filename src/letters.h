#pragma once
#include <Arduino.h>
#include "poetry_data.h"

bool lettersLoadPoem(const Poem &poem, const MotionPreset &motion);
bool lettersPrepareNext(const Poem &poem, const MotionPreset &motion);
void lettersAdoptNext();
void lettersSetResponse(float motionScale, float blowForce);
void lettersUpdate(float seconds);
void lettersAssemble();
void lettersDissolve();
bool lettersTransitionDone();
void lettersDraw();
int lettersCount();

// Read-only engine telemetry for diagnostics and native transition tests.
struct LettersInfo {
  const char *poemId;
  const char *nextPoemId;
  int particles, nextParticles;
  float time, nextTime;
  int resolvedLayout, nextResolvedLayout;
  int realParticles, nextRealParticles;
  uint32_t seed, nextSeed;
  float minBlend, maxBlend;
  float minScale, maxScale;
  int echoPhrases;
  bool sourceBoundsValid;
};
LettersInfo lettersInfo();
