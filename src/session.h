#pragma once

#include <Arduino.h>
#include "config.h"

void sessionStart(int minutes);
void sessionTick();
void sessionSetPaused(bool paused);
bool sessionPaused();
int sessionMinutes();
int sessionBlocks();
int sessionBlockNumber();   // 1-based
bool sessionInBreak();
uint32_t sessionElapsedMs();
uint32_t sessionDurationMs();
uint32_t sessionRemainingMs();
bool sessionPhaseDone();
bool sessionWarning();
void sessionStartBreak();
bool sessionStartNextFocus();  // false if the last break just ended
