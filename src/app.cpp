#include "app.h"
#include "display.h"
#include "touch.h"
#include "letters.h"
#include "poetry_data.h"
#include <esp_random.h>

namespace {
AppState state = ST_MOVING;
uint32_t enteredAt = 0, lastUpdate = 0, lastDraw = 0, ignorePressUntil = 0;
bool wasDown = false, dirty = true;
int poemIndex = -1, motionIndex = -1;

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
void appGo(AppState s) {
  state = s;
  enteredAt = millis();
  ignorePressUntil = enteredAt + 250;
  dirty = true;
  if (s == ST_MOVING) lettersAdoptNext();
  else if (s == ST_ASSEMBLING) lettersAssemble();
  else if (s == ST_DISSOLVING) {
    nextPoem(false);
    lettersDissolve();
  }
  Serial.printf("state -> %s\n", appStateName(s));
}
void appBegin() {
  Serial.println("\nAirplane Mode: poetry in motion");
  randomSeed(esp_random());
  displayBegin();
  touchBegin();
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
  // One action per new touch. A held finger never advances a second state.
  const bool pressed = down && !wasDown && (int32_t)(now - ignorePressUntil) >= 0;
  wasDown = down;
  float dt = (now - lastUpdate) / 1000.f;
  if (dt > .1f) dt = .1f;
  lastUpdate = now;
  lettersUpdate(dt);

  switch (state) {
    case ST_MOVING:
      if (pressed) appGo(ST_ASSEMBLING);
      break;
    case ST_ASSEMBLING:
      if (lettersTransitionDone()) appGo(ST_READING);
      break;
    case ST_READING:
      if (pressed) appGo(ST_DISSOLVING);
      break;
    case ST_DISSOLVING:
      if (lettersTransitionDone()) appGo(ST_MOVING);
      break;
  }
  if (!dirty && (state == ST_READING || now - lastDraw < 33)) return;
  lettersDraw();
  displayFlush();
  lastDraw = now;
  dirty = false;
}
