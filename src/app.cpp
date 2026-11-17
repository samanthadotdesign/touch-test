#include "app.h"
#include "display.h"
#include "touch.h"
#include "letters.h"
#include "poetry_data.h"
#include "atmosphere.h"
#include "audio_input.h"
#include <Preferences.h>
#include <esp_random.h>
#include <math.h>

namespace {
AppState state = ST_MOVING;
uint32_t enteredAt = 0, lastUpdate = 0, lastDraw = 0, ignorePressUntil = 0;
uint32_t menuTouchedAt = 0;
bool wasDown = false, dirty = true, menuOpen = false, settingsReady = false;
int poemIndex = -1, motionIndex = -1;
Preferences settings;
constexpr int MENU_TOP = 88;
constexpr uint8_t MIC_PROFILE_VERSION = 2;
constexpr uint8_t DEFAULT_THRESHOLD_PERCENT = 10;
constexpr uint8_t MIN_THRESHOLD_PERCENT = 5;
constexpr uint8_t MAX_THRESHOLD_PERCENT = 95;

int pickDifferent(int count, int previous) {
  if (count <= 1) return 0;
  if (previous < 0) return (int)random(count);
  const int n = (int)random(count - 1);
  return n >= previous ? n + 1 : n;
}
void nextPoem(bool initial) {
  poemIndex = pickDifferent(POEM_COUNT, poemIndex);
  motionIndex = pickDifferent(MOTION_COUNT, motionIndex);
  if (initial) lettersLoadPoem(POEMS[poemIndex], MOTIONS[motionIndex]);
  else lettersPrepareNext(POEMS[poemIndex], MOTIONS[motionIndex]);
  Serial.printf("poem=%s motion=%s\n", POEMS[poemIndex].id, MOTIONS[motionIndex].id);
}
bool inside(int x, int y, int left, int top, int width, int height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}
void textAt(const char *text, int x, int y, uint16_t colour) {
  gfx->setTextSize(1);
  gfx->setTextColor(colour);
  gfx->setCursor(x, y);
  while (*text) gfx->write((uint8_t)*text++);
}
void changeThreshold(int change) {
  int percent = (int)lroundf(atmosphereThreshold() * 100) + change;
  if (percent < MIN_THRESHOLD_PERCENT) percent = MIN_THRESHOLD_PERCENT;
  if (percent > MAX_THRESHOLD_PERCENT) percent = MAX_THRESHOLD_PERCENT;
  atmosphereSetThreshold(percent / 100.f);
  if (settingsReady) settings.putUChar("threshold", (uint8_t)percent);
}
void menuPress(int x, int y, uint32_t now) {
  menuTouchedAt = now;
  dirty = true;
  if (y < MENU_TOP || inside(x, y, 208, 88, 32, 30)) { menuOpen = false; return; }
  for (int i = 0; i < 3; ++i) {
    if (inside(x, y, 12, 120 + i * 36, 216, 31)) {
      atmosphereSetTheme((AtmosphereTheme)i);
      return;
    }
  }
  if (inside(x, y, 12, 233, 104, 26)) {
    audioInputSetEnabled(!audioInputEnabled());
  } else if (inside(x, y, 154, 261, 30, 24)) {
    changeThreshold(-5);
  } else if (inside(x, y, 194, 261, 30, 24)) {
    changeThreshold(5);
  }
}
void drawMenu() {
  const uint16_t paper = 0xf79c, line = 0xce79, ink = 0x2965, muted = 0x738e;
  gfx->fillRoundRect(0, MENU_TOP, LCD_W, LCD_H - MENU_TOP, 12, paper);
  gfx->fillRect(0, MENU_TOP + 16, LCD_W, LCD_H - MENU_TOP - 16, paper);
  textAt("Atmosphere", 12, 101, ink);
  textAt("x", 219, 100, muted);
  const char *names[] = {"Night clouds", "Paper clouds", "Colour fields"};
  const char *details[] = {"Black/grey, white text", "White/grey, black text", "Drifting colour palettes"};
  const uint16_t swatches[][3] = {{0x1082, 0x738e, 0xce59}, {0xffff, 0xe73c, 0xbdf7}, {0xb454, 0xf6b5, 0xdbae}};
  for (int i = 0; i < 3; ++i) {
    const int y = 120 + i * 36;
    const bool active = atmosphereTheme() == i;
    gfx->fillRoundRect(10, y - 2, 220, 35, 5, active ? 0x8ccf : line);
    gfx->fillRoundRect(11, y - 1, 218, 33, 4, active ? 0xe75b : 0xffde);
    for (int band = 0; band < 3; ++band) gfx->fillRect(17 + band * 7, y + 4, 7, 22, swatches[i][band]);
    textAt(names[i], 48, y + 3, ink);
    textAt(details[i], 48, y + 16, muted);
    if (active) textAt("*", 218, y + 3, 0x5b49);
  }
  gfx->fillRoundRect(12, 233, 104, 26, 4, audioInputEnabled() ? 0x5b49 : 0xe75b);
  textAt(audioInputEnabled() ? "Mic: on" : "Enable mic", 22, 242,
         audioInputEnabled() ? C_WHITE : ink);
  gfx->fillRect(128, 244, 100, 3, line);
  gfx->fillRect(128, 244, (int)(atmosphereLevel() * 100), 3, 0x738c);
  const int marker = 128 + (int)(atmosphereThreshold() * 100);
  gfx->fillRect(marker, 241, 1, 9, 0x9b68);
  char label[20];
  snprintf(label, sizeof(label), "Blow: %d%%", (int)lroundf(atmosphereThreshold() * 100));
  textAt(label, 12, 269, ink);
  gfx->fillRoundRect(154, 261, 30, 24, 4, 0xe75b);
  gfx->fillRoundRect(194, 261, 30, 24, 4, 0xe75b);
  textAt("-", 166, 269, ink);
  textAt("+", 206, 269, ink);
  textAt(!audioInputAvailable() ? "Microphone not detected" : audioInputEnabled()
           ? "Blow, hold, then release." : "Enable mic to use breath.", 12, 292, muted);
  textAt("Closes after 6 seconds", 12, 307, muted);
}
}

