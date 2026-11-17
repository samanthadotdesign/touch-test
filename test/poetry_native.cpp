// Native tests of production layout, projection, rendering and touch state flow.
// See test/README for the compiler command. Hardware I/O alone is stubbed.
#include <assert.h>
#include <math.h>
#include <string>
#include "app.h"
#include "letters.h"
#include "display.h"
#include "poetry_layout.h"
#include "poetry_motion.h"
#include "atmosphere.h"
#include <Preferences.h>

uint32_t nowMs = 0;
bool fingerDown = false;
int16_t touchX = 120, touchY = 160;
bool microphoneAvailable = true, microphoneEnabled = false;
float microphoneLevel = 0;
long forcedRandom = -1;
TestSerial Serial;
Arduino_GFX testGfx;
Arduino_GFX *gfx = &testGfx;
uint32_t millis() { return nowMs; }
long random(long upper) { return forcedRandom >= 0 ? forcedRandom % upper : rand() % upper; }
void randomSeed(unsigned long seed) { srand(seed); }
void displayBegin() {}
void displayFlush() {}
uint16_t *displayFramebuffer() { testGfx.glyphWrites.clear(); return nullptr; }
void displayCenter(const char *, int16_t, uint8_t, uint16_t, uint16_t) {}
void touchBegin() {}
bool touchRead(int16_t &x, int16_t &y) { x = touchX; y = touchY; return fingerDown; }
bool touchInRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
bool audioInputBegin() { microphoneEnabled = false; return microphoneAvailable; }
bool audioInputSetEnabled(bool enabled) {
  microphoneEnabled = enabled && microphoneAvailable;
  return !enabled || microphoneAvailable;
}
bool audioInputAvailable() { return microphoneAvailable; }
bool audioInputEnabled() { return microphoneEnabled; }
float audioInputLevel() { return microphoneEnabled ? microphoneLevel : 0; }
void audioInputUpdate() {}
void advance(int ms) {
  for (int t = 0; t < ms; t += 20) { nowMs += 20; appUpdate(); }
}
void tap() { fingerDown = true; advance(20); fingerDown = false; advance(20); }
void tapAt(int16_t x, int16_t y) {
  touchX = x; touchY = y; tap(); touchX = 120; touchY = 160;
}

