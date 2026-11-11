#include "picker_screen.h"
#include "../display.h"
#include "../config.h"
#include <math.h>

static const int16_t CX[4] = {70, 170, 70, 170};
static const int16_t CY[4] = {96, 96, 200, 200};
static const int MINUTES[4] = {30, 60, 90, 120};
static const int SEGS[4] = {0, 2, 3, 4};
static const int16_t R = 46;

static void drawDial(int i) {
  int16_t cx = CX[i], cy = CY[i];
  gfx->drawCircle(cx, cy, R, C_WHITE);
  gfx->drawCircle(cx, cy, R - 1, C_WHITE);
  int segs = SEGS[i];
  if (segs >= 2) {
    for (int s = 0; s < segs; s++) {
      float ang = -PI / 2.f + s * (2.f * PI / segs);
      int16_t x2 = cx + (int16_t)(cosf(ang) * (R - 4));
      int16_t y2 = cy + (int16_t)(sinf(ang) * (R - 4));
      gfx->drawLine(cx, cy, x2, y2, C_GREY);
    }
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", MINUTES[i]);
  uint8_t size = 2;
  int16_t w = (int16_t)strlen(buf) * 6 * size;
  gfx->setTextSize(size);
  gfx->setTextColor(C_WHITE, C_BLACK);
  gfx->setCursor(cx - w / 2, cy - 8);
  gfx->print(buf);
}

void pickerEnter() {}

void pickerDraw() {
  gfx->fillScreen(C_BLACK);
  displayCenter("how long?", 18, 2, C_GREY, C_BLACK);
  for (int i = 0; i < 4; i++) drawDial(i);
}

int pickerHit(int16_t x, int16_t y) {
  for (int i = 0; i < 4; i++) {
    int dx = x - CX[i], dy = y - CY[i];
    if (dx * dx + dy * dy <= (R + 10) * (R + 10)) return MINUTES[i];
  }
  return 0;
}
