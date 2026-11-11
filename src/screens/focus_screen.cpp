#include "focus_screen.h"
#include "../display.h"
#include "../letters.h"
#include "../session.h"
#include "../config.h"

static uint32_t motionStart = 0;
static float motionT = 0;
static uint32_t lastTick = 0;

void focusEnter() {
  lettersPrepareFocus();
  motionStart = millis();
  motionT = 0;
  lastTick = millis();
}

void focusUpdate() {
  uint32_t now = millis();
  float dt = (now - lastTick) / 1000.f;
  lastTick = now;
  if (dt > 0.1f) dt = 0.1f;
  if (!sessionPaused()) motionT += dt;

  float progress = (float)sessionElapsedMs() / (float)FOCUS_MS;
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  lettersUpdateFocus(progress, motionT, sessionPaused());
}

void focusDraw(bool paused) {
  gfx->fillScreen(C_BLACK);
  lettersDraw(C_WHITE);

  char hud[16];
  snprintf(hud, sizeof(hud), "%d/%d", sessionBlockNumber(), sessionBlocks());
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BLACK);
  gfx->setCursor(8, 6);
  gfx->print(hud);

  if (sessionWarning()) {
    char t[8];
    displayMMSS(sessionRemainingMs(), t, sizeof(t));
    gfx->fillRect(50, 4, 140, 30, C_BLACK);
    displayCenter(t, 8, 3, C_YELLOW, C_BLACK);
  }

  displayDrawPausePlay(paused, C_WHITE, C_DARK);
}
