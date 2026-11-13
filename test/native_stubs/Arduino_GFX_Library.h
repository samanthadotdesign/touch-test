#pragma once
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <vector>
class Arduino_GFX {
 public:
  struct Glyph { uint8_t ch; int16_t x, y; };
  std::vector<Glyph> glyphWrites;
  int triangles = 0, glyphs = 0, visibleTriangles = 0;
  int16_t cursorX = 0, cursorY = 0;
  uint8_t textSize = 1;
  void fillScreen(uint16_t) { glyphWrites.clear(); }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    int16_t x2, int16_t y2, uint16_t color) {
    assert(x0 >= -1024 && x0 <= 1024 && y0 >= -1024 && y0 <= 1024);
    assert(x1 >= -1024 && x1 <= 1024 && y1 >= -1024 && y1 <= 1024);
    assert(x2 >= -1024 && x2 <= 1024 && y2 >= -1024 && y2 <= 1024);
    triangles++;
    if (color) visibleTriangles++;
  }
  void setTextSize(uint8_t n) { textSize = n; }
  void setTextColor(uint16_t) {}
  void setCursor(int16_t x, int16_t y) { cursorX = x; cursorY = y; }
  size_t write(uint8_t ch) {
    glyphs++;
    glyphWrites.push_back({ch, cursorX, cursorY});
    cursorX += 6 * textSize;
    return 1;
  }
};