const char *appStateName(AppState s) {
  switch (s) {
    case ST_MOVING: return "MOVING";
    case ST_ASSEMBLING: return "ASSEMBLING";
    case ST_READING: return "READING";
    case ST_DISSOLVING: return "DISSOLVING";
  }
  return "?";
}
AppState appState() { return state; }
uint32_t appStateEnteredAt() { return enteredAt; }
bool appMenuVisible() { return menuOpen; }
void appGo(AppState s) {
  state = s;
  enteredAt = millis();
  ignorePressUntil = enteredAt + 250;
  dirty = true;
  if (s == ST_MOVING) lettersAdoptNext();
  else if (s == ST_ASSEMBLING) lettersAssemble();
  else if (s == ST_DISSOLVING) { nextPoem(false); lettersDissolve(); }
  Serial.printf("state -> %s\n", appStateName(s));
}
void appBegin() {
  Serial.println("\nAirplane Mode: poetry in motion");
  randomSeed(esp_random());
  displayBegin();
  touchBegin();
  atmosphereBegin();
  audioInputBegin();
  if (settingsReady) settings.end();
  settingsReady = settings.begin("poetry", false);
  if (settingsReady) {
    const uint8_t saved = settings.getUChar("threshold", DEFAULT_THRESHOLD_PERCENT);
    uint8_t threshold = saved >= MIN_THRESHOLD_PERCENT && saved <= MAX_THRESHOLD_PERCENT
        ? saved : DEFAULT_THRESHOLD_PERCENT;
    if (settings.getUChar("micProfile", 0) < MIC_PROFILE_VERSION) {
      // Install the gentler response once. Preserve an already-lower choice,
      // and retry next boot if NVS cannot save the new threshold or marker.
      if (threshold > DEFAULT_THRESHOLD_PERCENT) threshold = DEFAULT_THRESHOLD_PERCENT;
      if (settings.putUChar("threshold", threshold) == sizeof(uint8_t))
        settings.putUChar("micProfile", MIC_PROFILE_VERSION);
    }
    atmosphereSetThreshold(threshold / 100.f);
  }
  // Every boot starts colourful. NVS remembers only the last colourway so
  // consecutive boots differ; Night/Paper are temporary choices this session.
  const uint8_t previousColourway = settingsReady
      ? settings.getUChar("colourway", ATM_COLOURWAY_COUNT) : ATM_COLOURWAY_COUNT;
  const int previous = previousColourway < ATM_COLOURWAY_COUNT ? previousColourway : -1;
  atmosphereSetColourway(pickDifferent(ATM_COLOURWAY_COUNT, previous));
  atmosphereSetTheme(ATM_COLOUR);
  if (settingsReady) settings.putUChar("colourway", atmosphereColourway());
  menuOpen = wasDown = false;
  lettersSetResponse(1, 0);
  nextPoem(true);
  appGo(ST_MOVING);
  lettersDraw();
  displayFlush();
  lastUpdate = lastDraw = millis();
  dirty = false;
}
void appUpdate() {
  const uint32_t now = millis();
  int16_t x = 0, y = 0;
  const bool down = touchRead(x, y);
  const bool newPress = down && !wasDown;
  wasDown = down;
  bool pressed = false;
  if (newPress) {
    if (menuOpen) menuPress(x, y, now);
    else if (x >= LCD_W - 44 && y >= LCD_H - 44) {
      menuOpen = true;
      menuTouchedAt = now;
      dirty = true;
    } else pressed = (int32_t)(now - ignorePressUntil) >= 0;
  }
  if (menuOpen && (uint32_t)(now - menuTouchedAt) >= 6000) { menuOpen = false; dirty = true; }
  audioInputUpdate();
  float dt = (now - lastUpdate) / 1000.f;
  if (dt > .1f) dt = .1f;
  lastUpdate = now;
  atmosphereUpdate(dt, audioInputLevel(), audioInputEnabled(), !menuOpen);
  lettersSetResponse(atmosphereMotionScale(), atmosphereBlowForce());
  lettersUpdate(dt);
  const bool blew = atmosphereTakeBlow();
  const bool advance = pressed || blew;
  switch (state) {
    case ST_MOVING: if (advance) appGo(ST_ASSEMBLING); break;
    case ST_ASSEMBLING: if (lettersTransitionDone()) appGo(ST_READING); break;
    case ST_READING: if (advance) appGo(ST_DISSOLVING); break;
    case ST_DISSOLVING: if (lettersTransitionDone()) appGo(ST_MOVING); break;
  }
  // Backgrounds continue drifting while the poem itself stays readable.
  if (!dirty && now - lastDraw < 33) return;
  lettersDraw();
  if (menuOpen) drawMenu();
  displayFlush();
  lastDraw = now;
  dirty = false;
}
