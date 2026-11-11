#include "letters.h"
#include "config.h"
#include "display.h"
#include <math.h>

struct Letter {
  char ch;
  char targetCh;
  float x, y;
  float vx, vy;
  float homeX, homeY;
  float tx, ty;
  float phase;
  float spawn;
  uint8_t size;
  bool alive;
  bool useTargetGlyph;
};

static Letter L[MAX_LETTERS];
static int nL = 0;
static char quoteCopy[192];
static uint32_t scatterMs = 0;
static bool scatterOn = false;
static bool regroupOn = false;

static uint32_t rng = 1;
static uint32_t rnd() {
  rng = rng * 1664525u + 1013904223u;
  return rng;
}

static void addLetter(char ch, float x, float y, uint8_t size) {
  if (nL >= MAX_LETTERS) return;
  Letter &e = L[nL++];
  e.ch = ch;
  e.targetCh = ch;
  e.x = e.homeX = x;
  e.y = e.homeY = y;
  e.tx = x;
  e.ty = y;
  e.vx = e.vy = 0;
  e.size = size;
  e.alive = true;
  e.useTargetGlyph = false;
  e.phase = nL * 0.37f;
  e.spawn = 0;
}

void lettersLoadQuote(const char *text) {
  strncpy(quoteCopy, text, sizeof(quoteCopy) - 1);
  quoteCopy[sizeof(quoteCopy) - 1] = 0;
  nL = 0;
  scatterOn = regroupOn = false;
  rng = 0xA5A5u * (uint32_t)(strlen(text) + 3);

  uint8_t size = strlen(text) > 28 ? 1 : 2;
  int charW = 6 * size;
  int charH = 8 * size;
  int y = 36;
  const char *p = text;

  while (*p && nL < MAX_LETTERS - 24) {
    while (*p == ' ') p++;
    if (!*p) break;
    const char *w = p;
    while (*p && *p != ' ') p++;
    int wlen = (int)(p - w);
    int ww = wlen * charW;
    int indent = 10 + (int)(rnd() % 9) * 6;
    if (indent + ww > LCD_W - 8) indent = 8;
    if ((rnd() % 5) == 0) y += charH / 2;
    if (y + charH > LCD_H - 44) {
      y = 32 + (int)(nL % 6) * 2;
      indent = 16 + (int)(nL % 8) * 4;
    }
    for (int i = 0; i < wlen; i++) {
      float jx = (int)(rnd() % 5) - 2;
      float jy = (int)(rnd() % 5) - 2;
      addLetter(w[i], indent + i * charW + jx, y + jy, size);
    }
    y += charH + 6 + (int)(rnd() % 10);
  }

  int orig = nL;
  if (orig == 0) {
    addLetter('?', 110, 140, 2);
    orig = 1;
  }
  int i = 0;
  while (nL < 24) {
    Letter src = L[i % orig];
    addLetter(src.ch, src.homeX + (int)(rnd() % 7) - 3,
              src.homeY + (int)(rnd() % 7) - 3, src.size);
    i++;
  }
}

void lettersStartScatter(float speedPx) {
  scatterOn = true;
  regroupOn = false;
  scatterMs = millis();
  const float cx = LCD_W / 2.f, cy = LCD_H / 2.f;
  for (int i = 0; i < nL; i++) {
    float dx = L[i].x - cx;
    float dy = L[i].y - cy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.f) {
      float a = (rnd() % 628) / 100.f;
      dx = cosf(a);
      dy = sinf(a);
      len = 1.f;
    }
    float sp = speedPx * (0.65f + (rnd() % 80) / 100.f);
    L[i].vx = dx / len * sp;
    L[i].vy = dy / len * sp;
    L[i].useTargetGlyph = false;
  }
}

void lettersUpdateScatter(float dt) {
  if (!scatterOn) return;
  for (int i = 0; i < nL; i++) {
    L[i].x += L[i].vx * dt;
    L[i].y += L[i].vy * dt;
  }
}

bool lettersScatterDone() {
  if (!scatterOn) return true;
  if (millis() - scatterMs >= SCATTER_MS) return true;
  int far = 0;
  const float cx = LCD_W / 2.f, cy = LCD_H / 2.f;
  for (int i = 0; i < nL; i++) {
    float dx = L[i].x - cx, dy = L[i].y - cy;
    if (dx * dx + dy * dy > 170.f * 170.f) far++;
  }
  return far > nL * 2 / 3;
}

