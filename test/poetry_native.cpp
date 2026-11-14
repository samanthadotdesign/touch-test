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

uint32_t nowMs = 0;
bool fingerDown = false;
TestSerial Serial;
Arduino_GFX testGfx;
Arduino_GFX *gfx = &testGfx;
uint32_t millis() { return nowMs; }
long random(long upper) { return rand() % upper; }
void randomSeed(unsigned long seed) { srand(seed); }
void displayBegin() {}
void displayFlush() {}
void displayCenter(const char *, int16_t, uint8_t, uint16_t, uint16_t) {}
void touchBegin() {}
bool touchRead(int16_t &x, int16_t &y) { x = 120; y = 160; return fingerDown; }
void advance(int ms) {
  for (int t = 0; t < ms; t += 20) { nowMs += 20; appUpdate(); }
}
void tap() { fingerDown = true; advance(20); fingerDown = false; advance(20); }

int main() {
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
    for (uint8_t choice = 0; choice <= 12; choice++)
      if (choice != 8) eligible |= poetryLayout(p.text, layout, choice);
    assert(eligible);
  }
  assert(poetryLayout("longer line\na", layout, 1));
  assert(fabsf((layout.words[0].x - 18) - (layout.words[2].x - 3)) < .001f);
  assert(poetryLayout("one\ntwo", layout, 2));
  assert(fabsf(layout.words[1].x - layout.words[0].x - 8) < .001f);
  assert(!poetryLayout("a\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na\na", layout, 2));
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
    if (choice >= 3 && choice <= 7) assert(layout.authorY == 296);
    for (int i = 0; i < layout.count; i++) {
      assert(isfinite(layout.words[i].x) && isfinite(layout.words[i].y));
      assert(layout.words[i].x - layout.words[i].length * 3 >= -120);
      assert(layout.words[i].x + layout.words[i].length * 3 <= 120);
      assert(layout.words[i].y >= -156 && layout.words[i].y <= 124);
    }
  }
  Poem randomized = fixedPoem; randomized.layout = 8;
  int previousResolved = -1;
  assert(lettersLoadPoem(randomized, MOTIONS[0]));
  for (int i = 0; i < 16; i++) {
    const int resolved = lettersInfo().resolvedLayout;
    assert(resolved >= 0 && resolved <= 12 && resolved != 8 && resolved != previousResolved);
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
    const float t = seconds * m.speed * POETRY_MOTION_TEMPO;
    const float globalP = poetryRandom(seed, 0, 60) * 6.28318530718f;
    const float coreX = (poetryRandom(seed, 0, 61) * 2 - 1) * 62 + 18 * sinf(t * .07f + globalP);
    const float coreY = (poetryRandom(seed, 0, 62) * 2 - 1) * 94 + 14 * cosf(t * .05f + globalP);
    for (int i = 0; i < 180; i++) {
      const WordPose field = poetryFieldMotion(m, i, 180, seconds, i % 4 == 0 ? 24 : 6, seed);
      assert(isfinite(field.x) && isfinite(field.y) && isfinite(field.z));
      assert(field.scale >= .55f && field.scale <= 1.1f);
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

  appBegin();
  assert(appState() == ST_MOVING);
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
  puts("PASS: layouts, 8 motions, 3D projection, renderer, touch/hold cycle");
}
