#pragma once

#include <Arduino.h>

enum AppState : uint8_t {
  ST_BOOT,
  ST_QUOTE,
  ST_SCATTER,
  ST_REGROUP,
  ST_PICKER,
  ST_FOCUS,
  ST_FOCUS_PAUSED,
  ST_BREAK,
  ST_BREAK_PAUSED,
  ST_SIGN,
  ST_SUMMARY
};

void appBegin();
void appUpdate();
void appGo(AppState s);
AppState appState();
const char *appStateName(AppState s);
uint32_t appStateEnteredAt();
