#pragma once

// Placeholder wording — replace from the board's "start up screen" section.

struct Quote {
  const char *text;
  const char *by;
};

static const Quote QUOTES[] = {
  {"Time present and time past", "T.S. Eliot"},
  {"Time is the moving image of eternity", "Plato, Timaeus"},
  {"Time is a river of passing events", "Marcus Aurelius, Meditations"},
  {"Better see truly than sleep", "Bernard Shaw"},
};
static const int QUOTE_COUNT = 4;

const Quote &quotePick();
const Quote &quoteCurrent();
