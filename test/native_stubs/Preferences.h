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
    writeCounts()[space + "/" + key]++;
    return 1;
  }
  static void clearAll() { values().clear(); writeCounts().clear(); }
  static size_t writesTo(const char *space, const char *key) {
    return writeCounts()[std::string(space) + "/" + key];
  }
 private:
  static std::map<std::string, uint8_t> &values() {
    static std::map<std::string, uint8_t> store;
    return store;
  }
  static std::map<std::string, size_t> &writeCounts() {
    static std::map<std::string, size_t> counts;
    return counts;
  }
  std::string space;
  bool readonly = false;
};
