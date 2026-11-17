// Host checks for the actual firmware atmosphere, independent of app/audio I/O.
// Build and run with: bash scripts/test_native.sh
#include "atmosphere.h"
#include "display.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
#include <vector>

namespace {
struct GuardedFrame {
  uint32_t before = 0x13579bdf;
  uint16_t pixels[LCD_W * LCD_H] = {};
  uint32_t after = 0x2468ace0;
};
GuardedFrame frame;
Arduino_GFX graphics;
bool useFrame = true;
uint16_t snapshot[LCD_W * LCD_H];

int update(int steps, float strength, bool enabled = true, bool allow = true) {
  int events = 0;
  for (int i = 0; i < steps; i++) {
    atmosphereUpdate(.01f, strength, enabled, allow);
    events += atmosphereTakeBlow() ? 1 : 0;
  }
  return events;
}
int untilBlow() {
  for (int i = 1; i <= 200; i++)
    if (update(1, 1)) return i;
  assert(false && "Sustained blowing should cross the threshold");
  return 0;
}
void capture() {
  for (int y = 0; y < LCD_H; y++)
    for (int x = 0; x < LCD_W; x++) snapshot[y * LCD_W + x] = atmospherePixel(x, y);
}
int differences() {
  int count = 0;
  for (int y = 0; y < LCD_H; y++)
    for (int x = 0; x < LCD_W; x++) count += snapshot[y * LCD_W + x] != atmospherePixel(x, y);
  return count;
}
double luminance() {
  double total = 0;
  for (int y = 0; y < LCD_H; y += 8) for (int x = 0; x < LCD_W; x += 8) {
    const uint16_t c = atmospherePixel(x, y);
    total += .2126 * ((c >> 11) & 31) / 31 +
             .7152 * ((c >> 5) & 63) / 63 + .0722 * (c & 31) / 31;
  }
  return total / ((LCD_W / 8) * (LCD_H / 8));
}

void drawingAndThemes() {
  atmosphereBegin();
  assert(atmosphereTheme() == ATM_COLOUR);
  assert(atmosphereLevel() == 0 && atmosphereBlowForce() == 0);
  assert(atmosphereMotionScale() == 1);
  atmosphereDraw();
  assert(frame.before == 0x13579bdf && frame.after == 0x2468ace0);
  for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++)
    assert(frame.pixels[y * LCD_W + x] == atmospherePixel(x, y));
  assert(atmospherePixel(-100, -100) == atmospherePixel(0, 0));
  assert(atmospherePixel(999, 999) == atmospherePixel(LCD_W - 1, LCD_H - 1));

  for (int i = ATM_NIGHT; i <= ATM_COLOUR; i++) {
    atmosphereSetTheme((AtmosphereTheme)i);
    const uint16_t ink = i == ATM_NIGHT ? 0xffff : 0;
    for (int y = 0; y < LCD_H; y += 17) for (int x = 0; x < LCD_W; x += 19) {
      assert(atmosphereInk(0, x, y) == atmospherePixel(x, y));
      assert(atmosphereInk(1, x, y) == ink);
      assert(atmosphereInk(-1, x, y) == atmospherePixel(x, y));
      assert(atmosphereInk(2, x, y) == ink);
    }
  }
  atmosphereSetTheme(ATM_NIGHT);
  const double night = luminance();
  atmosphereSetTheme(ATM_PAPER);
  assert(luminance() > night + .4);
  atmosphereSetTheme((AtmosphereTheme)255);
  assert(atmosphereTheme() == ATM_PAPER);
  useFrame = false;
  atmosphereDraw();  // Exercise the no-framebuffer 4x4-tile path.
  useFrame = true;
}

