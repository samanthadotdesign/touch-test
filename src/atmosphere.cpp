#include "atmosphere.h"
#include "display.h"
#include <math.h>

namespace {
constexpr float TAU = 6.28318530718f;
constexpr int GRID_STEP = 8;
constexpr int GRID_W = (LCD_W + GRID_STEP - 1) / GRID_STEP + 1;
constexpr int GRID_H = (LCD_H + GRID_STEP - 1) / GRID_STEP + 1;
constexpr int BLOB_COUNT = 7;
constexpr int EXP_STEPS = 256;
constexpr float GRID_INTERVAL = .1f;

struct RGB { uint8_t r, g, b; };
struct Colour { float r, g, b; };
struct Stop { float position; Colour colour; };
struct Blob { float x, y, inverseRadius; Colour colour; };

// Figma Frame 33:31 shader: active Palette 1, 2 and 3 (Palette 4 is inactive).
// https://www.figma.com/design/nVvkRzyAbBU2QTXQGdhttv/motion?node-id=33-31
const Stop PALETTE_1[] = {
  {0.f, {.96f, .93f, .83f}}, {.32f, {.88f, .80f, .44f}},
  {.65f, {.64f, .43f, .57f}}, {1.f, {.65f, .78f, .87f}}
};
const Stop PALETTE_2[] = {
  {0.f, {.88f, .39f, .37f}}, {.5f, {.72f, .61f, .81f}},
  {1.f, {.98f, .90f, .68f}}
};
const Stop PALETTE_3[] = {
  {0.f, {.76f, .53f, .22f}}, {.5f, {.61f, .40f, .53f}},
  {1.f, {.96f, .93f, .84f}}
};
const Stop NIGHT[] = {
  {0.f, {.015f, .015f, .018f}}, {.32f, {.055f, .055f, .065f}},
  {.65f, {.22f, .23f, .25f}}, {1.f, {.56f, .57f, .60f}}
};
const Stop PAPER[] = {
  {0.f, {1.f, 1.f, .99f}}, {.32f, {.96f, .96f, .95f}},
  {.65f, {.85f, .85f, .85f}}, {1.f, {.69f, .70f, .72f}}
};

RGB grid[GRID_H][GRID_W];
float exponentials[EXP_STEPS + 1];
bool initialized = false, microphoneOn = false, pendingBlow = false;
bool armed = true;
AtmosphereTheme theme = ATM_COLOUR;
uint8_t colourway = 0;
float level = 0, detectionLevel = 0, threshold = .75f;
float held = 0, quiet = 0, cooldown = 0;
float driftClock = 0, paletteClock = 0, gridAge = 0;

float bounded(float n, float low, float high) {
  return n < low ? low : n > high ? high : n;
}
float normalized(float n) { return isfinite(n) ? bounded(n, 0, 1) : 0; }
float fract(float n) { return n - floorf(n); }
float mix(float a, float b, float t) { return a + (b - a) * t; }
float smooth(float n) { n = bounded(n, 0, 1); return n * n * (3 - 2 * n); }
float fieldHash(float x, float y) {
  const float qx = fract(x * 123.34f), qy = fract(y * 456.21f);
  const float d = qx * (qx + 45.32f) + qy * (qy + 45.32f);
  return fract((qx + d) * (qy + d));
}
float noise(float x, float y) {
  const float ix = floorf(x), iy = floorf(y);
  const float fx = smooth(x - ix), fy = smooth(y - iy);
  return mix(mix(fieldHash(ix, iy), fieldHash(ix + 1, iy), fx),
             mix(fieldHash(ix, iy + 1), fieldHash(ix + 1, iy + 1), fx), fy);
}
Colour blend(Colour a, Colour b, float amount) {
  return {mix(a.r, b.r, amount), mix(a.g, b.g, amount), mix(a.b, b.b, amount)};
}
Colour ramp(const Stop *stops, int count, float position) {
  if (position <= stops[0].position) return stops[0].colour;
  for (int i = 1; i < count; i++) {
    if (position <= stops[i].position) {
      const float t = (position - stops[i - 1].position) /
                      (stops[i].position - stops[i - 1].position);
      return blend(stops[i - 1].colour, stops[i].colour, t);
    }
  }
  return stops[count - 1].colour;
}
Colour colourPalette(int index, float position) {
  if (index == 0) return ramp(PALETTE_1, 4, position);
  if (index == 1) return ramp(PALETTE_2, 3, position);
  return ramp(PALETTE_3, 3, position);
}
Colour evolvingPalette(float position, float phase, float rate) {
  if (theme == ATM_NIGHT) return ramp(NIGHT, 4, position);
  if (theme == ATM_PAPER) return ramp(PAPER, 4, position);
  const float progress = paletteClock * rate + phase + colourway;
  const float count = ATM_COLOURWAY_COUNT;
  const float cycle = progress - floorf(progress / count) * count;
  const int current = (int)cycle;
  return blend(colourPalette(current, position),
               colourPalette((current + 1) % ATM_COLOURWAY_COUNT, position), smooth(fract(cycle)));
}
float gaussian(float exponent) {
  // Lookup replaces almost 9,000 expf calls for each grid update.
  if (exponent >= 16) return 0;
  const float coordinate = exponent * (EXP_STEPS / 16.f);
  const int index = (int)coordinate;
  return mix(exponentials[index], exponentials[index + 1], coordinate - index);
}
uint8_t byteChannel(float value) {
  return (uint8_t)(bounded(value, 0, 1) * 255 + .5f);
}

void rebuildGrid() {
  Blob blobs[BLOB_COUNT];
  const float metricX = 1, metricY = (float)LCD_H / LCD_W;
  const float force = normalized(level);
  for (int i = 0; i < BLOB_COUNT; i++) {
    const float r1 = fieldHash(i + .7f, 13.f);
    const float r2 = fieldHash(i + 8.2f, 14.9f);
    const float r3 = fieldHash(i + 3.1f, 18.1f);
    const float phase = r3 * TAU;
    float x = (sinf(driftClock * (.38f + r1 * .22f) + phase) +
               .2f * sinf(driftClock * (.19f + r3 * .08f) + r2 * 9)) * (.46f / 1.2f);
    float y = (cosf(driftClock * (.34f + r2 * .24f) + phase * 1.37f) +
               .2f * sinf(driftClock * (.23f + r1 * .07f) + r1 * 8)) * (.46f / 1.2f);
    const float ox = x + cosf(phase) * .02f, oy = y + sinf(phase) * .02f;
    const float length = fmaxf(sqrtf(ox * ox + oy * oy), .001f);
    x += ox / length * force * .34f;
    y += oy / length * force * .34f;
    const float radius = .65f * mix(.73f, 1.18f, r3);
    const float palettePhase = phase + i * 1.91f;
    const float position = .5f + .5f * sinf(palettePhase +
        .28f * sinf(palettePhase * .43f + r2 * 6));
    blobs[i] = {x * metricX, y * metricY,
                mix(8.f, 2.7f, .72f) / (radius * radius),
                evolvingPalette(position, r2 * ATM_COLOURWAY_COUNT + i * .381966f, .88f + r3 * .24f)};
  }
  const Colour base = evolvingPalette(.12f, 0, 1);
  for (int gy = 0; gy < GRID_H; gy++) {
    const float py = ((float)(gy * GRID_STEP) - LCD_H * .5f) / LCD_W;
    for (int gx = 0; gx < GRID_W; gx++) {
      const float px = ((float)(gx * GRID_STEP) - LCD_W * .5f) / LCD_W;
      const float x = px + (noise(px * 1.8f + 12 * .17f,
                                  py * 1.8f + driftClock * .15f) - .5f) * .26f;
      const float y = py + (noise(px * 1.8f + 8.3f + driftClock * .12f,
                                  py * 1.8f + 12 * .23f) - .5f) * .26f;
      float r = base.r * .012f, g = base.g * .012f, b = base.b * .012f;
      float total = .012f;
      for (int i = 0; i < BLOB_COUNT; i++) {
        const float dx = x - blobs[i].x, dy = y - blobs[i].y;
        const float weight = gaussian((dx * dx + dy * dy) * blobs[i].inverseRadius);
        r += blobs[i].colour.r * weight;
        g += blobs[i].colour.g * weight;
        b += blobs[i].colour.b * weight;
        total += weight;
      }
      grid[gy][gx] = {byteChannel(r / total), byteChannel(g / total), byteChannel(b / total)};
    }
  }
}

int grain(int x, int y) {
  // Fixed screen-space grain: no frame-to-frame noise or sparkling pixels.
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + 12u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return ((int)(h >> 24) - 128) / 24;
}
int channel(int value) { return value < 0 ? 0 : value > 255 ? 255 : value; }
uint16_t rgb565(int r, int g, int b, int noiseAmount) {
  r = channel(r + noiseAmount);
  g = channel(g + noiseAmount);
  b = channel(b + noiseAmount);
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
int interpolate(uint8_t a, uint8_t b, uint8_t c, uint8_t d, int fx, int fy) {
  return ((a * (8 - fx) + b * fx) * (8 - fy) +
          (c * (8 - fx) + d * fx) * fy + 32) >> 6;
}
void resetGate() { held = quiet = cooldown = 0; armed = true; pendingBlow = false; }
void updateGate(float dt, bool enabled) {
  if (!enabled) { resetGate(); return; }
  cooldown = fmaxf(0, cooldown - dt);
  if (!armed) {
    quiet = detectionLevel <= fminf(.25f, threshold * .45f) ? quiet + dt : 0;
    if (quiet + 1e-6f >= .5f && cooldown <= 1e-6f) { armed = true; held = 0; }
    return;
  }
  held = detectionLevel >= threshold ? held + dt : 0;
  if (held + 1e-6f < .35f) return;
  pendingBlow = true;
  armed = false;
  held = quiet = 0;
  cooldown = 1.6f;
}
}  // namespace

void atmosphereBegin() {
  theme = ATM_COLOUR;
  colourway = 0;
  level = detectionLevel = driftClock = paletteClock = gridAge = 0;
  threshold = .75f;
  microphoneOn = false;
  resetGate();
  for (int i = 0; i <= EXP_STEPS; i++) exponentials[i] = expf(-i * (16.f / EXP_STEPS));
  initialized = true;
  rebuildGrid();
}

void atmosphereUpdate(float dt, float rawLevel, bool micEnabled, bool allowAdvance) {
  if (!initialized) atmosphereBegin();
  dt = isfinite(dt) ? bounded(dt, 0, .1f) : 0;
  if (microphoneOn != micEnabled) { detectionLevel = 0; resetGate(); }
  microphoneOn = micEnabled;
  const float target = micEnabled ? normalized(rawLevel) : 0;
  const float response = 1 - expf(-dt / (target > level ? .09f : .65f));
  level += (target - level) * response;
  const float detectionResponse = 1 - expf(-dt / (target > detectionLevel ? .09f : .65f));
  detectionLevel += (target - detectionLevel) * detectionResponse;
  updateGate(dt, micEnabled && allowAdvance);
  driftClock += dt * .36f * atmosphereMotionScale();
  paletteClock += dt * .12f;
  gridAge += dt;
  if (gridAge + 1e-6f >= GRID_INTERVAL) {
    gridAge = fmodf(gridAge, GRID_INTERVAL);
    rebuildGrid();
  }
}

bool atmosphereTakeBlow() { const bool result = pendingBlow; pendingBlow = false; return result; }
float atmosphereLevel() { return level; }
float atmosphereMotionScale() { return fminf(5.f, (microphoneOn ? 1.25f : 1.f) + level * 3.75f); }
float atmosphereBlowForce() { return level; }
void atmosphereSetTheme(AtmosphereTheme value) {
  if (!initialized) atmosphereBegin();
  if (value != ATM_NIGHT && value != ATM_PAPER && value != ATM_COLOUR) return;
  if (theme != value) { theme = value; rebuildGrid(); }
}
AtmosphereTheme atmosphereTheme() { return theme; }
void atmosphereSetColourway(int value) {
  if (!initialized) atmosphereBegin();
  const uint8_t selected = value >= 0 && value < ATM_COLOURWAY_COUNT ? (uint8_t)value : 0;
  if (colourway != selected) { colourway = selected; rebuildGrid(); }
}
uint8_t atmosphereColourway() { return colourway; }
void atmosphereSetThreshold(float value) {
  if (!initialized) atmosphereBegin();
  threshold = isfinite(value) ? bounded(value, .35f, .95f) : .75f;
  held = 0;
}
float atmosphereThreshold() { return threshold; }

uint16_t atmospherePixel(int x, int y) {
  if (!initialized) atmosphereBegin();
  x = x < 0 ? 0 : x >= LCD_W ? LCD_W - 1 : x;
  y = y < 0 ? 0 : y >= LCD_H ? LCD_H - 1 : y;
  const int gx = x >> 3, gy = y >> 3, fx = x & 7, fy = y & 7;
  const RGB &a = grid[gy][gx], &b = grid[gy][gx + 1];
  const RGB &c = grid[gy + 1][gx], &d = grid[gy + 1][gx + 1];
  return rgb565(interpolate(a.r, b.r, c.r, d.r, fx, fy),
                interpolate(a.g, b.g, c.g, d.g, fx, fy),
                interpolate(a.b, b.b, c.b, d.b, fx, fy), grain(x, y));
}

uint16_t atmosphereInk(float opacity, int x, int y) {
  const uint16_t background = atmospherePixel(x, y);
  const int a = (int)(normalized(opacity) * 256 + .5f), inverse = 256 - a;
  const int r = (background >> 11) & 31, g = (background >> 5) & 63, b = background & 31;
  const int white = theme == ATM_NIGHT ? 1 : 0;
  return (uint16_t)((((r * inverse + 31 * white * a + 128) >> 8) << 11) |
                    (((g * inverse + 63 * white * a + 128) >> 8) << 5) |
                    ((b * inverse + 31 * white * a + 128) >> 8));
}

void atmosphereDraw() {
  if (!initialized) atmosphereBegin();
  uint16_t *frame = displayFramebuffer();
  if (!frame) {
    if (!gfx) return;
    for (int y = 0; y < LCD_H; y += 4)
      for (int x = 0; x < LCD_W; x += 4)
        gfx->fillRect(x, y, 4, 4, atmospherePixel(x + 2, y + 2));
    return;
  }
  // Share the vertical interpolation for all pixels in one row. Multiplication,
  // static grain, and RGB565 packing are the only work at display resolution.
  uint16_t row[GRID_W][3];
  for (int y = 0; y < LCD_H; y++) {
    const int gy = y >> 3, fy = y & 7;
    for (int gx = 0; gx < GRID_W; gx++) {
      const RGB &a = grid[gy][gx], &b = grid[gy + 1][gx];
      row[gx][0] = a.r * (8 - fy) + b.r * fy;
      row[gx][1] = a.g * (8 - fy) + b.g * fy;
      row[gx][2] = a.b * (8 - fy) + b.b * fy;
    }
    for (int x = 0; x < LCD_W; x++) {
      const int gx = x >> 3, fx = x & 7;
      const int r = (row[gx][0] * (8 - fx) + row[gx + 1][0] * fx + 32) >> 6;
      const int g = (row[gx][1] * (8 - fx) + row[gx + 1][1] * fx + 32) >> 6;
      const int b = (row[gx][2] * (8 - fx) + row[gx + 1][2] * fx + 32) >> 6;
      frame[y * LCD_W + x] = rgb565(r, g, b, grain(x, y));
    }
  }
}
