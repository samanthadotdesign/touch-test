/*
 * ES8311 register setup adapted from Espressif's Apache-2.0 driver:
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * https://github.com/espressif/esp-adf/blob/master/components/esp_codec_dev/device/es8311/es8311.c
 *
 * Board wiring: LCDWIKI ES3C28P Arduino Demo Instructions, pp. 4-5:
 * https://www.lcdwiki.com/res/ES3C28P/2.8inch_ES3C28P_ES3N28P_arduino_Demo_Instructions.pdf
 * Codec address 0x18 / analogue MIC path: LCDWIKI User Manual, p. 22:
 * https://www.lcdwiki.com/res/ES3C28P/2.8inch_IPS_ESP32-S3_ES3C28P_ES3N28P_User_Manual.pdf
 */
#include "audio_input.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <math.h>

// Keep this module buildable while board-level pin definitions remain optional.
#ifndef AUDIO_CODEC_ADDR
#define AUDIO_CODEC_ADDR 0x18
#endif
#ifndef AUDIO_MCLK
#define AUDIO_MCLK 4
#endif
#ifndef AUDIO_BCLK
#define AUDIO_BCLK 5
#endif
#ifndef AUDIO_WS
#define AUDIO_WS 7
#endif
#ifndef AUDIO_DIN
#define AUDIO_DIN 6
#endif
#ifndef AUDIO_AMP_EN
#define AUDIO_AMP_EN 1
#endif

namespace {
constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kWarmupMs = 120;
constexpr uint32_t kStaleMs = 100;
constexpr size_t kFramesPerBuffer = 128;
constexpr size_t kDmaBuffers = 8;
constexpr size_t kRegisterBudget = 4;
constexpr float kNoiseFloor = .003f;
constexpr float kLevelGain = 18.f;

struct RegisterValue { uint8_t reg, value; };

// Espressif es8311_open(), the 4.096 MHz / 16 kHz coefficient entry, and
// es8311_start(ADC), specialized to analogue microphone, slave, standard I2S,
// 16-bit samples, MCLK pin, no DAC reference channel. The final 0x0D write is
// deliberately after I2S starts: the codec's ADC needs running clocks to arm.
const RegisterValue kStart[] = {
  {0x0D, 0xFA}, {0x44, 0x08}, {0x44, 0x08}, {0x01, 0x30},
  {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10},
  {0x05, 0x00}, {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F},
  {0x11, 0x7F}, {0x00, 0x80}, {0x00, 0x80}, {0x01, 0x3F},
  {0x06, 0x00}, {0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A},
  {0x44, 0x08},
  // pre-divider 1, multiplier 1, ADC/DAC divider 1, OSR 16, LRCK 256.
  {0x02, 0x00}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x10},
  {0x07, 0x00}, {0x08, 0xFF}, {0x06, 0x03},
  {0x09, 0x0C}, {0x0A, 0x0C},
  {0x00, 0x80}, {0x01, 0x3F}, {0x09, 0x0C}, {0x0A, 0x0C},
  {0x17, 0xBF}, {0x0E, 0x02}, {0x14, 0x1A}, {0x14, 0x1A},
  {0x0D, 0x01}, {0x15, 0x40}, {0x37, 0x08}, {0x45, 0x00}
};

// Espressif es8311_suspend(). Capture is already stopped before these writes.
const RegisterValue kStop[] = {
  {0x32, 0x00}, {0x17, 0x00}, {0x0E, 0xFF}, {0x12, 0x02},
  {0x14, 0x00}, {0x0D, 0xFA}, {0x15, 0x00}, {0x02, 0x10},
  {0x00, 0x00}, {0x00, 0x1F}, {0x01, 0x30}, {0x01, 0x00},
  {0x45, 0x00}, {0x0D, 0xFC}, {0x02, 0x00}
};

enum class Phase : uint8_t { Off, Starting, Capturing, Stopping };
Phase phase = Phase::Off;
bool available = false, enabled = false, installed = false;
size_t registerIndex = 0;
uint32_t startedMs = 0, lastDataMs = 0;
float level = 0;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(static_cast<uint8_t>(AUDIO_CODEC_ADDR));
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void uninstall() {
  if (installed) i2s_driver_uninstall(kPort);
  installed = false;
}

void fail() {
  enabled = available = false;
  level = 0;
  // Stop and release capture even if the codec stopped acknowledging I2C.
  uninstall();
  phase = Phase::Off;
  digitalWrite(AUDIO_AMP_EN, HIGH);
}