void smoothMovement() {
  atmosphereBegin();
  capture();
  atmosphereUpdate(0, 1, true);
  assert(differences() == 0);  // Changing input never jumps an accumulated phase.
  update(300, 0, false);
  assert(differences() > LCD_W * LCD_H / 10);
  capture();
  atmosphereSetTheme(ATM_NIGHT);
  atmosphereSetTheme(ATM_PAPER);
  atmosphereSetTheme(ATM_COLOUR);
  assert(differences() == 0);  // Theme selection retains the current motion/colour phase.

  atmosphereBegin();
  capture();
  atmosphereUpdate(.01f, 1, true);
  const float first = atmosphereLevel();
  assert(fabsf(first - (1 - expf(-.01f / .018f))) < .0001f);
  assert(atmosphereMotionScale() > 6 && atmosphereMotionScale() < 7);
  update(100, 1);
  assert(atmosphereLevel() > .99f && atmosphereBlowForce() > .99f);
  assert(atmosphereMotionScale() > 8.99f && atmosphereMotionScale() <= 9);
  assert(differences() > LCD_W * LCD_H / 5);
  atmosphereUpdate(.01f, 0, false);
  assert(atmosphereLevel() > .9f);  // Release eases rather than snapping the blobs back.
  update(500, 0, false);
  assert(atmosphereMotionScale() < 1.01f && atmosphereBlowForce() < .001f);
  atmosphereUpdate(NAN, NAN, true);
  atmosphereUpdate(INFINITY, INFINITY, true);
  assert(isfinite(atmosphereLevel()) && isfinite(atmosphereMotionScale()));
}

void reactiveGridCadence() {
  atmosphereBegin(); capture();
  atmosphereUpdate(.09f, 0, false);
  assert(differences() == 0); // Quiet drift refreshes at 10Hz.
  atmosphereUpdate(.01f, 0, false);
  assert(differences() > 0);
  atmosphereBegin(); capture();
  atmosphereUpdate(.04f, .05f, true);
  assert(differences() == 0);
  atmosphereUpdate(.01f, .05f, true);
  assert(differences() > 0); // Sound reaches the cached CPU field within 50ms.
  capture();
  atmosphereUpdate(.05f, 0, false);
  assert(differences() > 0); // Releasing the mic retains 20Hz while motion eases.
}

void colourwayVariation() {
  std::vector<uint16_t> colourways[ATM_COLOURWAY_COUNT];
  for (int way = 0; way < ATM_COLOURWAY_COUNT; way++) {
    atmosphereBegin();
    atmosphereSetColourway(way);
    assert(atmosphereTheme() == ATM_COLOUR && atmosphereColourway() == way);
    colourways[way].resize(LCD_W * LCD_H);
    for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++)
      colourways[way][y * LCD_W + x] = atmospherePixel(x, y);
    // Repeating a boot + chosen offset reproduces the same complete image.
    atmosphereBegin();
    atmosphereSetColourway(way);
    for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++)
      assert(colourways[way][y * LCD_W + x] == atmospherePixel(x, y));
  }
  for (int first = 0; first < ATM_COLOURWAY_COUNT; first++) {
    for (int second = first + 1; second < ATM_COLOURWAY_COUNT; second++) {
      int changed = 0;
      for (int i = 0; i < LCD_W * LCD_H; i++)
        changed += colourways[first][i] != colourways[second][i];
      assert(changed > LCD_W * LCD_H / 4);
    }
  }
  for (int invalid : {-1, (int)ATM_COLOURWAY_COUNT, 255, INT_MAX, INT_MIN}) {
    atmosphereBegin();
    atmosphereSetColourway(2);
    atmosphereSetColourway(invalid);
    assert(atmosphereColourway() == 0);
    for (int y = 0; y < LCD_H; y++) for (int x = 0; x < LCD_W; x++)
      assert(colourways[0][y * LCD_W + x] == atmospherePixel(x, y));
  }
  // A colourway is an additive palette offset, not a reset or speed change.
  atmosphereBegin();
  update(300, 0, false);
  capture();
  atmosphereSetColourway(1);
  atmosphereSetColourway(2);
  atmosphereSetColourway(0);
  assert(differences() == 0);
  for (int monochrome = ATM_NIGHT; monochrome <= ATM_PAPER; monochrome++) {
    atmosphereSetTheme((AtmosphereTheme)monochrome);
    capture();
    atmosphereSetColourway(1);
    atmosphereSetColourway(2);
    assert(differences() == 0);
  }
}

