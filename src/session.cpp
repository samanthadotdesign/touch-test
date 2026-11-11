#include "session.h"

static int minutes_ = 0;
static int blocks_ = 0;
static int blockIndex_ = 0;
static bool inBreak_ = false;
static bool paused_ = false;
static bool active_ = false;
static uint32_t elapsed_ = 0;
static uint32_t lastMs_ = 0;

void sessionStart(int minutes) {
  minutes_ = minutes;
  blocks_ = minutes / 30;
  if (blocks_ < 1) blocks_ = 1;
  blockIndex_ = 0;
  inBreak_ = false;
  paused_ = false;
  active_ = true;
  elapsed_ = 0;
  lastMs_ = millis();
}

void sessionTick() {
  if (!active_) return;
  uint32_t now = millis();
  uint32_t dt = now - lastMs_;
  lastMs_ = now;
  if (!paused_) elapsed_ += dt * (uint32_t)DEV_TIME_SCALE;
}

void sessionSetPaused(bool paused) {
  if (paused_ == paused) return;
  paused_ = paused;
  lastMs_ = millis();
}

bool sessionPaused() { return paused_; }
int sessionMinutes() { return minutes_; }
int sessionBlocks() { return blocks_; }
int sessionBlockNumber() { return blockIndex_ + 1; }
bool sessionInBreak() { return inBreak_; }
uint32_t sessionElapsedMs() { return elapsed_; }

uint32_t sessionDurationMs() { return inBreak_ ? BREAK_MS : FOCUS_MS; }

uint32_t sessionRemainingMs() {
  uint32_t d = sessionDurationMs();
  return elapsed_ >= d ? 0 : d - elapsed_;
}

bool sessionPhaseDone() { return active_ && elapsed_ >= sessionDurationMs(); }

bool sessionWarning() {
  return active_ && !inBreak_ && elapsed_ >= WARN_AT_MS;
}

void sessionStartBreak() {
  inBreak_ = true;
  paused_ = false;
  elapsed_ = 0;
  lastMs_ = millis();
}

bool sessionStartNextFocus() {
  if (blockIndex_ + 1 >= blocks_) {
    active_ = false;
    return false;
  }
  blockIndex_++;
  inBreak_ = false;
  paused_ = false;
  elapsed_ = 0;
  lastMs_ = millis();
  return true;
}