bool startI2s() {
  if (installed) return i2s_start(kPort) == ESP_OK;
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = kSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = 0;
  config.dma_buf_count = kDmaBuffers;
  config.dma_buf_len = kFramesPerBuffer;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
  if (i2s_driver_install(kPort, &config, 0, nullptr) != ESP_OK) return false;
  installed = true;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = AUDIO_MCLK;
  pins.bck_io_num = AUDIO_BCLK;
  pins.ws_io_num = AUDIO_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = AUDIO_DIN;
  if (i2s_set_pin(kPort, &pins) != ESP_OK) { uninstall(); return false; }
  return true;
}
}  // namespace

bool audioInputBegin() {
  enabled = false;
  level = 0;
  uninstall();
  phase = Phase::Off;
  pinMode(AUDIO_AMP_EN, OUTPUT);
  digitalWrite(AUDIO_AMP_EN, HIGH);  // LCDWIKI PA is active low; never play mic audio.
  // Wire belongs to touchBegin(). Do not call begin()/end() or reassign its pins.
  Wire.beginTransmission(static_cast<uint8_t>(AUDIO_CODEC_ADDR));
  available = Wire.endTransmission() == 0;
  if (available) { phase = Phase::Stopping; registerIndex = 0; }
  return available;
}

bool audioInputSetEnabled(bool value) {
  if (!value) {
    enabled = false;
    level = 0;
    if (installed && i2s_stop(kPort) != ESP_OK) uninstall();
    if (available && phase != Phase::Off && phase != Phase::Stopping) {
      phase = Phase::Stopping;
      registerIndex = 0;
    }
    return true;
  }
  if (enabled) return true;
  if (!available && !audioInputBegin()) return false;
  if (!startI2s()) { fail(); return false; }
  enabled = true;
  level = 0;
  registerIndex = 0;
  phase = Phase::Starting;
  return true;
}

bool audioInputAvailable() { return available; }
bool audioInputEnabled() { return enabled; }
float audioInputLevel() { return enabled ? level : 0; }

void audioInputUpdate() {
  if (phase == Phase::Starting || phase == Phase::Stopping) {
    const bool starting = phase == Phase::Starting;
    const RegisterValue *sequence = starting ? kStart : kStop;
    const size_t count = starting ? sizeof(kStart) / sizeof(kStart[0]) : sizeof(kStop) / sizeof(kStop[0]);
    for (size_t writes = 0; writes < kRegisterBudget && registerIndex < count; ++writes, ++registerIndex) {
      if (!writeRegister(sequence[registerIndex].reg, sequence[registerIndex].value)) { fail(); return; }
    }
    if (registerIndex < count) return;
    if (!starting) { uninstall(); phase = Phase::Off; return; }
    phase = Phase::Capturing;
    startedMs = lastDataMs = millis();
  }
  if (phase != Phase::Capturing || !enabled) return;

  const uint32_t now = millis();
  float energy[2] = {0, 0};
  size_t frames = 0;
  int16_t samples[kFramesPerBuffer * 2];
  // At most the DMA ring's worth per update; a timeout of zero never waits for
  // future samples. Audio acquisition cannot hold up the display or touch loop.
  for (size_t block = 0; block < kDmaBuffers; ++block) {
    size_t bytes = 0;
    const esp_err_t result = i2s_read(kPort, samples, sizeof(samples), &bytes, 0);
    if (result != ESP_OK && result != ESP_ERR_TIMEOUT) { fail(); return; }
    const size_t received = bytes / (sizeof(int16_t) * 2);
    for (size_t i = 0; i < received; ++i) {
      const float a = samples[i * 2] / 32768.0f;
      const float b = samples[i * 2 + 1] / 32768.0f;
      energy[0] += a * a;
      energy[1] += b * b;
    }
    frames += received;
    if (bytes < sizeof(samples)) break;
  }
  if (now - startedMs < kWarmupMs) { level = 0; return; }
  if (frames) {
    // The codec is mono. Use its active slot without halving the amplitude when
    // the other stereo slot is silent; RMS is independent of DMA slot ordering.
    const float rms = sqrtf(fmaxf(energy[0], energy[1]) / static_cast<float>(frames));
    level = fminf(1.0f, fmaxf(0.0f, (rms - kNoiseFloor) * kLevelGain));
    lastDataMs = now;
  } else if (now - lastDataMs >= kStaleMs) {
    level = 0;
  }
}