void assertFooterRow(const char *text, int y, float fade = 1) {
  size_t count = 0;
  const int start = (240 - (int)strlen(text) * 6) / 2;
  for (const auto &glyph : testGfx.glyphWrites) {
    if (glyph.y != y) continue;
    assert(count < strlen(text));
    assert(glyph.ch == text[count] && glyph.x == start + (int)count * 6);
    assert(glyph.color == atmosphereInk(.4f * fade, 120, y));
    count++;
  }
  assert(count == strlen(text));
}
void assertBodyClearsFooter(const PoetryLayout &layout) {
  assert(layout.authorY == 295);
  for (int i = 0; i < layout.count; i++) {
    assert(layout.words[i].y + 160 - 4 >= 4 - .001f);
    assert(layout.words[i].y + 160 + 4 <= layout.footer.top - 16 + .001f);
  }
}
void footerLayoutAndRendering() {
  const char *body = "A small light\nleans into dusk.\nThe room remembers\nhow to be still.";
  const std::string longTitle = std::string(20, 'a') + " " + std::string(20, 'b') + " "
      + std::string(18, 'c');
  const std::string longAuthor(35, 'd');
  PoetryLayout layout, untitled;
  assert(poetryLayout(body, layout, 0, longTitle.c_str()));
  assert(layout.footer.count == 3 && layout.footer.top == 253);
  assert(strcmp(layout.footer.lines[0], std::string(20, 'a').c_str()) == 0);
  assert(strcmp(layout.footer.lines[1], std::string(20, 'b').c_str()) == 0);
  assert(strcmp(layout.footer.lines[2], std::string(18, 'c').c_str()) == 0);
  assertBodyClearsFooter(layout);
  assert(poetryLayout(body, layout, 0, std::string(60, 'w').c_str()));
  assert(layout.footer.count == 2 && layout.footer.top == 267);
  assert(strlen(layout.footer.lines[0]) == 36 && strlen(layout.footer.lines[1]) == 24);
  assert(!poetryLayout(body, layout, 0, std::string(61, 'w').c_str()));
  assert(poetryLayout(body, layout, 0, "  \t\n "));
  assert(layout.footer.count == 0 && layout.footer.top == 295);

  MotionPreset whole = MOTIONS[0]; whole.unit = 1;
  for (uint8_t choice = 0; choice <= 12; choice++) {
    if (choice == 8) continue;
    assert(poetryLayout(body, untitled, choice));
    assert(poetryLayout(body, layout, choice, longTitle.c_str()));
    assert(layout.count == untitled.count);
    assertBodyClearsFooter(layout);
    for (int i = 0; i < layout.count; i++) {
      assert(layout.words[i].offset == untitled.words[i].offset);
      assert(layout.words[i].length == untitled.words[i].length);
      assert(layout.words[i].x == untitled.words[i].x);
    }
    Poem poem = {"footer", "Book title", body, longAuthor.c_str(), choice};
    assert(lettersLoadPoem(poem, whole));
    lettersAssemble(); lettersUpdate(.55f); lettersDraw();
    assertFooterRow("Book title", 281);
    assertFooterRow(longAuthor.c_str(), 295);
    assert(testGfx.textSize == 1);
  }

  std::string grid = "a";
  for (int i = 1; i < 36; i++) grid += " a";
  assert(poetryLayout(grid.c_str(), untitled, 3));
  assert(poetryLayout(grid.c_str(), layout, 3, longTitle.c_str()));
  assert(layout.count == 36);
  assertBodyClearsFooter(layout);
  float first = 1000, last = -1000;
  for (int i = 0; i < layout.count; i++) {
    assert(layout.words[i].x == untitled.words[i].x);
    first = fminf(first, layout.words[i].y);
    last = fmaxf(last, layout.words[i].y);
  }
  assert(fabsf(last - first - 225) < .001f); // taller spatial forms tighten only their centres

  // Keep every verse row, including blank stanzas, with the longest footer.
  std::string verse = "word";
  for (int i = 1; i < 22; i++) verse += "\nword";
  for (uint8_t choice : {0, 1, 2, 9, 10}) {
    assert(poetryLayout(verse.c_str(), layout, choice, longTitle.c_str()));
    assert(layout.count == 22 && layout.words[21].line == 21);
    assertBodyClearsFooter(layout);
    const float pitch = layout.words[1].y - layout.words[0].y;
    assert(pitch >= 10 && pitch <= 12);
    for (int i = 1; i < layout.count; i++)
      assert(fabsf(layout.words[i].y - layout.words[i - 1].y - pitch) < .001f);
  }
  assert(!poetryLayout((verse + "\nword").c_str(), layout, 0, longTitle.c_str()));
  Poem longPoem = {"long-footer", longTitle.c_str(), verse.c_str(), longAuthor.c_str(), 0};
  assert(lettersLoadPoem(longPoem, whole));
  assert(lettersInfo().realParticles == 22);
  lettersAssemble(); lettersUpdate(.55f); lettersDraw();
  PoetryFooter footer;
  assert(poetryFooter(longTitle.c_str(), footer));
  for (int row = 0; row < 3; row++) assertFooterRow(footer.lines[row], 253 + row * 14);
  assertFooterRow(longAuthor.c_str(), 295);
  assert(testGfx.glyphWrites.size() == 22 * 4 + 58 + 35);

  // Both metadata rows share their fixed positions and fade through each theme.
  for (int theme = ATM_NIGHT; theme <= ATM_COLOUR; theme++) {
    atmosphereSetTheme((AtmosphereTheme)theme);
    Poem poem = {"fade-footer", "Book title", body, "Author", 0};
    assert(lettersLoadPoem(poem, whole)); lettersDraw();
    assert(testGfx.glyphWrites.empty());
    lettersAssemble(); lettersUpdate(.275f); lettersDraw();
    assertFooterRow("Book title", 281, .5f); assertFooterRow("Author", 295, .5f);
    lettersUpdate(.275f); lettersDraw();
    assertFooterRow("Book title", 281); assertFooterRow("Author", 295);
    assert(lettersPrepareNext(poem, whole)); lettersDissolve();
    lettersUpdate(.325f); lettersDraw();
    assertFooterRow("Book title", 281, .5f); assertFooterRow("Author", 295, .5f);
    lettersUpdate(.325f); lettersDraw();
    assertFooterRow("Book title", 281, 0); assertFooterRow("Author", 295, 0);
    poem.title = "";
    assert(lettersLoadPoem(poem, whole)); lettersAssemble(); lettersUpdate(.55f); lettersDraw();
    assertFooterRow("", 281); assertFooterRow("Author", 295);
  }
  atmosphereSetTheme(ATM_COLOUR);
}

