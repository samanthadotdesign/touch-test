#include "quote_screen.h"
#include "../display.h"
#include "../letters.h"
#include "../quotes.h"
#include "../config.h"
#include "../app.h"

static uint32_t regroupSettledAt = 0;
static bool regroupHeld = false;
static uint32_t regroupEnterMs = 0;

void quoteEnter() {
  const Quote &q = quotePick();
  lettersLoadQuote(q.text);
  Serial.printf("quote: \"%s\" — %s\n", q.text, q.by);
}

void quoteOnTap() {
  lettersStartScatter(150.f);
}

void scatterEnter() {
  // scatter already started from the tap
}

void scatterUpdate(float dt) { lettersUpdateScatter(dt); }

bool scatterDone() { return lettersScatterDone(); }

void regroupEnter() {
  lettersStartClock();
  regroupSettledAt = 0;
  regroupHeld = false;
  regroupEnterMs = millis();
}

void regroupUpdate(float dt) {
  lettersUpdateRegroup(dt);
  if (!regroupHeld && (lettersRegroupSettled() || millis() - regroupEnterMs > 2500)) {
    if (regroupSettledAt == 0) regroupSettledAt = millis();
    if (millis() - regroupSettledAt >= CLOCK_HOLD_MS) regroupHeld = true;
  }
}

bool regroupReadyForPicker() { return regroupHeld; }

void quoteDraw() {
  gfx->fillScreen(C_BLACK);
  lettersDraw(C_WHITE);
  if (appState() == ST_QUOTE) {
    displayCenter(quoteCurrent().by, LCD_H - 18, 1, C_GREY, C_BLACK);
  }
}
