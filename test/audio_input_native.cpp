#include "audio_input.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

AudioTestWire Wire;
AudioTestI2s audioTestI2s;
static uint32_t nowMs = 0;
static uint8_t lastOutputPin = 255, lastOutputValue = 255;
uint32_t millis() { return nowMs; }
void pinMode(uint8_t pin, uint8_t mode) { assert(pin == 1 && mode == OUTPUT); }
void digitalWrite(uint8_t pin, uint8_t value) { lastOutputPin = pin; lastOutputValue = value; }

static void update(size_t count = 1) {
  for (size_t i = 0; i < count; ++i) { nowMs += 10; audioInputUpdate(); }
}
static void resetFixture(bool present = true) {
  audioInputSetEnabled(false);
  update(20);
  Wire = AudioTestWire();
  Wire.present = present;
  audioTestI2s = AudioTestI2s();
  nowMs = 0;
}
static std::vector<int16_t> block(int16_t left, int16_t right, size_t frames = 128) {
  std::vector<int16_t> samples(frames * 2);
  for (size_t i = 0; i < frames; ++i) { samples[i * 2] = left; samples[i * 2 + 1] = right; }
  return samples;
}
static void finishStartup() { update(40); }

int main() {
  resetFixture(false);
  assert(!audioInputBegin());
  assert(!audioInputAvailable() && !audioInputEnabled());
  assert(!audioInputSetEnabled(true));
  assert(audioTestI2s.installs == 0 && audioInputLevel() == 0);
  assert(lastOutputPin == 1 && lastOutputValue == HIGH);

  resetFixture();
  assert(audioInputBegin() && audioInputAvailable() && !audioInputEnabled());
  assert(Wire.address == 0x18 && Wire.writes.empty());
  assert(audioInputSetEnabled(true));
  assert(audioInputEnabled() && audioTestI2s.installed);
  assert(audioTestI2s.config.sample_rate == 16000);
  assert(audioTestI2s.config.mode == (I2S_MODE_MASTER | I2S_MODE_RX));
  assert(audioTestI2s.config.mclk_multiple == I2S_MCLK_MULTIPLE_256);
  assert(audioTestI2s.pins.mck_io_num == 4 && audioTestI2s.pins.bck_io_num == 5);
  assert(audioTestI2s.pins.ws_io_num == 7 && audioTestI2s.pins.data_in_num == 6);
  assert(audioTestI2s.pins.data_out_num == I2S_PIN_NO_CHANGE);
  audioInputUpdate();
  assert(Wire.writes.size() == 4 && audioTestI2s.reads == 0);
  // Bring-up is staged; DMA can never publish a startup spike.
  audioTestI2s.repeat = true;
  update(10);
  assert(audioInputLevel() == 0);
  finishStartup();
  assert(audioInputLevel() == 1);
  const int readsBefore = audioTestI2s.reads;
  audioInputUpdate();
  assert(audioTestI2s.reads - readsBefore == 8);
  audioTestI2s.repeat = false;
  audioTestI2s.blocks.push_back(block(0, 655));
  update();
  assert(fabsf(audioInputLevel() - .306f) < .002f); // quiet .02 RMS breath
  audioTestI2s.blocks.push_back(block(655, 0)); update();
  assert(fabsf(audioInputLevel() - .306f) < .002f); // either active mono slot
  // No data eventually clears stale levels rather than latching a strong blow.
  update(9); assert(audioInputLevel() > .3f);
  update(); assert(audioInputLevel() == 0);
  audioTestI2s.blocks.push_back(block(32767, 0)); update(); assert(audioInputLevel() == 1);
  audioTestI2s.blocks.push_back(block(65, 0)); update(); assert(audioInputLevel() == 0);
  audioTestI2s.blocks.push_back(block(0, 98)); update(); assert(audioInputLevel() == 0);
  audioTestI2s.blocks.push_back(block(163, 0)); update();
  assert(audioInputLevel() > .03f && audioInputLevel() < .04f);

  const int stopsBefore = audioTestI2s.stops;
  assert(audioInputSetEnabled(false));
  assert(!audioInputEnabled() && audioInputLevel() == 0 && !audioTestI2s.running);
  assert(audioTestI2s.stops == stopsBefore + 1);
  // Repeated disable calls must not keep restarting the power-down sequence.
  for (int i = 0; i < 5; ++i) { audioInputSetEnabled(false); update(); }
  assert(!audioTestI2s.installed && audioTestI2s.uninstalls == 1);
  assert(audioInputAvailable());

  assert(audioInputSetEnabled(true));
  update(2);
  assert(audioInputSetEnabled(false));
  update(10);
  assert(!audioInputEnabled() && !audioTestI2s.installed && audioInputLevel() == 0);
  // Re-enable halfway through shutdown reuses the driver, then completes setup.
  assert(audioInputSetEnabled(true)); update(2);
  audioInputSetEnabled(false); update();
  assert(audioInputSetEnabled(true)); finishStartup();
  assert(audioInputEnabled() && audioTestI2s.running);
  audioTestI2s.blocks.push_back(block(655, 0)); update(); assert(audioInputLevel() > .3f);

  audioTestI2s.readResult = ESP_FAIL; update();
  assert(!audioInputAvailable() && !audioInputEnabled() && !audioTestI2s.installed && audioInputLevel() == 0);
  resetFixture(); assert(audioInputBegin());
  audioTestI2s.pinResult = ESP_FAIL;
  assert(!audioInputSetEnabled(true));
  assert(!audioInputEnabled() && !audioTestI2s.installed && audioTestI2s.uninstalls == 1);
  resetFixture(); assert(audioInputBegin());
  audioTestI2s.installResult = ESP_FAIL;
  assert(!audioInputSetEnabled(true));
  assert(!audioInputEnabled() && audioTestI2s.uninstalls == 0);
  resetFixture(); assert(audioInputBegin());
  Wire.failRegister = 0x44;
  assert(audioInputSetEnabled(true)); update();
  assert(!audioInputAvailable() && !audioInputEnabled() && !audioTestI2s.installed);
  puts("Audio input native tests passed");
}
