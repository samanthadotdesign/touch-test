#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <utility>

struct AudioTestWire {
  bool present = true;
  int failRegister = -1;
  uint8_t address = 0;
  std::vector<uint8_t> pending;
  std::vector<std::pair<uint8_t, uint8_t>> writes;
  void beginTransmission(uint8_t value) { address = value; pending.clear(); }
  size_t write(uint8_t value) { pending.push_back(value); return 1; }
  uint8_t endTransmission() {
    if (!present) return 2;
    if (pending.empty()) return 0;
    if (pending.size() != 2) return 1;
    writes.push_back({pending[0], pending[1]});
    return pending[0] == failRegister ? 3 : 0;
  }
};
extern AudioTestWire Wire;
