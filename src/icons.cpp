#include "icons.h"

#include <math.h>

#include "theme.h"

namespace {

constexpr uint32_t SUN = 0xFFC23Du;
constexpr uint32_t MOON = 0xEDE7C8u;
constexpr uint32_t STAR = 0xFFFFFFu;
constexpr uint32_t CLOUD_DAY = 0xF2F5F9u;
constexpr uint32_t CLOUD_NIGHT = 0xC5CEDBu;
constexpr uint32_t CLOUD_BACK = 0x9AA6B8u;
constexpr uint32_t CLOUD_STORM = 0x7D8797u;
constexpr uint32_t CLOUD_FLASH = 0xB9C1CEu;
constexpr uint32_t RAIN = 0x5AAEFFu;
constexpr uint32_t SNOW = 0xFFFFFFu;
constexpr uint32_t FOG = 0xAEB8C6u;

struct Ctx {
  lgfx::LovyanGFX& g;
  float s;      // Kantenlänge des Symbols
  uint32_t bg;  // Hintergrund unter dem Symbol
  float t;      // Zeit in Sekunden, < 0 = statisch
  bool animated() const { return t >= 0; }
};

void sun(Ctx& c, float x, float y, float r) {
  float turn = c.animated() ? c.t * 0.25f : 0;
  float outer = c.animated() ? 1.85f + 0.08f * sinf(c.t * 2) : 1.85f;
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4 + turn;
    c.g.drawWideLine(x + cosf(a) * r * 1.4f, y + sinf(a) * r * 1.4f, x + cosf(a) * r * outer,
                     y + sinf(a) * r * outer, r * 0.11f, SUN);
  }
  c.g.fillSmoothCircle(x, y, r, SUN);
}

void moon(Ctx& c, float x, float y, float r) {
  if (c.animated()) {
    const float stars[][3] = {{-1.5f, -0.9f, 0.0f}, {1.3f, -1.2f, 2.1f}, {1.6f, 0.6f, 4.0f}};
    for (auto& st : stars) {
      float glow = 0.5f + 0.5f * sinf(c.t * 1.7f + st[2]);
      c.g.fillSmoothCircle(x + st[0] * r, y + st[1] * r, 1.5f + glow * 1.5f, mixColor(c.bg, STAR, 0.3f + 0.7f * glow));
    }
  }
  c.g.fillSmoothCircle(x, y, r, MOON);
  c.g.fillSmoothCircle(x + r * 0.5f, y - r * 0.35f, r * 0.85f, c.bg);
}

void sky(Ctx& c, bool isDay, float x, float y, float r) {
  if (isDay) {
    sun(c, x, y, r);
  } else {
    moon(c, x, y, r * 1.3f);
  }
}

// Wolke mit Breite w; (x, y) ist die Mitte der Oberkante des flachen Wolkenbodens
void cloud(Ctx& c, float x, float y, float w, uint32_t color, float drift = 1) {
  if (c.animated()) x += sinf(c.t * 0.6f) * c.s * 0.025f * drift;
  c.g.fillSmoothRoundRect(x - w * 0.5f, y, w, w * 0.3f, w * 0.15f, color);
  c.g.fillSmoothCircle(x - w * 0.2f, y + w * 0.08f, w * 0.2f, color);
  c.g.fillSmoothCircle(x + w * 0.08f, y, w * 0.27f, color);
}

// Fallende Tropfen in Spalten; len = Länge eines Tropfens, fall = Fallstrecke (Anteile von s)
void drops(Ctx& c, float cx, float top, int columns, float len, float fall, float speed) {
  const float s = c.s;
  for (int i = 0; i < columns; i++) {
    float x = cx + (i - (columns - 1) / 2.0f) * s * 0.2f;
    if (!c.animated()) {
      c.g.drawWideLine(x, top, x - s * 0.05f, top + s * len, s * 0.028f, RAIN);
      continue;
    }
    for (int k = 0; k < 2; k++) {
      float p = fmodf(c.t * speed + i * 0.37f + k * 0.5f, 1.0f);
      float y = top + p * s * fall;
      float slant = s * 0.05f * len / 0.24f;
      uint32_t color = mixColor(c.bg, RAIN, sinf(PI * p));
      c.g.drawWideLine(x - p * slant, y, x - p * slant - slant, y + s * len, s * 0.026f, color);
    }
  }
}

