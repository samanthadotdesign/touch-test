#include "boot_screen.h"
#include "../display.h"
#include "../config.h"

void bootEnter() {}

void bootDraw() {
  gfx->fillScreen(C_BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(C_WHITE, C_BLACK);
  const int16_t x0 = (LCD_W - 21 * 6) / 2;
  gfx->setCursor(x0, 118);
  gfx->print("       __|__");
  gfx->setCursor(x0, 130);
  gfx->print("---o--o-(_)-o--o---");
  displayCenter("airplane mode", 168, 2, C_WHITE, C_BLACK);
  displayCenter("samantha lee", 200, 1, C_GREY, C_BLACK);
}
