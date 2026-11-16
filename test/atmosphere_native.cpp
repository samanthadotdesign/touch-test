// Host checks for the actual firmware atmosphere, independent of app/audio I/O.
// c++ -std=c++11 -O2 -Itest/native_stubs -Isrc test/atmosphere_native.cpp \
//   src/atmosphere.cpp -o /tmp/atmosphere-native && /tmp/atmosphere-native
#include "atmosphere.h"
#include "display.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

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
  assert(first > 0 && first < .2f);
  assert(atmosphereMotionScale() > 1.25f && atmosphereMotionScale() < 2);
  update(100, 1);
  assert(atmosphereLevel() > .99f && atmosphereBlowForce() > .99f);
  assert(atmosphereMotionScale() > 4.99f && atmosphereMotionScale() <= 5);
  assert(differences() > LCD_W * LCD_H / 5);
  atmosphereUpdate(.01f, 0, false);
  assert(atmosphereLevel() > .9f);  // Release eases rather than snapping the blobs back.
  update(500, 0, false);
  assert(atmosphereMotionScale() < 1.01f && atmosphereBlowForce() < .001f);
  atmosphereUpdate(NAN, NAN, true);
  atmosphereUpdate(INFINITY, INFINITY, true);
  assert(isfinite(atmosphereLevel()) && isfinite(atmosphereMotionScale()));
}

void oneEventPerBreath() {
  atmosphereBegin();
  const int frames = untilBlow();
  assert(frames >= 35 && frames <= 100);
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
  assert(update(34, 1) == 0);
  assert(update(1, 1) == 1);
  atmosphereUpdate(.01f, 1, false);
  assert(!atmosphereTakeBlow());
  assert(update(300, 1, false) == 0);

  atmosphereSetThreshold(10);
  assert(atmosphereThreshold() == .95f);
  atmosphereSetThreshold(-10);
  assert(atmosphereThreshold() == .35f);
  atmosphereSetThreshold(NAN);
  assert(atmosphereThreshold() == .75f);
  atmosphereBegin();
  assert(update(30, 1) == 0);
  atmosphereSetThreshold(.9f);
  assert(update(34, 1) == 0);
  assert(update(1, 1) == 1);
}
}  // namespace

Arduino_GFX *gfx = &graphics;
uint16_t *displayFramebuffer() { return useFrame ? frame.pixels : nullptr; }

int main() {
  drawingAndThemes();
  smoothMovement();
  oneEventPerBreath();
  menuAndThreshold();
  puts("Native atmosphere checks passed (CPU background, themes, pressure, breath gate)");
}
