#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

constexpr int POETRY_MAX_WORDS = 96;
constexpr int POETRY_MAX_CHARS = 512;
struct PoetryWord {
  uint16_t offset;
  uint8_t length;
  uint8_t line;
  float x, y; // centre relative to the screen centre
};
struct PoetryLayout {
  PoetryWord words[POETRY_MAX_WORDS];
  int count = 0;
  uint8_t size = 1;
  float authorY = 0;
};

inline bool poetryLayout(const char *text, PoetryLayout &out, uint8_t arrangement = 0) {
  const uint8_t size = 1;
  float widths[POETRY_MAX_WORDS + 1] = {};
  int line = 0;
  out.count = 0;
  const int advance = 6 * size, lineHeight = 8 * size + 6;
  const size_t length = strlen(text);
  if (length > POETRY_MAX_CHARS) return false;
  for (size_t p = 0; p < length;) {
    if (text[p] == '\n') {
      if (++line > POETRY_MAX_WORDS) return false;
      p++;
      continue;
    }
    if (text[p] == ' ') { p++; continue; }
    const size_t start = p;
    while (p < length && text[p] != ' ' && text[p] != '\n') {
      if (text[p] < 32 || text[p] > 126) return false;
      p++;
    }
    const size_t len = p - start;
    const float wordWidth = len * advance;
    if (wordWidth > 168 || out.count >= POETRY_MAX_WORDS) return false;
    const float gap = widths[line] > 0 ? advance : 0;
    if (widths[line] + gap + wordWidth > 168) {
      if (++line > POETRY_MAX_WORDS) return false;
    }
    const float offset = widths[line] + (widths[line] > 0 ? advance : 0);
    out.words[out.count++] = {(uint16_t)start, (uint8_t)len, (uint8_t)line,
                             offset + wordWidth / 2, 0};
    widths[line] = offset + wordWidth;
  }
  if (!out.count) return false;
  const int lines = line + 1;
  if (arrangement >= 3 && arrangement != 9 && arrangement != 10) {
    if (arrangement == 8 || arrangement > 12) return false; // random is resolved by the scene engine
    const float pi = 3.14159265359f;
    out.size = 1;
    out.authorY = 296;
    float columnWidths[3] = {};
    int rows = 0, half = 0, firstLines = 0;
    int groupCounts[3] = {}, groupStarts[3] = {};
    float groupHeights[3] = {};
    float totalWidth = 0, spanY = 0, top = 0, h1 = 0;
    if (arrangement == 3) {
      rows = (out.count + 2) / 3;
      if (rows > 12) return false;
      for (int i = 0; i < out.count; i++)
        columnWidths[i % 3] = fmaxf(columnWidths[i % 3], out.words[i].length * 6);
      totalWidth = columnWidths[0] + columnWidths[1] + columnWidths[2] + 36;
      if (totalWidth > 216) return false;
      spanY = rows > 1 ? fminf(232, (rows - 1) * 50) : 0;
      top = (288 - spanY) / 2;
    } else if (arrangement == 4) {
      if (out.count > 20) return false;
    } else if (arrangement == 5 || arrangement == 11) {
      if (lines < 2) return false;
      firstLines = (lines + 1) / 2;
      h1 = firstLines * 14 - 6;
      const float h2 = (lines - firstLines) * 14 - 6;
      const float height = h1 + 48 + h2;
      if (height > 248) return false;
      top = (280 - height) / 2 + 8;
    } else if (arrangement == 6) {
      if (lines > 7) return false;
      const float height = (lines - 1) * 32 + 22;
      top = (280 - height) / 2 + 8;
    } else if (arrangement == 7) {
      half = (out.count + 1) / 2;
      if (half > 17 || out.count - half < 1) return false;
      for (int i = 0; i < out.count; i++) {
        const int col = i < half ? 0 : 1;
        columnWidths[col] = fmaxf(columnWidths[col], out.words[i].length * 6);
      }
      totalWidth = columnWidths[0] + 24 + columnWidths[1];
      if (totalWidth > 216) return false;
      top = (280 - ((half - 1) * 14 + 8)) / 2 + 8;
    } else { // Three stanza islands, preserving original line order.
      if (lines < 3) return false;
      for (int i = 0; i < lines; i++) {
        const int group = i * 3 / lines;
        if (!groupCounts[group]) groupStarts[group] = i;
        groupCounts[group]++;
      }
      float height = 64;
      for (int g = 0; g < 3; g++) {
        groupHeights[g] = groupCounts[g] * 14 - 6;
        height += groupHeights[g];
      }
      if (height > 248) return false;
      top = (280 - height) / 2 + 8;
    }
    for (int i = 0; i < out.count; i++) {
      PoetryWord &word = out.words[i];
      const float width = word.length * 6;
      float x = 0, y = 0;
      if (arrangement == 3) {
        const int col = i % 3, row = i / 3;
        x = (240 - totalWidth) / 2 + columnWidths[col] / 2;
        for (int c = 0; c < col; c++) x += columnWidths[c] + 18;
        x += sinf(row * 1.7f + col) * 4;
        y = top + (rows > 1 ? row * spanY / (rows - 1) : 0) + (col == 1 ? 8 : 0);
      } else if (arrangement == 4) {
        const float u = out.count > 1 ? (float)i / (out.count - 1) : .5f;
        x = fmaxf(12 + width / 2, fminf(228 - width / 2,
              120 + (u - .5f) * 110 + sinf(u * pi * 3) * 18));
        y = 40 + u * 224;
      } else if (arrangement == 5 || arrangement == 11) {
        const bool upper = word.line < firstLines;
        const bool leftAligned = arrangement == 5 ? upper : !upper;
        x = (leftAligned ? 18 : 222 - widths[word.line]) + word.x;
        y = upper ? top + word.line * 14 + 4
                  : top + h1 + 48 + (word.line - firstLines) * 14 + 4;
      } else if (arrangement == 6) {
        x = 120 + sinf(word.line * .9f) * 18 - widths[word.line] / 2 + word.x;
        y = top + word.line * 32 + 11
            + sinf((word.x / fmaxf(widths[word.line], 1)) * 2 * pi + word.line * .6f) * 7;
      } else if (arrangement == 7) {
        x = (240 - totalWidth) / 2 + (i < half ? columnWidths[0] / 2
                   : columnWidths[0] + 24 + columnWidths[1] / 2);
        y = top + (i % half) * 14 + 4;
      } else {
        const int group = word.line * 3 / lines;
        x = (group == 0 ? 18 : group == 1 ? 120 - widths[word.line] / 2
                                                      : 222 - widths[word.line]) + word.x;
        y = top + (word.line - groupStarts[group]) * 14 + 4;
        for (int g = 0; g < group; g++) y += groupHeights[g] + 32;
      }
      word.x = x - 120;
      word.y = y - 160;
    }
    return true;
  }
  const float bodyHeight = lines * lineHeight - 6;
  const float totalHeight = bodyHeight + 20 + 8;
  if (totalHeight > 288) return false;
  const auto indent = [lines, arrangement](int i) -> float {
    if (arrangement == 2) return i * 8.f;
    if (arrangement == 9) return (lines - 1 - i) * 8.f;
    if (arrangement == 10) return (i % 4 <= 2 ? i % 4 : 4 - i % 4) * 12.f;
    return 0;
  };
  float blockWidth = 0;
  for (int i = 0; i <= line; i++) {
    const float stepped = indent(i);
    const float right = stepped + widths[i];
    if (right > blockWidth) blockWidth = right;
  }
  if (blockWidth > 216) return false;
  const float top = (320 - totalHeight) / 2;
  out.size = size;
  out.authorY = top + bodyHeight + 20;
  for (int i = 0; i < out.count; i++) {
    PoetryWord &w = out.words[i];
    if (arrangement == 0) w.x -= widths[w.line] / 2;
    else w.x += indent(w.line) - blockWidth / 2;
    w.y = top + w.line * lineHeight + 4 * size - 160;
  }
  return true;
}
