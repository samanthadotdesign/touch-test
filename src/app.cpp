#include "app.h"
#include "config.h"
#include "display.h"
#include "touch.h"
#include "audio.h"
#include "mic.h"
#include "session.h"
#include "storage.h"
#include "screens/boot_screen.h"
#include "screens/quote_screen.h"
#include "screens/picker_screen.h"
#include "screens/focus_screen.h"
#include "screens/break_screen.h"
#include "screens/sign_screen.h"
#include <esp_random.h>
#include <esp_system.h>

static AppState state = ST_BOOT;
static AppState prevState = ST_BOOT;
static uint32_t enteredAt = 0;
static uint32_t lastAnimMs = 0;
static uint32_t lastDrawMs = 0;
static bool dirty = true;
static bool wasDown = false;
static uint32_t ignorePressUntil = 0;
static uint16_t fpsFrames = 0;
static uint32_t fpsT = 0;

const char *appStateName(AppState s) {
  switch (s) {
    case ST_BOOT: return "BOOT";
    case ST_QUOTE: return "QUOTE";
    case ST_SCATTER: return "SCATTER";
    case ST_REGROUP: return "REGROUP";
    case ST_PICKER: return "PICKER";
    case ST_FOCUS: return "FOCUS";
    case ST_FOCUS_PAUSED: return "FOCUS_PAUSED";
    case ST_BREAK: return "BREAK";
    case ST_BREAK_PAUSED: return "BREAK_PAUSED";
    case ST_SIGN: return "SIGN";
    case ST_SUMMARY: return "SUMMARY";
  }
  return "?";
}

AppState appState() { return state; }
uint32_t appStateEnteredAt() { return enteredAt; }

void appGo(AppState s) {
  prevState = state;
  state = s;
  enteredAt = millis();
  lastAnimMs = millis();
  ignorePressUntil = enteredAt + 250;
  dirty = true;
  Serial.printf("state -> %s\n", appStateName(s));

  bool resumeFocus = (s == ST_FOCUS && prevState == ST_FOCUS_PAUSED);
  bool resumeBreak = (s == ST_BREAK && prevState == ST_BREAK_PAUSED);
  if (resumeFocus || resumeBreak) return;

  switch (s) {
    case ST_BOOT: bootEnter(); break;
    case ST_QUOTE: quoteEnter(); break;
    case ST_SCATTER: scatterEnter(); break;
    case ST_REGROUP: regroupEnter(); break;
    case ST_PICKER: pickerEnter(); break;
    case ST_FOCUS: focusEnter(); break;
    case ST_FOCUS_PAUSED: break;
    case ST_BREAK: breakEnter(); break;
    case ST_BREAK_PAUSED: break;
    case ST_SIGN: signEnter(); break;
    case ST_SUMMARY: summaryEnter(); break;
  }
}

static void draw() {
  switch (state) {
    case ST_BOOT: bootDraw(); break;
    case ST_QUOTE:
    case ST_SCATTER:
    case ST_REGROUP: quoteDraw(); break;
    case ST_PICKER: pickerDraw(); break;
    case ST_FOCUS: focusDraw(false); break;
    case ST_FOCUS_PAUSED: focusDraw(true); break;
    case ST_BREAK: breakDraw(false); break;
    case ST_BREAK_PAUSED: breakDraw(true); break;
    case ST_SIGN: signDraw(); break;
    case ST_SUMMARY: summaryDraw(); break;
  }
  displayFlush();
  dirty = false;
}

void appBegin() {
  Serial.println("\nAirplane Mode pomodoro");
  Serial.printf("DEV_TIME_SCALE=%d  PSRAM=%s\n", DEV_TIME_SCALE,
                psramFound() ? "yes" : "no");

  randomSeed(esp_random());
  displayBegin();
  touchBegin();
  storageBegin();
  audioBegin();
  micBegin();
  appGo(ST_BOOT);
  draw();
  lastDrawMs = millis();
  fpsT = millis();
}