const Poem &poemNamed(const char *id) {
  for (const auto &poem : POEMS) if (strcmp(poem.id, id) == 0) return poem;
  assert(false && "Missing authored reference fixture");
  return POEMS[0];
}
void assertAllWords(const char *text, const PoetryLayout &layout) {
  int index = 0;
  for (size_t p = 0; text[p];) {
    if (text[p] == ' ' || text[p] == '\n') { p++; continue; }
    const size_t start = p;
    while (text[p] && text[p] != ' ' && text[p] != '\n') p++;
    assert(index < layout.count);
    assert(layout.words[index].offset == start && layout.words[index].length == p - start);
    index++;
  }
  assert(index == layout.count);
}
void assertReferenceGroups(const Poem &poem, int id, const std::vector<int> &groups) {
  const PoetryReadingTemplate *reference = nullptr;
  for (const auto &entry : POETRY_READING_TEMPLATES) if (entry.id == id) reference = &entry;
  assert(reference && reference->count == groups.size());
  PoetryLayout layout;
  assert(poetryLayout(poem.text, layout, id, poem.title));
  assertAllWords(poem.text, layout);
  assertBodyClearsFooter(layout);
  int start = 0;
  for (size_t box = 0; box < groups.size(); box++) {
    const auto &anchor = reference->boxes[box];
    const PoetryWord &word = layout.words[start];
    assert(fabsf(word.x + 120 - (anchor.x + word.length * 3)) < .001f);
    assert(fabsf(word.y + 160 - (anchor.y + 4)) < .001f);
    for (int index = start; index < start + groups[box]; index++) {
      const PoetryWord &placed = layout.words[index];
      assert(placed.x + 120 - placed.length * 3 >= anchor.x - .001f);
      assert(placed.x + 120 + placed.length * 3 <= anchor.x + anchor.width + .001f);
      const float row = (placed.y + 160 - 4 - anchor.y) / 14;
      assert(row >= 0 && row < anchor.rows && fabsf(row - roundf(row)) < .001f);
    }
    start += groups[box];
  }
  assert(start == layout.count);
}
void referenceReadingLayouts() {
  assertReferenceGroups(poemNamed("i-went-to-the-woods"), 13, {6, 5, 8, 12, 2, 5, 6});
  assertReferenceGroups(poemNamed("and-once-the-storm-is-over"), 14, {6, 8, 5, 5, 6, 5, 7, 9, 6});
  const Poem &rubin = poemNamed("make-sure-the-things-we-do");
  assertReferenceGroups(rubin, 15, {2, 4, 5, 5});
  assertReferenceGroups(rubin, 16, {6, 5, 5});

  PoetryLayout layout;
  // A failed preferred split can still use every box without dropping words.
  assert(poetryLayout("a a a a a a", layout));
  const PoetryReadingTemplate fallback = {13, 3, {
    {12, 18, 6, 1, 1}, {12, 50, 18, 1, 1}, {12, 82, 30, 1, 1}
  }};
  PoetryReferencePartition partition("a a a a a a", layout, fallback);
  assert(partition.apply());
  assert(layout.words[0].y == 18 + 4 - 160);
  assert(layout.words[1].y == 50 + 4 - 160 && layout.words[2].y == 50 + 4 - 160);
  assert(layout.words[3].y == 82 + 4 - 160 && layout.words[5].y == 82 + 4 - 160);

  // Source blank lines count within a box; old 168px wrap boundaries do not.
  assert(poetryLayout("one\n\ntwo", layout));
  PoetryReadingBox box = {12, 18, 108, 2, 2};
  assert(!poetryReferenceSegment("one\n\ntwo", layout, box, 0, 2));
  box.rows = 3;
  assert(poetryReferenceSegment("one\n\ntwo", layout, box, 0, 2, true));
  assert(layout.words[1].y == 18 + 28 + 4 - 160);
  const char *wrapped = "word word word word word word word word word";
  assert(poetryLayout(wrapped, layout));
  assert(poetryReferenceSegment(wrapped, layout, box, 0, 9, true));
  assert(layout.words[8].y == 18 + 28 + 4 - 160);

  const std::string longTitle = std::string(20, 'a') + " " + std::string(20, 'b') + " "
      + std::string(18, 'c');
  int referencesSeen[4] = {};
  for (const auto &poem : POEMS) {
    for (uint8_t choice = 0; choice <= POETRY_MAX_LAYOUT; choice++) {
      if (choice == 8 || !poetryLayout(poem.text, layout, choice, longTitle.c_str())) continue;
      assertAllWords(poem.text, layout);
      assertBodyClearsFooter(layout);
      if (choice < 13) continue;
      referencesSeen[choice - 13]++;
      for (int a = 0; a < layout.count; a++) {
        const PoetryWord &word = layout.words[a];
        assert(word.x + 120 - word.length * 3 >= 6);
        assert(word.x + 120 + word.length * 3 <= 234);
        for (int b = a + 1; b < layout.count; b++) {
          const PoetryWord &other = layout.words[b];
          const bool xOverlap = fabsf(word.x - other.x) < (word.length + other.length) * 3 - .001f;
          const bool yOverlap = fabsf(word.y - other.y) < 8 - .001f;
          assert(!xOverlap || !yOverlap);
        }
      }
    }
  }
  for (int seen : referencesSeen) assert(seen > 0);
  for (uint8_t choice = 13; choice <= POETRY_MAX_LAYOUT; choice++) {
    assert(!poetryLayout("one two", layout, choice)); // all reference boxes need words
    assert(!poetryLayout(std::string(28, 'x').c_str(), layout, choice));
  }
  MotionPreset whole = MOTIONS[0]; whole.unit = 1;
  Poem selected = rubin; selected.layout = 15;
  assert(lettersLoadPoem(selected, whole));
  const LettersInfo initial = lettersInfo();
  lettersAssemble(); lettersUpdate(.55f); lettersDraw();
  assert(lettersInfo().resolvedLayout == 15 && lettersInfo().realParticles == 16);
  assert(lettersInfo().seed == initial.seed); // reading is static; only the motion field was animated
  assertFooterRow(rubin.by, 295);
  const auto reading = testGfx.glyphWrites;
  lettersUpdate(3); lettersDraw();
  assert(reading.size() == testGfx.glyphWrites.size());
  for (size_t i = 0; i < reading.size(); i++) {
    assert(reading[i].ch == testGfx.glyphWrites[i].ch);
    assert(reading[i].x == testGfx.glyphWrites[i].x && reading[i].y == testGfx.glyphWrites[i].y);
  }

  // Exercise every weighted random bucket with a fixed previous composition:
  // each eligible reference style gets three chances and each older one one.
  std::vector<uint8_t> weighted;
  for (uint8_t choice = 1; choice <= POETRY_MAX_LAYOUT; choice++) {
    if (choice == 8 || !poetryLayout(rubin.text, layout, choice, rubin.title)) continue;
    for (int copy = 0; copy < (choice >= 13 ? 3 : 1); copy++) weighted.push_back(choice);
  }
  for (size_t bucket = 0; bucket < weighted.size(); bucket++) {
    selected.layout = 0; forcedRandom = -1;
    assert(lettersLoadPoem(selected, whole));
    selected.layout = 8; forcedRandom = bucket;
    assert(lettersLoadPoem(selected, whole));
    assert(lettersInfo().resolvedLayout == weighted[bucket]);
  }
  forcedRandom = -1;
}