void flakes(Ctx& c, float cx, float top) {
  const float s = c.s;
  const float pos[][2] = {{-0.2f, 0.05f}, {0.0f, 0.12f}, {0.2f, 0.05f}, {-0.1f, 0.27f}, {0.1f, 0.27f}};
  for (int i = 0; i < 5; i++) {
    float x = cx + pos[i][0] * s, y = top + pos[i][1] * s;
    uint32_t color = SNOW;
    if (c.animated()) {
      float p = fmodf(c.t * 0.35f + i * 0.21f, 1.0f);
      y = top + p * s * 0.3f;
      x += sinf(c.t * 1.5f + i) * s * 0.03f;
      color = mixColor(c.bg, SNOW, sinf(PI * p));
    }
    c.g.fillSmoothCircle(x, y, s * 0.04f, color);
  }
}

void bolt(Ctx& c, float cx, float top) {
  const float s = c.s;
  c.g.fillTriangle(cx + 0.06f * s, top, cx - 0.1f * s, top + 0.22f * s, cx + 0.04f * s, top + 0.2f * s, SUN);
  c.g.fillTriangle(cx - 0.03f * s, top + 0.16f * s, cx + 0.1f * s, top + 0.16f * s, cx - 0.08f * s,
                   top + 0.38f * s, SUN);
}

// Doppelblitz etwa alle 3 Sekunden
bool flashing(const Ctx& c) {
  if (!c.animated()) return true;
  float p = fmodf(c.t, 3.2f);
  return p < 0.12f || (p > 0.25f && p < 0.4f);
}

}  // namespace

void drawWeatherIcon(lgfx::LovyanGFX& g, int code, bool isDay, int cx, int cy, int size, uint32_t bg, float t) {
  Ctx c{g, (float)size, bg, t};
  const float s = size;
  const uint32_t cloudColor = isDay ? CLOUD_DAY : CLOUD_NIGHT;

  switch (code) {
    case 0:
      sky(c, isDay, cx, cy, s * 0.22f);
      return;
    case 1:
      sky(c, isDay, cx - s * 0.08f, cy - s * 0.08f, s * 0.2f);
      cloud(c, cx + s * 0.18f, cy + s * 0.12f, s * 0.5f, cloudColor);
      return;
    case 2:
      sky(c, isDay, cx - s * 0.15f, cy - s * 0.15f, s * 0.17f);
      cloud(c, cx + s * 0.08f, cy + s * 0.02f, s * 0.75f, cloudColor);
      return;
    case 3:
      cloud(c, cx + s * 0.15f, cy - s * 0.12f, s * 0.6f, CLOUD_BACK, -0.6f);
      cloud(c, cx - s * 0.05f, cy + s * 0.02f, s * 0.8f, cloudColor);
      return;
    case 45:
    case 48:
      cloud(c, cx, cy - s * 0.2f, s * 0.75f, CLOUD_BACK);
      for (int i = 0; i < 3; i++) {
        float y = cy + s * (0.2f + i * 0.1f);
        float inset = (i % 2) * s * 0.08f;
        float shift = c.animated() ? sinf(t * 0.8f + i * 1.3f) * s * 0.04f : 0;
        g.drawWideLine(cx - s * 0.36f + inset + shift, y, cx + s * 0.36f - inset + shift, y, s * 0.025f, FOG);
      }
      return;
  }

  // Niederschlag: Wolke oben, darunter Tropfen, Flocken oder Blitz
  const bool showers = (code >= 80 && code <= 86);
  const bool storm = code >= 95;
  const float top = cy + s * 0.14f;
  if (showers) sky(c, isDay, cx - s * 0.2f, cy - s * 0.24f, s * 0.14f);

  if (storm) {
    bool flash = flashing(c);
    cloud(c, cx, cy - s * 0.16f, s * 0.8f, flash && c.animated() ? CLOUD_FLASH : CLOUD_STORM);
    drops(c, cx - s * 0.12f, top, 2, 0.1f, 0.24f, 1.6f);
    if (flash) bolt(c, cx + s * 0.1f, top - s * 0.04f);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    cloud(c, cx, cy - s * 0.16f, s * 0.8f, cloudColor);
    flakes(c, cx, top + s * 0.04f);
  } else if (code >= 51 && code <= 57) {
    cloud(c, cx, cy - s * 0.16f, s * 0.8f, cloudColor);
    drops(c, cx, top, 3, 0.06f, 0.26f, 1.0f);
  } else {
    cloud(c, cx, cy - s * 0.16f, s * 0.8f, cloudColor);
    drops(c, cx, top, 3, 0.1f, 0.24f, 1.6f);
  }
}
