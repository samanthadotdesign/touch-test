#pragma once

// Pure motion math: add styles here and in sandbox/engine.js.
// Units: x/y/z and camera in pixels, rotations in radians, time in seconds.
#include <math.h>
#include "poetry_data.h"

constexpr float POETRY_MOTION_TEMPO = 2.f;

struct WordPose {
  float x = 0, y = 0, z = 0;
  float rx = 0, ry = 0, rz = 0;
  float scale = 1;
};

inline WordPose poetryMotion(const MotionPreset &m, int index, int count, float seconds) {
  const float u = (index + .5f) / count;
  const float p = index * 2.39996323f;
  const float t = seconds * m.speed;
  WordPose v;
  float a, r;
  switch (m.kind) {
    case 0: // Orbit: nested paths with gentle swivelling.
      a = p + t * .45f;
      v.x = cosf(a) * m.spread * (.35f + .55f * u);
      v.y = sinf(a * .8f + p * .3f) * m.spread * .85f;
      v.z = sinf(a) * m.depth;
      v.rx = sinf(a * .7f) * m.tilt;
      v.ry = cosf(a * .6f) * m.tilt;
      v.rz = sinf(a * .4f) * m.spin;
      break;
    case 1: // Helix: a vertical column in depth.
      a = p + t * .65f;
      v.x = cosf(a) * m.spread * .7f;
      v.y = (u * 2 - 1) * m.spread * 1.1f + sinf(t * .4f + p) * 8;
      v.z = sinf(a) * m.depth;
      v.rx = sinf(a) * m.tilt * .4f;
      v.ry = sinf(a) * m.tilt;
      v.rz = sinf(a * .6f) * m.spin;
      break;
    case 2: // Wave: words travel through a rippling field.
      v.x = (2 * u - 1) * m.spread;
      v.y = sinf(p + t) * m.spread * .65f;
      v.z = cosf(p + t) * m.depth;
      v.rx = cosf(p + t) * m.tilt;
      v.ry = sinf(p + t * .7f) * m.tilt;
      v.rz = cosf(p + t * .5f) * m.spin;
      break;
    case 3: // Vortex: nested spiralling rings.
      a = p + t * (.25f + u * .6f);
      r = m.spread * (.18f + .72f * u);
      v.x = cosf(a) * r;
      v.y = sinf(a) * r * 1.25f;
      v.z = sinf(p - t * .6f) * m.depth;
      v.rx = sinf(a) * m.tilt;
      v.ry = cosf(a) * m.tilt;
      v.rz = a * m.spin;
      break;
    case 4: // Drift: independent slow paths, no fixed centre of rotation.
      v.x = sinf(p + t * .23f) * m.spread;
      v.y = cosf(p * .7f + t * .19f) * m.spread * 1.15f;
      v.z = sinf(p * .4f + t * .31f) * m.depth;
      v.rx = sinf(p + t * .3f) * m.tilt;
      v.ry = cosf(p + t * .27f) * m.tilt;
      v.rz = sinf(p + t * .2f) * m.spin;
      break;
    case 5: // Tumble: continuous rotation on all three axes.
      v.x = cosf(p + t * .4f) * m.spread * .85f;
      v.y = sinf(p * .7f + t * .3f) * m.spread;
      v.z = sinf(p + t * .5f) * m.depth;
      v.rx = (t * .45f + p) * m.tilt;
      v.ry = (t * .6f + p) * m.tilt;
      v.rz = (t * .3f + p) * m.spin;
      break;
    case 6: { // Cloud: a rotating sphere of independent characters.
      const float h = 1 - 2 * u;
      const float radius = sqrtf(fmaxf(0, 1 - h * h));
      a = p + t * .28f;
      v.x = cosf(a) * radius * m.spread;
      v.y = h * m.spread * 1.15f + sinf(a * .7f) * 5;
      v.z = sinf(a) * radius * m.depth;
      v.rx = sinf(p + t * .2f) * m.tilt * .35f;
      v.ry = sinf(a) * m.tilt;
      v.rz = sinf(t * .2f + p) * m.spin;
      break;
    }
    default: { // Ribbon: curved rows, connected through a travelling wave.
      const int rows = (count + 17) / 18, row = index / 18, col = index % 18;
      a = (col - 8.5f) * .12f + t * .25f;
      v.x = sinf(a) * m.spread;
      v.y = (row - (rows - 1) / 2.f) * 12 + cosf(a * .6f + t * .3f) * m.spread * .25f;
      v.z = sinf(a) * m.depth;
      v.rx = m.tilt * .35f;
      v.ry = sinf(a) * m.tilt * .8f;
      v.rz = -.18f + m.spin * sinf(t * .25f);
      break;
    }
  }
  return v;
}

// Stateless per-particle randomness: all trajectories remain reproducible for
// a scene seed, with no frame-to-frame noise or shared direction/rate.
inline float poetryRandom(uint32_t seed, int index, int channel) {
  uint32_t x = seed ^ ((uint32_t)(index + 1) * 0x9e3779b9u)
                    ^ ((uint32_t)(channel + 1) * 0x85ebca6bu);
  x ^= x >> 16; x *= 0x7feb352du;
  x ^= x >> 15; x *= 0x846ca68bu;
  x ^= x >> 16;
  return (x >> 8) / 16777216.f;
}

