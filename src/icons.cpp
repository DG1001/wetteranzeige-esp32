#include "icons.h"

#include <math.h>

static constexpr uint32_t SUN = 0xFFC23Du;
static constexpr uint32_t MOON = 0xEDE7C8u;
static constexpr uint32_t CLOUD_DAY = 0xF2F5F9u;
static constexpr uint32_t CLOUD_NIGHT = 0xC5CEDBu;
static constexpr uint32_t CLOUD_BACK = 0x9AA6B8u;
static constexpr uint32_t CLOUD_STORM = 0x7D8797u;
static constexpr uint32_t RAIN = 0x5AAEFFu;
static constexpr uint32_t SNOW = 0xFFFFFFu;
static constexpr uint32_t FOG = 0xAEB8C6u;

static void sun(lgfx::LovyanGFX& g, float x, float y, float r) {
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4;
    g.drawWideLine(x + cosf(a) * r * 1.4f, y + sinf(a) * r * 1.4f, x + cosf(a) * r * 1.85f,
                   y + sinf(a) * r * 1.85f, r * 0.11f, SUN);
  }
  g.fillSmoothCircle(x, y, r, SUN);
}

static void moon(lgfx::LovyanGFX& g, float x, float y, float r, uint32_t bg) {
  g.fillSmoothCircle(x, y, r, MOON);
  g.fillSmoothCircle(x + r * 0.5f, y - r * 0.35f, r * 0.85f, bg);
}

static void sky(lgfx::LovyanGFX& g, bool isDay, float x, float y, float r, uint32_t bg) {
  if (isDay) {
    sun(g, x, y, r);
  } else {
    moon(g, x, y, r * 1.3f, bg);
  }
}

// Wolke mit Breite w; (x, y) ist die Mitte der Oberkante des flachen Wolkenbodens
static void cloud(lgfx::LovyanGFX& g, float x, float y, float w, uint32_t color) {
  g.fillSmoothRoundRect(x - w * 0.5f, y, w, w * 0.3f, w * 0.15f, color);
  g.fillSmoothCircle(x - w * 0.2f, y + w * 0.08f, w * 0.2f, color);
  g.fillSmoothCircle(x + w * 0.08f, y, w * 0.27f, color);
}

static void drops(lgfx::LovyanGFX& g, float cx, float top, float s, int count, float len) {
  for (int i = 0; i < count; i++) {
    float x = cx + (i - (count - 1) / 2.0f) * s * 0.2f;
    g.drawWideLine(x, top, x - s * 0.05f, top + s * len, s * 0.028f, RAIN);
  }
}

static void flakes(lgfx::LovyanGFX& g, float cx, float top, float s) {
  const float pos[][2] = {{-0.2f, 0.05f}, {0.0f, 0.12f}, {0.2f, 0.05f}, {-0.1f, 0.27f}, {0.1f, 0.27f}};
  for (auto& p : pos) g.fillSmoothCircle(cx + p[0] * s, top + p[1] * s, s * 0.04f, SNOW);
}

static void bolt(lgfx::LovyanGFX& g, float cx, float top, float s) {
  g.fillTriangle(cx + 0.06f * s, top, cx - 0.1f * s, top + 0.22f * s, cx + 0.04f * s, top + 0.2f * s, SUN);
  g.fillTriangle(cx - 0.03f * s, top + 0.16f * s, cx + 0.1f * s, top + 0.16f * s, cx - 0.08f * s,
                 top + 0.42f * s, SUN);
}

void drawWeatherIcon(lgfx::LovyanGFX& g, int code, bool isDay, int cx, int cy, int size, uint32_t bg) {
  const float s = size;
  const uint32_t cloudColor = isDay ? CLOUD_DAY : CLOUD_NIGHT;

  switch (code) {
    case 0:
      sky(g, isDay, cx, cy, s * 0.22f, bg);
      return;
    case 1:
      sky(g, isDay, cx - s * 0.08f, cy - s * 0.08f, s * 0.2f, bg);
      cloud(g, cx + s * 0.18f, cy + s * 0.12f, s * 0.5f, cloudColor);
      return;
    case 2:
      sky(g, isDay, cx - s * 0.15f, cy - s * 0.15f, s * 0.17f, bg);
      cloud(g, cx + s * 0.08f, cy + s * 0.02f, s * 0.75f, cloudColor);
      return;
    case 3:
      cloud(g, cx + s * 0.15f, cy - s * 0.12f, s * 0.6f, CLOUD_BACK);
      cloud(g, cx - s * 0.05f, cy + s * 0.02f, s * 0.8f, cloudColor);
      return;
    case 45:
    case 48:
      cloud(g, cx, cy - s * 0.2f, s * 0.75f, CLOUD_BACK);
      for (int i = 0; i < 3; i++) {
        float y = cy + s * (0.2f + i * 0.1f);
        float inset = (i % 2) * s * 0.08f;
        g.drawWideLine(cx - s * 0.36f + inset, y, cx + s * 0.36f - inset, y, s * 0.025f, FOG);
      }
      return;
  }

  // Niederschlag: Wolke oben, darunter Tropfen, Flocken oder Blitz
  const bool showers = (code >= 80 && code <= 86);
  const bool storm = code >= 95;
  const float top = cy + s * 0.14f;
  if (showers) sky(g, isDay, cx - s * 0.2f, cy - s * 0.24f, s * 0.14f, bg);
  cloud(g, cx, cy - s * 0.16f, s * 0.8f, storm ? CLOUD_STORM : cloudColor);

  if (storm) {
    drops(g, cx - s * 0.12f, top, s, 2, 0.12f);
    bolt(g, cx + s * 0.1f, top - s * 0.04f, s);
  } else if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    flakes(g, cx, top + s * 0.04f, s);
  } else if (code >= 51 && code <= 57) {
    drops(g, cx, top, s, 3, 0.1f);
  } else {
    drops(g, cx, top, s, 3, 0.24f);
  }
}
