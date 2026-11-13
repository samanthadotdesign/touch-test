#include "letters.h"
#include "display.h"
#include "poetry_layout.h"
#include "poetry_motion.h"
#include <font/glcdfont.h>
#include <algorithm>

namespace {
enum Phase : uint8_t { MOVING, ASSEMBLING, READING, DISSOLVING };
struct Particle {
  uint16_t offset;
  uint8_t length;
  bool echo;
  float blend;
  float targetX, targetY;
  WordPose pose;
  float anchorX, anchorY, anchorZ;
};
struct Scene {
  PoetryLayout layout;
  const Poem *poem = nullptr;
  const MotionPreset *motion = nullptr;
  Particle particles[POETRY_MAX_CHARS];
  int count = 0, realCount = 0;
  uint8_t resolvedLayout = 0;
  uint32_t seed = 0;
  float time = 0;
  bool valid = false;
};
struct DrawItem { uint16_t particle; Scene *scene; };
Scene scenes[2];
Scene *current = &scenes[0], *incoming = &scenes[1];
DrawItem order[POETRY_MAX_CHARS * 2];
Phase phase = MOVING;
float transitionTime = 0, progress = 0, eased = 0;
bool nextReady = false;

float clamp01(float n) { return n < 0 ? 0 : n > 1 ? 1 : n; }
float angle(float a) { return atan2f(sinf(a), cosf(a)); }
float unwrapped(float a, float anchor) { return anchor + angle(a - anchor); }
uint16_t grey(float amount) {
  const int g = (int)(clamp01(amount) * 255);
  return ((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3);
}
float shade(const Scene &scene, const WordPose &pose) {
  const float range = fmaxf(scene.motion->depth, 1);
  return .12f + .84f * powf(clamp01(.5f + pose.z / (2 * range)), 1.35f);
}

WordPose movingPose(const Scene &scene, int index) {
  return poetryFieldMotion(*scene.motion, index, scene.count, scene.time,
                           scene.particles[index].length * 6, scene.seed);
}

bool prepare(Scene &scene, const Poem &poem, const MotionPreset &motion) {
  const int previousLayout = current->valid ? current->resolvedLayout : -1;
  scene.poem = &poem;
  scene.motion = &motion;
  scene.time = 0;
  const uint32_t seedHigh = (uint32_t)random(65536);
  const uint32_t seedLow = (uint32_t)random(65536);
  scene.seed = (seedHigh << 16) | seedLow;
  scene.count = 0;
  scene.resolvedLayout = poem.layout;
  if (poem.layout == 8) {
    uint8_t eligible[12];
    int count = 0;
    for (uint8_t choice = 0; choice <= 12; choice++)
      if (choice != 8 && poetryLayout(poem.text, scene.layout, choice)) eligible[count++] = choice;
    if (count > 1) {
      for (int i = 0; i < count; i++) if (eligible[i] == previousLayout) {
        for (int j = i; j < count - 1; j++) eligible[j] = eligible[j + 1];
        count--;
        break;
      }
    }
    if (count) scene.resolvedLayout = eligible[random(count)];
  }
  scene.valid = poetryLayout(poem.text, scene.layout, scene.resolvedLayout);
  if (!scene.valid) {
    Serial.printf("Poem cannot fit screen: %s\n", poem.id);
    return false;
  }
  bool wholeWords[POETRY_MAX_WORDS] = {};
  int eligibleWords = 0, wholeCount = 0, firstEligible = -1, lastEligible = -1;
  for (int i = 0; i < scene.layout.count; i++) {
    if (scene.layout.words[i].length <= 1) continue;
    if (firstEligible < 0) firstEligible = i;
    lastEligible = i;
    eligibleWords++;
    wholeWords[i] = poetryRandom(scene.seed, i, 35) > .62f;
    if (wholeWords[i]) wholeCount++;
  }
  if (motion.unit == 2 && eligibleWords >= 2) {
    if (wholeCount == 0) wholeWords[firstEligible] = true;
    if (wholeCount == eligibleWords) wholeWords[lastEligible] = false;
  }
  for (int i = 0; i < scene.layout.count; i++) {
    const PoetryWord &word = scene.layout.words[i];
    const bool whole = motion.unit == 1 || (motion.unit == 2 && wholeWords[i]);
    const int groups = whole ? 1 : word.length;
    for (int c = 0; c < groups; c++) {
      Particle &p = scene.particles[scene.count++];
      p.offset = word.offset + (whole ? 0 : c);
      p.length = whole ? word.length : 1;
      p.targetX = whole ? word.x : word.x - word.length * 3 + c * 6 + 3;
      p.targetY = word.y;
      p.echo = false;
      p.blend = 0;
      p.anchorX = p.anchorY = p.anchorZ = 0;
    }
  }
  scene.realCount = scene.count;
  while (scene.count < 180 && scene.count < POETRY_MAX_CHARS) {
    Particle &echo = scene.particles[scene.count];
    const int source = (int)(poetryRandom(scene.seed, scene.count, 37) * scene.realCount);
    echo = scene.particles[source < scene.realCount ? source : scene.realCount - 1];
    echo.echo = true;
    if (motion.unit != 0 && poetryRandom(scene.seed, scene.count, 38) < .68f) {
      const int oldOffset = echo.offset, oldLength = echo.length;
      int maxRun = 0;
      while (maxRun < 18 && poem.text[oldOffset + maxRun]
             && poem.text[oldOffset + maxRun] != '\n') maxRun++;
      const int skipRange = oldLength < 4 ? oldLength : 4;
      const int skip = (int)(poetryRandom(scene.seed, scene.count, 39) * skipRange);
      int offset = oldOffset + skip;
      maxRun -= skip;
      const int maxLength = maxRun < 14 ? maxRun : 14;
      int length = 1 + (int)(poetryRandom(scene.seed, scene.count, 40) * maxLength);
      while (length > 0 && poem.text[offset] == ' ') { offset++; length--; }
      while (length > 0 && poem.text[offset + length - 1] == ' ') length--;
      if (length > 0) {
        echo.offset = offset;
        echo.length = length;
        echo.targetX += (offset - oldOffset) * 6 + (length - oldLength) * 3;
      }
    }
    scene.count++;
  }
  for (int i = 0; i < scene.count; i++)
    scene.particles[i].pose = movingPose(scene, i);
  return true;
}
void captureAnchors(Scene &scene) {
  for (int i = 0; i < scene.count; i++) {
    Particle &p = scene.particles[i];
    const WordPose dynamic = movingPose(scene, i);
    p.anchorX = angle(dynamic.rx);
    p.anchorY = angle(dynamic.ry);
    p.anchorZ = angle(dynamic.rz);
  }
}

void drawPixelQuad(const Scene &scene, const WordPose &v, const WordBasis &basis,
                   float x0, float y0, float x1, float y1, uint16_t color) {
  float x[4], y[4];
  if (!poetryProject(v, basis, x0, y0, scene.motion->perspective, x[0], y[0]) ||
      !poetryProject(v, basis, x1, y0, scene.motion->perspective, x[1], y[1]) ||
      !poetryProject(v, basis, x1, y1, scene.motion->perspective, x[2], y[2]) ||
      !poetryProject(v, basis, x0, y1, scene.motion->perspective, x[3], y[3])) return;
  float left = x[0], right = x[0], top = y[0], bottom = y[0];
  for (int i = 1; i < 4; i++) {
    left = fminf(left, x[i]); right = fmaxf(right, x[i]);
    top = fminf(top, y[i]); bottom = fmaxf(bottom, y[i]);
  }
  if (right < 0 || left >= LCD_W || bottom < 0 || top >= LCD_H) return;
  // Arduino's triangle rasterizer includes both end pixels. Pull each edge
  // inward before flooring so an identity 1x1 glyph cell draws one pixel,
  // matching the crisp built-in glyph used after assembly instead of thickening.
  const float cx = (x[0] + x[1] + x[2] + x[3]) * .25f;
  const float cy = (y[0] + y[1] + y[2] + y[3]) * .25f;
  for (int i = 0; i < 4; i++) {
    x[i] = floorf(fmaxf(-1024, fminf(1024, x[i] + (cx - x[i]) * .01f)));
    y[i] = floorf(fmaxf(-1024, fminf(1024, y[i] + (cy - y[i]) * .01f)));
  }
  gfx->fillTriangle((int)x[0], (int)y[0], (int)x[1], (int)y[1],
                    (int)x[2], (int)y[2], color);
  gfx->fillTriangle((int)x[0], (int)y[0], (int)x[2], (int)y[2],
                    (int)x[3], (int)y[3], color);
}
void drawParticle(const Scene &scene, int index, uint16_t color) {
  const Particle &p = scene.particles[index];
  const WordPose &v = p.pose;
  const float width = p.length * 6;
  if (phase == READING) {
    gfx->setTextSize(1);
    gfx->setTextColor(color);
    gfx->setCursor((int)(120 + v.x - width / 2), (int)(160 + v.y - 4));
    for (int c = 0; c < p.length; c++) gfx->write((uint8_t)scene.poem->text[p.offset + c]);
    return;
  }
  const WordBasis basis = poetryBasis(v);
  for (int c = 0; c < p.length; c++) {
    const uint8_t ch = (uint8_t)scene.poem->text[p.offset + c];
    for (int col = 0; col < 5; col++) {
      const uint8_t bits = pgm_read_byte(&font[ch * 5 + col]);
      for (int row = 0; row < 8;) {
        if (!(bits & (1 << row))) { row++; continue; }
        const int first = row;
        while (row < 8 && (bits & (1 << row))) row++;
        const float x = c * 6 + col - width / 2;
        drawPixelQuad(scene, v, basis, x, first - 4, x + 1, row - 4, color);
      }
    }
  }
}
}

bool lettersLoadPoem(const Poem &p, const MotionPreset &m) {
  phase = MOVING;
  transitionTime = progress = eased = 0;
  nextReady = false;
  return prepare(*current, p, m);
}
bool lettersPrepareNext(const Poem &p, const MotionPreset &m) {
  nextReady = prepare(*incoming, p, m);
  return nextReady;
}
void lettersAdoptNext() {
  if (!nextReady) return;
  Scene *previous = current;
  current = incoming;
  incoming = previous;
  nextReady = false;
  phase = MOVING;
  transitionTime = progress = eased = 0;
}

void lettersUpdate(float seconds) {
  current->time += seconds;
  if (phase == DISSOLVING && nextReady) incoming->time += seconds;
  transitionTime += seconds;
  const float duration = phase == DISSOLVING ? .65f : .55f;
  progress = clamp01(transitionTime / duration);
  eased = progress * progress * progress * (progress * (progress * 6 - 15) + 10);
  if (!current->valid) return;
  for (int i = 0; i < current->count; i++) {
    Particle &p = current->particles[i];
    WordPose dynamic = movingPose(*current, i);
    if (phase == MOVING) { p.pose = dynamic; p.blend = 0; }
    else if (phase == READING) {
      p.blend = 1;
      p.pose = {};
      p.pose.x = p.targetX; p.pose.y = p.targetY;
    } else {
      const bool assembling = phase == ASSEMBLING;
      const float delay = (assembling ? .08f : .07f)
          * poetryRandom(current->seed, i, assembling ? 20 : 22);
      const float duration = assembling
          ? .34f + .13f * poetryRandom(current->seed, i, 21)
          : .40f + .17f * poetryRandom(current->seed, i, 23);
      const float p0 = clamp01((transitionTime - delay) / duration);
      p.blend = p0 * p0 * p0 * (p0 * (p0 * 6 - 15) + 10);
      const float blend = assembling ? 1 - p.blend : p.blend;
      p.pose.x = p.targetX + (dynamic.x - p.targetX) * blend;
      p.pose.y = p.targetY + (dynamic.y - p.targetY) * blend;
      p.pose.z = dynamic.z * blend;
      p.pose.scale = 1 + (dynamic.scale - 1) * blend;
      p.pose.rx = unwrapped(dynamic.rx, p.anchorX) * blend;
      p.pose.ry = unwrapped(dynamic.ry, p.anchorY) * blend;
      p.pose.rz = unwrapped(dynamic.rz, p.anchorZ) * blend;
    }
  }
  if (phase == DISSOLVING && nextReady) {
    for (int i = 0; i < incoming->count; i++)
      incoming->particles[i].pose = movingPose(*incoming, i);
  }
  if (phase == ASSEMBLING && progress >= 1) phase = READING;
}
void lettersAssemble() {
  if (phase != MOVING) return;
  captureAnchors(*current);
  for (int i = 0; i < current->count; i++) current->particles[i].blend = 0;
  phase = ASSEMBLING;
  transitionTime = progress = eased = 0;
}
void lettersDissolve() {
  if (phase != READING || !nextReady) return;
  captureAnchors(*current);
  for (int i = 0; i < current->count; i++) current->particles[i].blend = 0;
  phase = DISSOLVING;
  transitionTime = progress = eased = 0;
}
bool lettersTransitionDone() {
  return phase == READING || (phase == DISSOLVING && progress >= 1);
}
void lettersDraw() {
  gfx->fillScreen(C_BLACK);
  if (!current->valid) {
    displayCenter("poem does not fit", 140, 1, C_WHITE, C_BLACK);
    displayCenter("check poetry database", 160, 1, C_GREY, C_BLACK);
    return;
  }
  int count = 0;
  const int scenesToDraw = phase == DISSOLVING && nextReady ? 2 : 1;
  for (int s = 0; s < scenesToDraw; s++) {
    Scene *scene = s == 0 ? current : incoming;
    for (int i = 0; i < scene->count; i++) {
      order[count++] = {(uint16_t)i, scene};
    }
  }
  // Introsort uses the fixed array, with no heap allocation and O(n log n)
  // comparisons even during the maximum two-scene overlap. Ties keep a
  // deterministic order independent of the previous frame's particle depths.
  std::sort(order, order + count, [](const DrawItem &a, const DrawItem &b) {
    const float az = a.scene->particles[a.particle].pose.z;
    const float bz = b.scene->particles[b.particle].pose.z;
    if (az < bz) return true;
    if (az > bz) return false;
    if (a.scene != b.scene) return a.scene == current;
    return a.particle < b.particle;
  });
  for (int n = 0; n < count; n++) {
    const DrawItem &item = order[n];
    const Particle &p = item.scene->particles[item.particle];
    if (p.echo && phase == READING) continue;
    const float movingShade = shade(*item.scene, p.pose);
    float brightness = movingShade;
    if (phase == ASSEMBLING) brightness += (.9f - brightness) * p.blend;
    if (phase == READING) brightness = .9f;
    if (phase == DISSOLVING) {
      brightness = item.scene == incoming ? movingShade * eased
        : (.9f + (movingShade - .9f) * p.blend) * (1 - eased);
    }
    if (p.echo) {
      brightness *= .78f;
      if (phase == ASSEMBLING) brightness *= 1 - p.blend;
      if (phase == DISSOLVING && item.scene == current) brightness *= p.blend;
    }
    if (brightness > .003f) drawParticle(*item.scene, item.particle, grey(brightness));
  }
  if (phase == ASSEMBLING || phase == READING || phase == DISSOLVING) {
    const float opacity = phase == READING ? 1 : phase == ASSEMBLING ? eased : 1 - eased;
    displayCenter(current->poem->by, (int)current->layout.authorY, 1,
                  grey(.38f * opacity), C_BLACK);
  }
}
int lettersCount() { return current->count; }
LettersInfo lettersInfo() {
  float minBlend = 1, maxBlend = 0, minScale = 1000, maxScale = 0;
  int echoPhrases = 0;
  bool sourceBoundsValid = true;
  const size_t textLength = current->poem ? strlen(current->poem->text) : 0;
  for (int i = 0; i < current->count; i++) {
    minBlend = fminf(minBlend, current->particles[i].blend);
    maxBlend = fmaxf(maxBlend, current->particles[i].blend);
    const Particle &p = current->particles[i];
    minScale = fminf(minScale, p.pose.scale);
    maxScale = fmaxf(maxScale, p.pose.scale);
    if (!p.length || p.offset + p.length > textLength) sourceBoundsValid = false;
    else {
      bool phrase = false;
      for (int c = 0; c < p.length; c++) {
        if (current->poem->text[p.offset + c] == '\n') sourceBoundsValid = false;
        if (current->poem->text[p.offset + c] == ' ') phrase = true;
      }
      if (p.echo && phrase) echoPhrases++;
    }
  }
  return {current->poem ? current->poem->id : nullptr,
          nextReady ? incoming->poem->id : nullptr,
          current->count, nextReady ? incoming->count : 0,
          current->time, nextReady ? incoming->time : 0,
          current->resolvedLayout, nextReady ? incoming->resolvedLayout : -1,
          current->realCount, nextReady ? incoming->realCount : 0,
          current->seed, nextReady ? incoming->seed : 0, minBlend, maxBlend,
          minScale, maxScale, echoPhrases, sourceBoundsValid};
}