// Coherent formations with independent seeded text fragments. Dense centres,
// loose plumes and irregular islands leave intentional negative space.
inline WordPose poetryFieldMotion(const MotionPreset &m, int index, int count,
                                  float seconds, float width = 6, uint32_t seed = 0) {
  seconds *= POETRY_MOTION_TEMPO;
  const auto r = [seed, index](int channel) { return poetryRandom(seed, index, channel); };
  const float pi = 3.14159265359f, pi2 = 2 * pi;
  const float rate = .35f + 1.4f * r(3), direction = r(4) < .5f ? -1 : 1;
  const float phaseX = r(0) * pi2, phaseY = r(1) * pi2, phaseZ = r(2) * pi2;
  WordPose v = poetryMotion(m, index, count, seconds * rate * direction + phaseX * 18);
  const float t = seconds * m.speed;
  v.z = .65f * v.z + .35f * m.depth * sinf(t * (.15f + .45f * r(11)) + phaseZ);
  v.rx = .45f * sinf(v.rx) + .15f * m.tilt * sinf(t * rate * .15f + phaseZ);
  v.ry = .45f * sinf(v.ry) + .15f * m.tilt * cosf(t * rate * .13f + phaseX);
  v.rz = .12f * sinf(v.rz) + .08f * m.spin * sinf(t * rate * .11f + phaseY);
  v.scale = .55f + .55f * r(27) * r(27);
  const float gX = r(14) + r(15) + r(16) - 1.5f;
  const float gY = r(17) + r(18) + r(19) - 1.5f;
  float sx, sy;
  if (m.formation == 0) { // A dense clump with a long, dispersing plume.
    const float globalP = poetryRandom(seed, 0, 60) * pi2;
    const float baseX = (poetryRandom(seed, 0, 61) * 2 - 1) * 62;
    const float baseY = (poetryRandom(seed, 0, 62) * 2 - 1) * 94;
    const float cx = baseX + 18 * sinf(t * .07f + globalP);
    const float cy = baseY + 14 * cosf(t * .05f + globalP);
    const float tailAngle = (hypotf(baseX, baseY) < 12
      ? poetryRandom(seed, 0, 63) * pi2 : atan2f(-baseY, -baseX))
      + (poetryRandom(seed, 0, 64) * 2 - 1) * .4f;
    if (r(13) < .74f) {
      sx = cx + gX * (10 + 38 * r(24) * r(24));
      sy = cy + gY * (12 + 28 * r(25) * r(25));
    } else if (r(13) < .92f) {
      const float u = powf(r(24), 1.6f);
      sx = cx + cosf(tailAngle) * 180 * u + gX * (8 + 14 * u);
      sy = cy + sinf(tailAngle) * 225 * u + gY * (9 + 22 * u);
    } else {
      sx = (2 * r(14) - 1) * 110;
      sy = (2 * r(17) - 1) * 146;
    }
  } else if (m.formation == 1) { // A snaking typographic stream, loosely bunched.
    float u = r(28) < .70f
      ? (floorf(r(13) * 6) + .5f) / 6 + .038f * (r(14) + r(15) - 1)
      : r(13);
    u += .018f * sinf(t * .14f + phaseX);
    sx = 45 * sinf(u * pi * 2.7f - .7f + t * .05f)
         + (u - .5f) * 85 + gX * (4 + 16 * r(25) * r(25));
    sy = (u - .5f) * 284 + gY * (3 + 9 * r(24));
    if (r(28) > .83f) sx += (r(29) < .5f ? -1 : 1) * (30 + 35 * r(30));
  } else { // Small broken-line islands with occasional escaped fragments.
    const int group = (int)floorf(r(13) * 6);
    const float cx = (group % 2 ? 52 : -52)
        + 12 * sinf(group * 1.7f + poetryRandom(seed, 0, 60) * pi2);
    const float cy = -118 + group * 46;
    const int row = (int)floorf(r(14) * 4);
    sx = cx + (r(15) - .5f) * 74;
    sy = cy + (row - 1.5f) * 8 + (r(16) - .5f) * 3;
    if (r(28) < .12f) {
      sx = (2 * r(29) - 1) * 110;
      sy = (2 * r(30) - 1) * 146;
    }
  }
  sx += sinf(t * (.22f + .28f * r(8)) + phaseX) * (5 + 4 * r(31));
  sy += cosf(t * (.20f + .28f * r(10)) + phaseY) * (5 + 4 * r(32));
  const float q = m.perspective / (m.perspective - v.z);
  const float limitX = fmaxf(6, 116 - fminf(104, width * v.scale * q * .55f));
  const float limitY = fmaxf(20, 154 - 4 * v.scale * q);
  v.x = fmaxf(-limitX, fminf(limitX, sx)) / q;
  v.y = fmaxf(-limitY, fminf(limitY, sy)) / q;
  return v;
}

struct WordBasis {
  float xx, xy, xz, yx, yy, yz;
};
inline WordBasis poetryBasis(const WordPose &v) {
  const float sx = sinf(v.rx), cx = cosf(v.rx);
  const float sy = sinf(v.ry), cy = cosf(v.ry);
  const float sz = sinf(v.rz), cz = cosf(v.rz);
  // Rotation order X, then Y, then Z. Only two axes needed for flat glyphs.
  return {cy * cz, cy * sz, -sy,
          sx * sy * cz - cx * sz, sx * sy * sz + cx * cz, sx * cy};
}
inline bool poetryProject(const WordPose &v, const WordBasis &b,
                          float lx, float ly, float camera, float &x, float &y) {
  lx *= v.scale;
  ly *= v.scale;
  const float px = v.x + lx * b.xx + ly * b.yx;
  const float py = v.y + lx * b.xy + ly * b.yy;
  const float pz = v.z + lx * b.xz + ly * b.yz;
  if (camera - pz < 24) return false; // near-plane clipping
  const float q = camera / (camera - pz);
  x = 120 + px * q;
  y = 160 + py * q;
  return true;
}
