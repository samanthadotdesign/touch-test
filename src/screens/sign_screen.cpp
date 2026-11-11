#include "sign_screen.h"
#include "../display.h"
#include "../storage.h"
#include "../session.h"
#include "../config.h"

static StrokePt pts[MAX_STROKES];
static int nPts = 0;
static bool downLast = false;
static uint32_t lastTouchMs = 0;
static bool hasStroke = false;
static int16_t lastX = -1, lastY = -1;
static bool saved = false;

void signEnter() {
  nPts = 0;
  downLast = false;
  hasStroke = false;
  lastX = lastY = -1;
  lastTouchMs = millis();
  saved = false;
}

void signTouch(bool down, int16_t x, int16_t y) {
  if (!down) {
    downLast = false;
    lastX = lastY = -1;
    return;
  }
  lastTouchMs = millis();
  bool start = !downLast;
  downLast = true;
  hasStroke = true;

  if (nPts < MAX_STROKES) {
    if (!start && lastX >= 0) {
      int dx = x - lastX, dy = y - lastY;
      if (dx * dx + dy * dy < 4) return;  // skip duplicates
    }
    pts[nPts++] = {x, y, (uint8_t)(start ? 1 : 0)};
    lastX = x;
    lastY = y;
  }
}

bool signIdleDone() {
  return hasStroke && (millis() - lastTouchMs >= SIGN_IDLE_MS);
}

static void drawStrokes(uint16_t colour) {
  int16_t px = -1, py = -1;
  for (int i = 0; i < nPts; i++) {
    if (pts[i].start || px < 0) {
      gfx->fillCircle(pts[i].x, pts[i].y, 2, colour);
    } else {
      gfx->drawLine(px, py, pts[i].x, pts[i].y, colour);
      gfx->fillCircle(pts[i].x, pts[i].y, 2, colour);
    }
    px = pts[i].x;
    py = pts[i].y;
  }
}

void signDraw() {
  gfx->fillScreen(C_BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BLACK);
  gfx->setCursor(8, 8);
  gfx->print("sign");
  drawStrokes(C_CYAN);
}

void summaryEnter() {
  if (!saved) {
    int n = storageNextNumber();
    storageSaveSession(n, sessionBlocks(), sessionMinutes(), pts, nPts);
    saved = true;
  }
}

void summaryDraw() {
  gfx->fillScreen(C_BLACK);
  drawStrokes(C_WHITE);
  char line[32];
  snprintf(line, sizeof(line), "%d blocks · %d min", sessionBlocks(), sessionMinutes());
  displayCenter(line, LCD_H - 28, 1, C_GREY, C_BLACK);
}
