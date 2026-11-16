#pragma once
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <string>

// Native-only in-memory NVS replacement, shared across appBegin() calls.
class Preferences {
 public:
  bool begin(const char *name, bool readOnly = false) {
    space = name; readonly = readOnly; return true;
  }
  void end() {}
  uint8_t getUChar(const char *key, uint8_t fallback = 0) const {
    const auto found = values().find(space + "/" + key);
    return found == values().end() ? fallback : found->second;
  }
  size_t putUChar(const char *key, uint8_t value) {
    if (readonly) return 0;
    values()[space + "/" + key] = value;
    return 1;
  }
  static void clearAll() { values().clear(); }
 private:
  static std::map<std::string, uint8_t> &values() {
    static std::map<std::string, uint8_t> store;
    return store;
  }
  std::string space;
  bool readonly = false;
};
