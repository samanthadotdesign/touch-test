#include "storage.h"
#include <LittleFS.h>

static int nextN = 1;

static int parseSessionN(const char *name) {
  const char *p = strstr(name, "session-");
  if (!p) return 0;
  return atoi(p + 8);
}

bool storageBegin() {
  if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
    Serial.println("LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists("/log")) LittleFS.mkdir("/log");

  Serial.println("signature log:");
  File root = LittleFS.open("/log");
  int maxN = 0;
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    bool any = false;
    while (f) {
      Serial.printf("  %s  %u bytes\n", f.name(), (unsigned)f.size());
      int n = parseSessionN(f.name());
      if (n > maxN) maxN = n;
      any = true;
      f = root.openNextFile();
    }
    if (!any) Serial.println("  (empty)");
  } else {
    Serial.println("  (no /log directory)");
  }
  nextN = maxN + 1;
  Serial.printf("next session number: %d\n", nextN);
  return true;
}

int storageNextNumber() { return nextN; }

bool storageSaveSession(int sessionN, int blocks, int minutes, const StrokePt *pts, int n) {
  if (!LittleFS.exists("/log")) LittleFS.mkdir("/log");

  const char *tmp = "/log/tmp.json";
  char path[40];
  snprintf(path, sizeof(path), "/log/session-%04d.json", sessionN);

  File f = LittleFS.open(tmp, "w");
  if (!f) {
    Serial.println("log write failed (tmp)");
    return false;
  }
  f.printf("{\"n\":%d,\"blocks\":%d,\"minutes\":%d,\"pts\":[", sessionN, blocks, minutes);
  for (int i = 0; i < n; i++) {
    if (i) f.print(',');
    f.printf("{\"x\":%d,\"y\":%d,\"s\":%u}", pts[i].x, pts[i].y, (unsigned)pts[i].start);
  }
  f.print("]}");
  f.close();

  if (LittleFS.exists(path)) LittleFS.remove(path);
  if (!LittleFS.rename(tmp, path)) {
    Serial.println("log rename failed");
    return false;
  }
  if (sessionN >= nextN) nextN = sessionN + 1;
  Serial.printf("saved %s (%d points)\n", path, n);
  return true;
}
