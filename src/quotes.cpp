#include "quotes.h"
#include <Arduino.h>

static int last = -1;

const Quote &quotePick() {
  int n;
  do {
    n = (int)random(QUOTE_COUNT);
  } while (QUOTE_COUNT > 1 && n == last);
  last = n;
  return QUOTES[n];
}

const Quote &quoteCurrent() {
  return QUOTES[last < 0 ? 0 : last];
}