void oneEventPerBreath() {
  atmosphereBegin();
  const int frames = untilBlow();
  assert(frames >= 6 && frames <= 15);
  assert(!atmosphereTakeBlow());
  assert(update(1000, 1) == 0);  // A held blow never skips ahead again.
  assert(update(10, 0) == 0);
  assert(update(200, 1) == 0);   // A short release cannot rearm.
  assert(update(300, 0) == 0);
  assert(update(100, 1) == 1);
  assert(update(300, 1) == 0);

  atmosphereBegin();
  untilBlow();
  assert(update(145, 0) == 0);   // Enough quiet, but the 1.6s cooldown has not elapsed.
  assert(update(100, 1) == 0);
  assert(update(300, 0) == 0);
  assert(update(100, 1) == 1);
}

void menuAndThreshold() {
  atmosphereBegin();
  assert(update(300, 1, true, false) == 0);
  assert(atmosphereLevel() > .99f);  // The menu suppresses advancement, not pressure.
  assert(update(5, 1) == 0);
  assert(update(1, 1) == 1);
  atmosphereUpdate(.01f, 1, false);
  assert(!atmosphereTakeBlow());
  assert(update(300, 1, false) == 0);

  atmosphereSetThreshold(10);
  assert(atmosphereThreshold() == .95f);
  atmosphereSetThreshold(-10);
  assert(atmosphereThreshold() == .05f);
  atmosphereSetThreshold(NAN);
  assert(atmosphereThreshold() == .10f);
  atmosphereBegin();
  assert(update(4, 1) == 0);
  atmosphereSetThreshold(.9f);
  assert(update(5, 1) == 0);
  assert(update(1, 1) == 1);
}

void gentleBreathsAndSpikes() {
  atmosphereBegin();
  assert(atmosphereThreshold() == .10f);
  assert(update(200, .05f) == 0);
  assert(fabsf(atmosphereLevel() - .05f) < .0001f); // the meter remains linear
  assert(fabsf(atmosphereBlowForce() - sqrtf(.05f)) < .0001f);
  assert(atmosphereMotionScale() > 2.98f && atmosphereMotionScale() < 3);

  for (float gentle : {.135f, .2f}) {
    atmosphereBegin();
    int elapsed = 0;
    for (; elapsed < 15; elapsed++) if (update(1, gentle)) break;
    assert(elapsed >= 5 && elapsed < 15); // light breath responds in under .15s
    assert(update(300, gentle) == 0); // sustained gentle breath still triggers once
    assert(update(20, 0) == 0);
    assert(update(100, gentle) == 0); // quiet period is shorter than the .5s rearm
    assert(update(200, 0) == 0);
    assert(update(15, gentle) == 1);
  }

  // A full-scale click/handling spike never finishes from a smoothed tail.
  for (int spikeFrames : {1, 5}) {
    atmosphereBegin();
    assert(update(spikeFrames, 1) == 0);
    assert(update(300, 0) == 0);
  }
  atmosphereBegin();
  atmosphereUpdate(.05f, 1, true);
  assert(!atmosphereTakeBlow());
  atmosphereUpdate(.05f, 0, true);
  assert(!atmosphereTakeBlow());

  atmosphereBegin();
  atmosphereSetThreshold(.05f);
  assert(update(200, .04f) == 0);
  assert(update(15, .075f) == 1); // the menu can tune navigation to a lighter breath
}
}  // namespace

Arduino_GFX *gfx = &graphics;
uint16_t *displayFramebuffer() { return useFrame ? frame.pixels : nullptr; }

int main() {
  drawingAndThemes();
  smoothMovement();
  reactiveGridCadence();
  colourwayVariation();
  oneEventPerBreath();
  menuAndThreshold();
  gentleBreathsAndSpikes();
  puts("Native atmosphere checks passed (CPU background, colourways, themes, pressure, breath gate)");
}