void appUpdate() {
  sessionTick();

  uint32_t now = millis();
  int16_t x = 0, y = 0;
  bool down = touchRead(x, y);
  bool pressed = down && !wasDown && now >= ignorePressUntil;
  wasDown = down;
  float dt = (now - lastAnimMs) / 1000.f;
  if (dt > 0.1f) dt = 0.1f;
  lastAnimMs = now;

  switch (state) {
    case ST_BOOT:
      if (now - enteredAt >= BOOT_MS) appGo(ST_QUOTE);
      break;

    case ST_QUOTE:
      if (pressed || blowDetected()) {
        quoteOnTap();
        appGo(ST_SCATTER);
      }
      break;

    case ST_SCATTER:
      scatterUpdate(dt);
      if (scatterDone()) appGo(ST_REGROUP);
      break;

    case ST_REGROUP:
      regroupUpdate(dt);
      if (regroupReadyForPicker()) appGo(ST_PICKER);
      break;

    case ST_PICKER:
      if (pressed) {
        int mins = pickerHit(x, y);
        if (mins) {
          Serial.printf("session %d min (%d blocks)\n", mins, mins / 30);
          sessionStart(mins);
          playRing(1);
          musicStart();
          touchWaitRelease();
          wasDown = false;
          appGo(ST_FOCUS);
        }
      }
      break;

    case ST_FOCUS:
      focusUpdate();
      if (pressed && displayInPause(x, y)) {
        sessionSetPaused(true);
        musicPause();
        touchWaitRelease();
        wasDown = false;
        appGo(ST_FOCUS_PAUSED);
        break;
      }
      if (sessionPhaseDone()) {
        playRing(2);
        sessionStartBreak();
        appGo(ST_BREAK);
      }
      break;

    case ST_FOCUS_PAUSED:
      focusUpdate();
      if (pressed && displayInPause(x, y)) {
        sessionSetPaused(false);
        musicResume();
        touchWaitRelease();
        wasDown = false;
        appGo(ST_FOCUS);
      }
      break;

    case ST_BREAK:
      if (pressed && displayInPause(x, y)) {
        sessionSetPaused(true);
        musicPause();
        touchWaitRelease();
        wasDown = false;
        appGo(ST_BREAK_PAUSED);
        break;
      }
      if (sessionPhaseDone()) {
        if (sessionStartNextFocus()) {
          playRing(3);
          appGo(ST_FOCUS);
        } else {
          playRing(4);
          musicStop();
          appGo(ST_SIGN);
        }
      }
      break;

    case ST_BREAK_PAUSED:
      if (pressed && displayInPause(x, y)) {
        sessionSetPaused(false);
        musicResume();
        touchWaitRelease();
        wasDown = false;
        appGo(ST_BREAK);
      }
      break;

    case ST_SIGN:
      signTouch(down, x, y);
      if (down) dirty = true;
      if (signIdleDone()) appGo(ST_SUMMARY);
      break;

    case ST_SUMMARY:
      if (now - enteredAt >= SUMMARY_MS) appGo(ST_QUOTE);
      break;
  }

  bool anim = state == ST_SCATTER || state == ST_REGROUP || state == ST_FOCUS ||
              state == ST_FOCUS_PAUSED || state == ST_BREAK ||
              state == ST_BREAK_PAUSED || state == ST_SIGN;
  uint32_t minFrame = (state == ST_SIGN) ? 16 : 33;
  if (!dirty && !anim && now - lastDrawMs < 200) return;
  if (!dirty && now - lastDrawMs < minFrame) return;

  draw();
  lastDrawMs = now;
  fpsFrames++;
  if (now - fpsT >= 1000) {
    if (anim) Serial.printf("fps=%u state=%s\n", fpsFrames, appStateName(state));
    fpsFrames = 0;
    fpsT = now;
  }
}
