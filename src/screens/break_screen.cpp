#include "break_screen.h"
#include "../display.h"
#include "../session.h"
#include "../config.h"

void breakEnter() {}

void breakDraw(bool paused) {
  gfx->fillScreen(C_WHITE);
  displayCenter("break", 88, 2, C_GREY, C_WHITE);

  char t[8];
  displayMMSS(sessionRemainingMs(), t, sizeof(t));
  displayCenter(t, 140, 4, C_BLACK, C_WHITE);

  char hud[16];
  snprintf(hud, sizeof(hud), "%d/%d", sessionBlockNumber(), sessionBlocks());
  displayCenter(hud, 200, 1, C_GREY, C_WHITE);

  displayDrawPausePlay(paused, C_BLACK, C_WHITE);
}
