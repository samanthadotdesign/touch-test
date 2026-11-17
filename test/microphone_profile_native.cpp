// Run the production app, atmosphere and letter engine with a simulated mic/NVS.
#include "app.h"
#include "atmosphere.h"
#include "display.h"
#include "letters.h"
#include <Preferences.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

TestSerial Serial;
Arduino_GFX graphics;
Arduino_GFX *gfx = &graphics;
uint32_t nowMs = 0;
bool fingerDown = false, microphoneEnabled = false;
int16_t fingerX = 120, fingerY = 160;
float normalizedInput = 0;
uint32_t millis() { return nowMs; }
long random(long upper) { return rand() % upper; }
void randomSeed(unsigned long seed) { srand(seed); }
void displayBegin() {}
void displayFlush() {}
uint16_t *displayFramebuffer() { graphics.glyphWrites.clear(); return nullptr; }
void displayCenter(const char *, int16_t, uint8_t, uint16_t, uint16_t) {}
void touchBegin() {}
bool touchRead(int16_t &x, int16_t &y) { x = fingerX; y = fingerY; return fingerDown; }
bool audioInputBegin() { microphoneEnabled = false; return true; }
bool audioInputSetEnabled(bool enabled) { microphoneEnabled = enabled; return true; }
bool audioInputAvailable() { return true; }
bool audioInputEnabled() { return microphoneEnabled; }
float audioInputLevel() { return microphoneEnabled ? normalizedInput : 0; }
void audioInputUpdate() {}

namespace {
void advance(int milliseconds) {
  for (int i = 0; i < milliseconds; i += 10) { nowMs += 10; appUpdate(); }
}
void tapAt(int16_t x, int16_t y) {
  fingerX = x; fingerY = y;
  fingerDown = true; advance(10);
  fingerDown = false; advance(10);
}
void boot() {
  fingerDown = false; normalizedInput = 0;
  appBegin();
  assert(!microphoneEnabled && appState() == ST_MOVING);
}
void close(float actual, float expected) { assert(fabsf(actual - expected) < .0001f); }

void migrateProfiles() {
  Preferences stored;
  assert(stored.begin("poetry", false));
  Preferences::clearAll();
  boot();
  close(atmosphereThreshold(), .25f);
  assert(stored.getUChar("threshold", 0) == 25 && stored.getUChar("micProfile", 0) == 1);
  assert(Preferences::writesTo("poetry", "threshold") == 1);
  assert(Preferences::writesTo("poetry", "micProfile") == 1);

  for (uint8_t legacy : {75, 25, 20, 5, 0, 255}) {
    Preferences::clearAll();
    stored.putUChar("threshold", legacy);
    boot();
    const uint8_t expected = legacy >= 5 && legacy < 25 ? legacy : 25;
    close(atmosphereThreshold(), expected / 100.f);
    assert(stored.getUChar("threshold", 0) == expected && stored.getUChar("micProfile", 0) == 1);
  }

  // The one-time update must not overwrite later personal calibration.
  stored.putUChar("threshold", 65);
  const size_t thresholdWrites = Preferences::writesTo("poetry", "threshold");
  boot(); close(atmosphereThreshold(), .65f);
  assert(Preferences::writesTo("poetry", "threshold") == thresholdWrites);
  stored.putUChar("micProfile", 2); stored.putUChar("threshold", 45);
  boot(); close(atmosphereThreshold(), .45f);
  assert(stored.getUChar("micProfile", 0) == 2); // an older firmware never downgrades a newer profile
  stored.putUChar("threshold", 255);
  boot(); close(atmosphereThreshold(), .25f);

  // Never mark migration complete until its threshold is saved successfully.
  Preferences::clearAll(); stored.putUChar("threshold", 75);
  Preferences::failNextWrites("poetry", "threshold");
  boot(); close(atmosphereThreshold(), .25f);
  assert(stored.getUChar("threshold", 0) == 75 && stored.getUChar("micProfile", 0) == 0);
  assert(Preferences::writesTo("poetry", "micProfile") == 0);
  boot();
  assert(stored.getUChar("threshold", 0) == 25 && stored.getUChar("micProfile", 0) == 1);
  Preferences::clearAll(); stored.putUChar("threshold", 75);
  Preferences::failNextWrites("poetry", "micProfile");
  boot();
  assert(stored.getUChar("threshold", 0) == 25 && stored.getUChar("micProfile", 0) == 0);
  boot();
  assert(stored.getUChar("micProfile", 0) == 1);
}

void menuCalibrationPersists() {
  Preferences::clearAll(); boot(); advance(300);
  tapAt(220, 300); assert(appMenuVisible());
  for (int i = 0; i < 5; i++) tapAt(170, 273);
  close(atmosphereThreshold(), .05f); // minus control reaches its new 5% lower limit
  tapAt(210, 273); close(atmosphereThreshold(), .1f);
  tapAt(220, 100); assert(!appMenuVisible());
  boot(); close(atmosphereThreshold(), .1f);
}

void gentleBlowUsesTheTapSequence() {
  Preferences::clearAll(); boot(); advance(300);
  tapAt(220, 300); tapAt(60, 245); tapAt(220, 100);
  assert(microphoneEnabled && !appMenuVisible());
  normalizedInput = .1f; advance(1000);
  assert(appState() == ST_MOVING); // movement response below the navigation threshold
  normalizedInput = .306f; // the hardware driver's normalized .02 RMS breath
  const uint32_t firstBlowAt = nowMs;
  while (appState() == ST_MOVING && nowMs - firstBlowAt < 300) advance(10);
  assert(appState() == ST_ASSEMBLING && nowMs - firstBlowAt < 300);
  advance(600); assert(appState() == ST_READING);
  advance(3000); assert(appState() == ST_READING); // a held breath is one action
  normalizedInput = 0; advance(2200);
  normalizedInput = .306f;
  const uint32_t secondBlowAt = nowMs;
  while (appState() == ST_READING && nowMs - secondBlowAt < 300) advance(10);
  assert(appState() == ST_DISSOLVING && nowMs - secondBlowAt < 300);
  assert(lettersInfo().nextPoemId != nullptr);
  advance(700); assert(appState() == ST_MOVING);
  advance(2000); assert(appState() == ST_MOVING);
}
} // namespace

int main() {
  migrateProfiles();
  menuCalibrationPersists();
  gentleBlowUsesTheTapSequence();
  puts("Native microphone profile checks passed (gentle breath, tap sequence, persistent calibration, migration retries)");
}