int main() {
  Preferences::clearAll();
  atmosphereBegin();
  PoetryLayout layout;
  assert(poetryLayout("two words\none", layout));
  assert(layout.size == 1 && layout.count == 3);
  assert(fabsf(layout.words[0].x + 18) < .01f);
  assert(fabsf(layout.words[1].x - 12) < .01f);
  assert(fabsf(layout.words[2].x) < .01f);
  assert(layout.words[2].y > layout.words[0].y);
  assert(poetryLayout("one\n\ntwo", layout));
  assert(layout.words[1].line == 2);
  assert(poetryLayout("1234567890123456789012345678", layout));
  assert(layout.size == 1); // all type stays small, no truncation
  assert(!poetryLayout(std::string(513, 'a').c_str(), layout));
  assert(!poetryLayout(std::string(29, 'a').c_str(), layout));
  assert(!poetryLayout("", layout));
  assert(!poetryLayout("bad\ttext", layout));
  std::string tooMany;
  for (int i = 0; i < 97; i++) tooMany += "a ";
  assert(!poetryLayout(tooMany.c_str(), layout));
  for (const auto &p : POEMS) {
    bool eligible = false;
    for (uint8_t choice = 0; choice <= POETRY_MAX_LAYOUT; choice++)
      if (choice != 8) eligible |= poetryLayout(p.text, layout, choice, p.title);
    assert(eligible);
  }
  assert(poetryLayout("longer line\na", layout, 1));
  assert(fabsf((layout.words[0].x - 18) - (layout.words[2].x - 3)) < .001f);
  assert(poetryLayout("one\ntwo", layout, 2));
  assert(fabsf(layout.words[1].x - layout.words[0].x - 8) < .001f);
  assert(!poetryLayout("a\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na", layout, 2));
  std::string longVerse;
  for (int i = 0; i < 20; i++) {
    if (i) longVerse += "\n";
    if (i != 4 && i != 9 && i != 15) longVerse += "line";
  }
  for (uint8_t choice : {0, 1, 10}) {
    assert(poetryLayout(longVerse.c_str(), layout, choice));
    assert(layout.words[layout.count - 1].line == 19);
    assert(fabsf(layout.words[layout.count - 1].y - layout.words[0].y - 247) < .001f);
    assert(layout.words[0].y - 4 >= -144);
    assert(layout.authorY + 8 <= 304);
    assert(layout.authorY - (layout.words[layout.count - 1].y + 164) >= 20);
  }
  std::string denseVerse = "word";
  for (int i = 1; i < 22; i++) denseVerse += "\nword";
  assert(poetryLayout(denseVerse.c_str(), layout));
  denseVerse += "\nword";
  assert(!poetryLayout(denseVerse.c_str(), layout));
  // Authored fixture keeps layout/glyph tests independent of the user's corpus.
  const Poem fixedPoem = {"fixture", "Fixture",
    "A small light\nleans into dusk.\nThe room remembers\nhow to be still.", "Test", 0};
  for (const auto &m : MOTIONS) {
    MotionPreset grouped = m;
    grouped.unit = 1;
    assert(lettersLoadPoem(fixedPoem, grouped));
    const int words = lettersInfo().realParticles;
    lettersAssemble(); lettersUpdate(.55f); lettersDraw();
    const auto wordGlyphs = testGfx.glyphWrites;
    grouped.unit = 0;
    assert(lettersLoadPoem(fixedPoem, grouped));
    assert(lettersInfo().realParticles > words);
    int chars = 0;
    for (const char *c = fixedPoem.text; *c; c++) if (*c != ' ' && *c != '\n') chars++;
    assert(lettersInfo().realParticles == chars && lettersCount() >= 180);
    assert(lettersInfo().sourceBoundsValid && lettersInfo().echoPhrases == 0);
    lettersAssemble(); lettersUpdate(.55f); lettersDraw();
    assert(testGfx.glyphWrites.size() == wordGlyphs.size());
    for (size_t i = 0; i < wordGlyphs.size(); i++) {
      assert(testGfx.glyphWrites[i].ch == wordGlyphs[i].ch);
      assert(testGfx.glyphWrites[i].x == wordGlyphs[i].x);
      assert(testGfx.glyphWrites[i].y == wordGlyphs[i].y);
    } // letter particles reconstruct exactly the original word layout
    grouped.unit = 2;
    assert(lettersLoadPoem(fixedPoem, grouped));
    const LettersInfo mixed = lettersInfo();
    assert(mixed.realParticles > words && mixed.realParticles < chars);
    assert(mixed.particles == 180 && mixed.particles > mixed.realParticles);
    lettersAssemble(); lettersUpdate(.55f); lettersDraw();
    assert(testGfx.glyphWrites.size() == wordGlyphs.size()); // no echo ghosts in reading
    for (size_t i = 0; i < wordGlyphs.size(); i++) {
      assert(testGfx.glyphWrites[i].ch == wordGlyphs[i].ch);
      assert(testGfx.glyphWrites[i].x == wordGlyphs[i].x);
      assert(testGfx.glyphWrites[i].y == wordGlyphs[i].y);
    }
  }

  for (uint8_t choice = 0; choice <= 12; choice++) {
    if (choice == 8) continue;
    assert(poetryLayout(fixedPoem.text, layout, choice));
    assert(layout.authorY == 295);
    for (int i = 0; i < layout.count; i++) {
      assert(isfinite(layout.words[i].x) && isfinite(layout.words[i].y));
      assert(layout.words[i].x - layout.words[i].length * 3 >= -120);
      assert(layout.words[i].x + layout.words[i].length * 3 <= 120);
      assert(layout.words[i].y >= -156 && layout.words[i].y <= 124);
    }
  }
  footerLayoutAndRendering();
  referenceReadingLayouts();
  Poem randomized = fixedPoem; randomized.layout = 8;
  int previousResolved = -1;
  assert(lettersLoadPoem(randomized, MOTIONS[0]));
  for (int i = 0; i < 16; i++) {
    const int resolved = lettersInfo().resolvedLayout;
    assert(resolved >= 0 && resolved <= POETRY_MAX_LAYOUT && resolved != 8 && resolved != previousResolved);
    lettersUpdate(.2f);
    assert(lettersInfo().resolvedLayout == resolved); // stable within scene
    previousResolved = resolved;
    assert(lettersLoadPoem(randomized, MOTIONS[0]));
  }
  assert(poetryRandom(0, 0, 0) == 16583815.f / 16777216.f);
  assert(poetryRandom(42, 3, 20) == 4683051.f / 16777216.f);
  assert(poetryRandom(0xdeadbeef, 119, 40) == 16173732.f / 16777216.f);
  for (const auto &m : MOTIONS) for (uint32_t seed : {0u, 1u, 42u, 0xdeadbeefu})
    for (float seconds : {0.f, 5.f, 20.f}) {
    bool cells[48] = {};
    int occupied = 0, inCore = 0, outliers = 0, onStream = 0, onPatch = 0;
    const float t = seconds * fmaxf(POETRY_MIN_SPEED, m.speed) * POETRY_MOTION_TEMPO;
    const float globalP = poetryRandom(seed, 0, 60) * 6.28318530718f;
    const float coreX = (poetryRandom(seed, 0, 61) * 2 - 1) * 62 + 18 * sinf(t * .07f + globalP);
    const float coreY = (poetryRandom(seed, 0, 62) * 2 - 1) * 94 + 14 * cosf(t * .05f + globalP);
    for (int i = 0; i < 180; i++) {
      const WordPose field = poetryFieldMotion(m, i, 180, seconds, i % 4 == 0 ? 24 : 6, seed);
      assert(isfinite(field.x) && isfinite(field.y) && isfinite(field.z));
      assert(field.scale >= .66f && field.scale <= 1.1f);
      const float q = m.perspective / (m.perspective - field.z);
      const float sx = field.x * q, sy = field.y * q;
      assert(sx >= -116 && sx <= 116 && sy >= -154 && sy <= 154);
      const int col = (int)((sx + 120) / 40), row = (int)((sy + 160) / 40);
      const int cell = row * 6 + col;
      if (!cells[cell]) { cells[cell] = true; occupied++; }
      if (fabsf(sx - coreX) < 50 && fabsf(sy - coreY) < 50) inCore++;
      if (hypotf(sx - coreX, sy - coreY) > 90) outliers++;
      const float u = sy / 284 + .5f;
      const float streamX = 45 * sinf(u * 3.14159265359f * 2.7f - .7f + t * .05f)
                            + (u - .5f) * 85;
      if (fabsf(sx - streamX) < 35) onStream++;
      float nearestBand = 1000;
      for (int g = 0; g < 6; g++) nearestBand = fminf(nearestBand, fabsf(sy - (-118 + g * 46)));
      if (nearestBand <= 18) onPatch++;
    }
    assert(occupied < 42); // coherent formations keep visible negative space
    if (m.formation == 0) assert(inCore > 95 && outliers >= 5);
    if (m.formation == 1) assert(onStream > 100);
    if (m.formation == 2) assert(onPatch > 140);
  }
  for (const auto &m : MOTIONS) {
    int moving = 0;
    float slowest = 1000, fastest = 0;
    for (int i = 0; i < 120; i++) {
      const WordPose a = poetryFieldMotion(m, i, 120, 3, 6, 42);
      const WordPose again = poetryFieldMotion(m, i, 120, 3, 6, 42);
      const WordPose b = poetryFieldMotion(m, i, 120, 3.05f, 6, 42);
      const WordPose anotherSeed = poetryFieldMotion(m, i, 120, 3, 6, 43);
      assert(a.x == again.x && a.y == again.y && a.z == again.z);
      assert(fabsf(a.x - anotherSeed.x) + fabsf(a.y - anotherSeed.y) > .01f);
      const float aq = m.perspective / (m.perspective - a.z);
      const float bq = m.perspective / (m.perspective - b.z);
      const float dx = b.x * bq - a.x * aq, dy = b.y * bq - a.y * aq;
      const float distance = hypotf(dx, dy);
      slowest = fminf(slowest, distance); fastest = fmaxf(fastest, distance);
      if (distance > .001f) moving++;
    }
    assert(moving > 90 && fastest - slowest > .01f);
  }
  // Continuous bounded orientation also holds as Tumble's raw angles cross PI.
  MotionPreset tumble = MOTIONS[0]; tumble.kind = 5;
  for (int particle = 0; particle < 24; particle++) {
    WordPose previous = poetryFieldMotion(tumble, particle, 180, 0, 6, 42);
    for (int tick = 1; tick <= 400; tick++) {
      const WordPose pose = poetryFieldMotion(tumble, particle, 180, tick * .05f, 6, 42);
      assert(fabsf(pose.rx - previous.rx) < .05f);
      assert(fabsf(pose.ry - previous.ry) < .05f);
      assert(fabsf(pose.rz - previous.rz) < .05f);
      previous = pose;
    }
  }
  MotionPreset mixedMotion = MOTIONS[0]; mixedMotion.unit = 2;
  uint32_t previousSeed = 0;
  int firstRealCount = -1;
  bool groupingChanged = false;
  for (int repeat = 0; repeat < 16; repeat++) {
    assert(lettersLoadPoem(fixedPoem, mixedMotion));
    const LettersInfo initial = lettersInfo();
    assert(initial.sourceBoundsValid && initial.echoPhrases > 0);
    assert(initial.minScale < .75f && initial.maxScale > 1.f && initial.maxScale <= 1.1f);
    assert(repeat == 0 || initial.seed != previousSeed);
    if (firstRealCount < 0) firstRealCount = initial.realParticles;
    if (initial.realParticles != firstRealCount) groupingChanged = true;
    previousSeed = initial.seed;
    lettersUpdate(.2f);
    assert(lettersInfo().seed == initial.seed); // stable throughout motion
    lettersAssemble(); lettersUpdate(.2f);
    assert(lettersInfo().maxBlend - lettersInfo().minBlend > .05f);
    lettersUpdate(.35f);
    assert(lettersTransitionDone());
    assert(lettersInfo().minScale == 1 && lettersInfo().maxScale == 1);
    assert(lettersPrepareNext(fixedPoem, mixedMotion));
    const uint32_t nextSeed = lettersInfo().nextSeed;
    assert(nextSeed != initial.seed);
    lettersDissolve(); lettersUpdate(.24f);
    assert(lettersInfo().maxBlend - lettersInfo().minBlend > .05f);
    lettersUpdate(.41f);
    assert(lettersTransitionDone());
    lettersAdoptNext();
    assert(lettersInfo().seed == nextSeed);
  }
  assert(groupingChanged); // mixed grouping varies per scene, not every third word

  WordPose v;
  float x, y;
  assert(poetryProject(v, poetryBasis(v), 12, -16, 360, x, y));
  assert(fabsf(x - 132) < .001f && fabsf(y - 144) < .001f);
  v.scale = 1.5f;
  assert(poetryProject(v, poetryBasis(v), 12, -16, 360, x, y));
  assert(fabsf(x - 138) < .001f && fabsf(y - 136) < .001f);
  v.scale = 1;
  v.ry = 3.14159265f / 2;
  assert(poetryProject(v, poetryBasis(v), 12, 0, 360, x, y));
  assert(fabsf(x - 120) < .001f); // glyph plane becomes edge-on
  v = {}; v.z = 350;
  assert(!poetryProject(v, poetryBasis(v), 0, 0, 360, x, y));

  for (const auto &m : MOTIONS) {
    assert(lettersLoadPoem(POEMS[0], m));
    for (int frame = 0; frame < 120; frame++) {
      lettersUpdate(.033f);
      lettersDraw();
      WordPose a = poetryMotion(m, frame % 10, 10, frame * .033f);
      assert(isfinite(a.x) && isfinite(a.y) && isfinite(a.z));
    }
    lettersAssemble();
    for (int frame = 0; frame < 50; frame++) lettersUpdate(.033f);
    assert(lettersTransitionDone());
    lettersDraw();
    assert(lettersPrepareNext(POEMS[1], m));
    lettersDissolve();
    for (int frame = 0; frame < 20; frame++) {
      const int before = testGfx.visibleTriangles;
      lettersUpdate(.033f); lettersDraw();
      assert(testGfx.visibleTriangles > before); // overlapping scenes have no blank frame
    }
    const LettersInfo beforeSwap = lettersInfo();
    assert(beforeSwap.nextParticles > 0 && beforeSwap.nextTime > .65f);
    assert(lettersTransitionDone());
    lettersAdoptNext();
    assert(fabsf(lettersInfo().time - beforeSwap.nextTime) < .0001f);
    assert(strcmp(lettersInfo().poemId, POEMS[1].id) == 0);
  }
  assert(testGfx.triangles > 0 && testGfx.glyphs > 0);


  // A near-limit corpus stays bounded and never loses characters to uint8
  // particle indices. Eighteen 27-character lines contain 468 visible glyphs.
  std::string longText;
  for (int i = 0; i < 18; i++) {
    if (i) longText += "\n";
    longText += "aaaaaaaaaaaaa aaaaaaaaaaaaa";
  }
  assert(longText.size() <= 512);
  Poem longPoem = {"limit", "Limit", longText.c_str(), "Test", 0};
  MotionPreset dense = MOTIONS[0]; dense.unit = 0;
  assert(lettersLoadPoem(longPoem, dense));
  int expectedParticles = 0;
  for (char c : longText) if (c != ' ' && c != '\n') expectedParticles++;
  assert(lettersCount() == expectedParticles && lettersCount() > 255);
  assert(lettersPrepareNext(longPoem, dense));
  lettersAssemble(); lettersUpdate(.55f); lettersDissolve();
  for (int i = 0; i < 14; i++) { lettersUpdate(.05f); lettersDraw(); }
  assert(lettersTransitionDone());
  lettersAdoptNext();
  assert(lettersCount() == expectedParticles);

  // The deployed field shares the browser's outward fan and 20% larger floor.
  for (const auto &m : MOTIONS) for (uint32_t seed : {0u, 8u, 12u, 42u}) {
    for (int i = 0; i < 180; i++) {
      const float width = i % 4 == 0 ? 48.f : 6.f;
      const WordPose base = poetryFieldMotion(m, i, 180, 4, width, seed);
      const WordPose zero = poetryBreathMotion(m, base, i, width, seed, 0);
      assert(base.x == zero.x && base.y == zero.y && base.scale == zero.scale);
      assert(base.scale >= .66f && base.scale <= 1.1f);
      const float q = m.perspective / (m.perspective - base.z);
      float previousDistance = hypotf(base.x * q, base.y * q);
      const float limitX = fmaxf(6, 116 - fminf(104, width * base.scale * q * .55f));
      const float limitY = fmaxf(20, 154 - 4 * base.scale * q);
      for (float force : {.3f, .6f, 1.f}) {
        const WordPose pushed = poetryBreathMotion(m, base, i, width, seed, force);
        assert(isfinite(pushed.x) && isfinite(pushed.y));
        assert(pushed.z == base.z && pushed.scale == base.scale);
        assert(fabsf(pushed.x * q) <= limitX + .001f);
        assert(fabsf(pushed.y * q) <= limitY + .001f);
        const float distance = hypotf(pushed.x * q, pushed.y * q);
        assert(distance + .001f >= previousDistance);
        previousDistance = distance;
      }
    }
  }
  assert(lettersLoadPoem(fixedPoem, mixedMotion));
  lettersSetResponse(9, 1);
  lettersUpdate(.1f);
  assert(fabsf(lettersInfo().time - .9f) < .001f);
  lettersAssemble(); lettersUpdate(.54f);
  assert(!lettersTransitionDone());
  lettersUpdate(.01f);
  assert(lettersTransitionDone());
  assert(lettersInfo().minScale == 1 && lettersInfo().maxScale == 1);
  assert(lettersPrepareNext(fixedPoem, mixedMotion));
  lettersDissolve(); lettersUpdate(.64f);
  assert(!lettersTransitionDone());
  lettersUpdate(.01f);
  assert(lettersTransitionDone());
  const float scaledIncoming = lettersInfo().nextTime;
  assert(fabsf(scaledIncoming - 5.85f) < .001f);
  lettersAdoptNext();
  assert(fabsf(lettersInfo().time - scaledIncoming) < .001f);
  lettersSetResponse(NAN, INFINITY); // invalid response cannot contaminate poses
  lettersUpdate(.1f);
  assert(fabsf(lettersInfo().time - scaledIncoming - .1f) < .001f);
  lettersSetResponse(1, 0);

  appBegin();
  assert(appState() == ST_MOVING);
  assert(atmosphereTheme() == ATM_COLOUR && atmosphereColourway() < ATM_COLOURWAY_COUNT);
  assert(Preferences::writesTo("poetry", "colourway") == 1); // missing key, first boot
  advance(10000);
  assert(appState() == ST_MOVING); // no automatic poem assembly
  fingerDown = true; advance(20);
  assert(appState() == ST_ASSEMBLING);
  advance(520);
  assert(appState() == ST_ASSEMBLING);
  advance(40);
  assert(appState() == ST_READING);
  advance(10000);
  assert(appState() == ST_READING); // held finger never advances twice
  fingerDown = false; advance(20);
  const std::string outgoingId = lettersInfo().poemId;
  tap();
  assert(appState() == ST_DISSOLVING);
  const LettersInfo overlap = lettersInfo();
  assert(overlap.nextPoemId && outgoingId != overlap.nextPoemId);
  assert(outgoingId == overlap.poemId); // next poem selected now, current retained
  const std::string incomingId = overlap.nextPoemId;
  advance(600);
  assert(appState() == ST_DISSOLVING); // .62s since press, before .65 boundary
  const float incomingTime = lettersInfo().nextTime;
  advance(40);
  assert(appState() == ST_MOVING);
  assert(incomingId == lettersInfo().poemId);
  assert(lettersInfo().time >= incomingTime); // motion clock survives adoption
  advance(300); tap(); advance(1800);
  assert(appState() == ST_READING);
  advance(10000);
  assert(appState() == ST_READING); // readable poem holds indefinitely

  // The corner is invisible during normal use and consumes its opening press.
  advance(300); tapAt(220, 300);
  assert(appMenuVisible() && appState() == ST_READING);
  advance(300); tapAt(120, 132);
  assert(atmosphereTheme() == ATM_NIGHT && appMenuVisible());
  const float originalThreshold = atmosphereThreshold();
  advance(300); tapAt(170, 273);
  assert(fabsf(atmosphereThreshold() - originalThreshold + .05f) < .001f);
  const float savedThreshold = atmosphereThreshold();
  advance(300); tapAt(120, 50); // outside sheet closes without changing poem
  assert(!appMenuVisible() && appState() == ST_READING);
  const int themeGlyphsBefore = testGfx.glyphs;
  advance(300); tapAt(220, 300);
  assert(appMenuVisible());
  advance(6100);
  assert(!appMenuVisible() && appState() == ST_READING);
  assert(testGfx.glyphs > themeGlyphsBefore);
  Preferences oldSettings;
  assert(oldSettings.begin("poetry", false));
  oldSettings.putUChar("theme", ATM_NIGHT); // legacy firmware may have saved this
  const uint8_t previousColourway = atmosphereColourway();
  const size_t colourwayWritesBeforeBoot = Preferences::writesTo("poetry", "colourway");
  appBegin(); // Threshold survives; every reboot starts a fresh colourful scene.
  assert(atmosphereTheme() == ATM_COLOUR);
  assert(atmosphereColourway() != previousColourway);
  assert(Preferences::writesTo("poetry", "colourway") == colourwayWritesBeforeBoot + 1);
  assert(fabsf(atmosphereThreshold() - savedThreshold) < .001f);
  assert(!microphoneEnabled && appState() == ST_MOVING && !appMenuVisible());

  // Enabling a quiet microphone only enables the response, never navigation.
  advance(300); tapAt(220, 300);
  advance(300); tapAt(60, 245);
  assert(microphoneEnabled && appState() == ST_MOVING);
  advance(300); tapAt(220, 100);
  assert(!appMenuVisible());
  advance(1200);
  assert(appState() == ST_MOVING);
  microphoneLevel = 1;
  for (int i = 0; i < 75 && appState() == ST_MOVING; i++) advance(20);
  assert(appState() == ST_ASSEMBLING);
  advance(520); assert(appState() == ST_ASSEMBLING);
  advance(60); assert(appState() == ST_READING);
  advance(4000); assert(appState() == ST_READING); // held breath triggers once
  assert(lettersInfo().minScale == 1 && lettersInfo().maxScale == 1);

  microphoneLevel = 0;
  advance(2200);
  advance(300); tapAt(220, 300);
  advance(300); tapAt(60, 245); // disable
  assert(!microphoneEnabled && appState() == ST_READING);
  advance(300); tapAt(60, 245); // re-enable in a quiet room
  assert(microphoneEnabled && appState() == ST_READING);
  advance(300); tapAt(220, 100);
  advance(1200); assert(appState() == ST_READING);
  const std::string breathOutgoing = lettersInfo().poemId;
  microphoneLevel = 1;
  for (int i = 0; i < 75 && appState() == ST_READING; i++) advance(20);
  assert(appState() == ST_DISSOLVING);
  advance(620); assert(appState() == ST_DISSOLVING);
  advance(60); assert(appState() == ST_MOVING);
  assert(breathOutgoing != lettersInfo().poemId);
  advance(2500); assert(appState() == ST_MOVING);

  // A touch and an already-latched breath on the final dissolve frame are
  // both consumed by that transition, never replayed into the incoming poem.
  microphoneLevel = 0;
  advance(2200);
  tap(); advance(600);
  assert(appState() == ST_READING);
  advance(300); tap(); advance(620);
  assert(appState() == ST_DISSOLVING); // .64 seconds, just before handoff
  atmosphereBegin(); // Prime a fresh detector without advancing the app clock.
  microphoneLevel = 1;
  for (int i = 0; i < 8; i++) atmosphereUpdate(.1f, microphoneLevel, true);
  assert(atmosphereLevel() > .99f);
  fingerDown = true; advance(20);
  assert(appState() == ST_MOVING);
  fingerDown = false; advance(20);
  assert(appState() == ST_MOVING); // a short-circuited pending blow would assemble
  advance(1000);
  assert(appState() == ST_MOVING && !atmosphereTakeBlow());

  // Hardware failure leaves tap controls and the menu usable.
  microphoneAvailable = false; microphoneLevel = 0;
  appBegin(); advance(300); tapAt(220, 300);
  advance(300); tapAt(60, 245);
  assert(!microphoneEnabled && appMenuVisible() && appState() == ST_MOVING);
  advance(6100); assert(!appMenuVisible());
  advance(300); tap(); assert(appState() == ST_ASSEMBLING);

  // A remembered colourway avoids adjacent repeats without persisting the
  // session's Night/Paper choice or enabling the mic on the next boot.
  microphoneAvailable = true;
  appBegin();
  uint8_t previousBootColourway = atmosphereColourway();
  const float persistentThreshold = atmosphereThreshold();
  for (int boot = 0; boot < 48; boot++) {
    advance(300); tapAt(220, 300);
    const AtmosphereTheme temporaryTheme = boot % 2 ? ATM_PAPER : ATM_NIGHT;
    const size_t oldThemeWrites = Preferences::writesTo("poetry", "theme");
    advance(300); tapAt(120, temporaryTheme == ATM_NIGHT ? 132 : 168);
    assert(atmosphereTheme() == temporaryTheme);
    assert(Preferences::writesTo("poetry", "theme") == oldThemeWrites);
    advance(300); tapAt(60, 245);
    assert(microphoneEnabled);
    const size_t writesBeforeBoot = Preferences::writesTo("poetry", "colourway");
    appBegin();
    const uint8_t selected = atmosphereColourway();
    assert(atmosphereTheme() == ATM_COLOUR && selected < ATM_COLOURWAY_COUNT);
    assert(selected != previousBootColourway);
    assert(oldSettings.getUChar("colourway", 255) == selected);
    assert(Preferences::writesTo("poetry", "colourway") == writesBeforeBoot + 1);
    assert(fabsf(atmosphereThreshold() - persistentThreshold) < .001f);
    assert(!microphoneEnabled && !appMenuVisible() && appState() == ST_MOVING);
    previousBootColourway = selected;
  }
  // Missing and malformed remembered indices start safely, then become valid.
  for (uint8_t invalid : {(uint8_t)ATM_COLOURWAY_COUNT, (uint8_t)255}) {
    oldSettings.putUChar("colourway", invalid);
    const size_t writesBeforeBoot = Preferences::writesTo("poetry", "colourway");
    appBegin();
    assert(atmosphereColourway() < ATM_COLOURWAY_COUNT && atmosphereTheme() == ATM_COLOUR);
    assert(Preferences::writesTo("poetry", "colourway") == writesBeforeBoot + 1);
  }
  oldSettings.end();
  puts("PASS: layouts, motion, 20% fragment floor, breath/tap controls, and colourful reboot variation");
}
