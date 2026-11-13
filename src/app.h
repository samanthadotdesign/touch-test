#pragma once
#include <Arduino.h>

enum AppState : uint8_t {
  ST_MOVING,
  ST_ASSEMBLING,
  ST_READING,
  ST_DISSOLVING
};

void appBegin();
void appUpdate();
void appGo(AppState s);
AppState appState();
const char *appStateName(AppState s);
uint32_t appStateEnteredAt();