void lettersStartClock() {
  scatterOn = false;
  regroupOn = true;
  static const char *labs[12] = {
      "12", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11"};
  const float cx = LCD_W / 2.f, cy = 138.f, r = 96.f;

  struct Tgt { char ch; float x, y; };
  Tgt t[24];
  int nt = 0;
  for (int i = 0; i < 12; i++) {
    float ang = -PI / 2.f + i * (PI / 6.f);
    float px = cx + cosf(ang) * r;
    float py = cy + sinf(ang) * r;
    t[nt].ch = labs[i][0];
    t[nt].x = px - 6;
    t[nt].y = py - 4;
    nt++;
    t[nt].ch = labs[i][1];
    t[nt].x = px + 6;
    t[nt].y = py - 4;
    nt++;
  }
  for (int i = 0; i < nL; i++) {
    Tgt &g = t[i % nt];
    L[i].tx = g.x + ((i / nt) * 3);
    L[i].ty = g.y + ((i / nt) * 2);
    L[i].targetCh = g.ch;
    L[i].vx = L[i].vy = 0;
    L[i].useTargetGlyph = false;
    L[i].size = 1;
  }
}

void lettersUpdateRegroup(float dt) {
  if (!regroupOn) return;
  float k = 1.f - expf(-dt * 6.5f);
  if (k > 1.f) k = 1.f;
  for (int i = 0; i < nL; i++) {
    L[i].x += (L[i].tx - L[i].x) * k;
    L[i].y += (L[i].ty - L[i].y) * k;
    float dx = L[i].tx - L[i].x, dy = L[i].ty - L[i].y;
    if (dx * dx + dy * dy < 64.f) L[i].useTargetGlyph = true;
  }
}

bool lettersRegroupSettled() {
  float maxd = 0;
  for (int i = 0; i < nL; i++) {
    float dx = L[i].tx - L[i].x, dy = L[i].ty - L[i].y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > maxd) maxd = d;
  }
  return maxd < 2.8f;
}

void lettersPrepareFocus() {
  char buf[192];
  int m = 0;
  for (int i = 0; quoteCopy[i] && m < 190; i++) {
    if (quoteCopy[i] != ' ') buf[m++] = quoteCopy[i];
  }
  if (m == 0) buf[m++] = '*';

  nL = 0;
  scatterOn = regroupOn = false;
  rng = 0xC0FFEEu ^ (uint32_t)m;
  const int N = 80;
  for (int i = 0; i < N; i++) {
    float x = 8 + (rnd() % (LCD_W - 22));
    float y = 14 + (rnd() % (LCD_H - 90));
    uint8_t sz = (rnd() & 3) == 0 ? 2 : 1;
    addLetter(buf[i % m], x, y, sz);
    L[nL - 1].spawn = (i + 0.35f) / (float)N;
    L[nL - 1].alive = false;
    L[nL - 1].phase = (rnd() % 628) / 100.f;
  }
}

void lettersUpdateFocus(float progress, float motionT, bool paused) {
  for (int i = 0; i < nL; i++) {
    L[i].alive = progress >= L[i].spawn;
    if (!L[i].alive) continue;
    if (!paused) {
      L[i].x = L[i].homeX + sinf(motionT * 1.25f + L[i].phase) * 3.4f;
      L[i].y = L[i].homeY + cosf(motionT * 0.95f + L[i].phase * 1.6f) * 2.4f;
    }
  }
}

void lettersDraw(uint16_t fg) {
  for (int i = 0; i < nL; i++) {
    if (!L[i].alive) continue;
    int16_t x = (int16_t)L[i].x;
    int16_t y = (int16_t)L[i].y;
    if (x < -12 || y < -12 || x > LCD_W || y > LCD_H) continue;
    gfx->setTextSize(L[i].size);
    gfx->setTextColor(fg);
    gfx->setCursor(x, y);
    gfx->write((uint8_t)(L[i].useTargetGlyph ? L[i].targetCh : L[i].ch));
  }
}

int lettersCount() { return nL; }
