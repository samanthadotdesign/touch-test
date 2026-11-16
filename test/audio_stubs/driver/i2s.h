#pragma once
#include <algorithm>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <deque>
#include <vector>

using esp_err_t = int;
using TickType_t = uint32_t;
constexpr esp_err_t ESP_OK = 0, ESP_ERR_TIMEOUT = 0x107, ESP_FAIL = -1;
enum i2s_port_t { I2S_NUM_0 };
enum i2s_mode_t { I2S_MODE_MASTER = 1, I2S_MODE_RX = 8 };
enum i2s_bits_per_sample_t { I2S_BITS_PER_SAMPLE_16BIT = 16 };
enum i2s_bits_per_chan_t { I2S_BITS_PER_CHAN_16BIT = 16 };
enum i2s_channel_fmt_t { I2S_CHANNEL_FMT_RIGHT_LEFT };
enum i2s_comm_format_t { I2S_COMM_FORMAT_STAND_I2S = 1 };
enum i2s_mclk_multiple_t { I2S_MCLK_MULTIPLE_256 = 256 };
constexpr int I2S_PIN_NO_CHANGE = -1;
struct i2s_config_t {
  i2s_mode_t mode;
  uint32_t sample_rate;
  i2s_bits_per_sample_t bits_per_sample;
  i2s_channel_fmt_t channel_format;
  i2s_comm_format_t communication_format;
  int intr_alloc_flags, dma_buf_count, dma_buf_len;
  bool use_apll, tx_desc_auto_clear;
  int fixed_mclk;
  i2s_mclk_multiple_t mclk_multiple;
  i2s_bits_per_chan_t bits_per_chan;
};
struct i2s_pin_config_t { int mck_io_num, bck_io_num, ws_io_num, data_out_num, data_in_num; };
struct AudioTestI2s {
  bool installed = false, running = false, repeat = false;
  int installs = 0, uninstalls = 0, starts = 0, stops = 0, reads = 0;
  esp_err_t installResult = ESP_OK, pinResult = ESP_OK, readResult = ESP_OK;
  i2s_config_t config = {};
  i2s_pin_config_t pins = {};
  std::deque<std::vector<int16_t>> blocks;
};
extern AudioTestI2s audioTestI2s;
inline esp_err_t i2s_driver_install(i2s_port_t, const i2s_config_t *config, int queueSize, void *queue) {
  assert(queueSize == 0 && queue == nullptr);
  audioTestI2s.installs++;
  audioTestI2s.config = *config;
  if (audioTestI2s.installResult != ESP_OK) return audioTestI2s.installResult;
  audioTestI2s.installed = audioTestI2s.running = true;
  return ESP_OK;
}
inline esp_err_t i2s_driver_uninstall(i2s_port_t) {
  audioTestI2s.uninstalls++;
  audioTestI2s.installed = audioTestI2s.running = false;
  audioTestI2s.blocks.clear();
  return ESP_OK;
}
inline esp_err_t i2s_start(i2s_port_t) { audioTestI2s.starts++; audioTestI2s.running = true; return ESP_OK; }
inline esp_err_t i2s_stop(i2s_port_t) { audioTestI2s.stops++; audioTestI2s.running = false; return ESP_OK; }
inline esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t *pins) { audioTestI2s.pins = *pins; return audioTestI2s.pinResult; }
inline esp_err_t i2s_read(i2s_port_t, void *destination, size_t size, size_t *bytes, TickType_t wait) {
  assert(wait == 0 && audioTestI2s.installed && audioTestI2s.running);
  audioTestI2s.reads++;
  *bytes = 0;
  if (audioTestI2s.readResult != ESP_OK) return audioTestI2s.readResult;
  if (audioTestI2s.repeat) {
    auto *samples = static_cast<int16_t *>(destination);
    for (size_t i = 0; i < size / sizeof(int16_t); ++i) samples[i] = (i & 1) ? 0 : 3277;
    *bytes = size;
    return ESP_OK;
  }
  if (audioTestI2s.blocks.empty()) return ESP_ERR_TIMEOUT;
  const auto block = audioTestI2s.blocks.front();
  audioTestI2s.blocks.pop_front();
  *bytes = std::min(size, block.size() * sizeof(int16_t));
  memcpy(destination, block.data(), *bytes);
  return ESP_OK;
}
