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
    const std::string path = space + "/" + key;
    if (failedWrites()[path]) { failedWrites()[path]--; return 0; }
    values()[path] = value;
    writeCounts()[path]++;
    return 1;
  }
  static void clearAll() { values().clear(); writeCounts().clear(); failedWrites().clear(); }
  static void failNextWrites(const char *space, const char *key, size_t count = 1) {
    failedWrites()[std::string(space) + "/" + key] = count;
  }
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
  static std::map<std::string, size_t> &failedWrites() {
    static std::map<std::string, size_t> failures;
    return failures;
  }
  std::string space;
  bool readonly = false;
};
