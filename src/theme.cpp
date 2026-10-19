#include "theme.h"

#include <math.h>

namespace {

struct Sky {
  uint32_t top, bottom;
};

constexpr Sky NIGHT_CLEAR = {0x0B1630, 0x050A16};
constexpr Sky NIGHT_CLOUDY = {0x161C27, 0x0A0D14};
constexpr Sky DAWN = {0x34477A, 0xC9714A};
constexpr Sky DAY_CLEAR = {0x1D5C96, 0x4B8FC4};
constexpr Sky DAY_CLOUDY = {0x45525F, 0x66737F};
constexpr Sky DAY_RAIN = {0x2C3743, 0x485461};
constexpr Sky DAY_STORM = {0x24202F, 0x3E3851};
constexpr Sky DAY_SNOW = {0x4F5D70, 0x7D8CA0};

Sky mix(const Sky& a, const Sky& b, float t) { return {mixColor(a.top, b.top, t), mixColor(a.bottom, b.bottom, t)}; }

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

}  // namespace

uint32_t mixColor(uint32_t a, uint32_t b, float t) {
  uint32_t c = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    float ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
    c |= (uint32_t)lroundf(ca + (cb - ca) * t) << shift;
  }
  return c;
}

uint32_t skyAt(const Theme& t, int y) { return mixColor(t.top, t.bottom, y / 479.0f); }

Theme skyTheme(const Weather& w, int minute) {
  const DayForecast& today = w.day[Weather::TODAY];
  const int code = w.code;
  const bool storm = code >= 95;
  const bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  const bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
  const bool cloudy = code == 3 || code == 45 || code == 48;
  // Bedeckungsgrad 0 (klar) bis 1 (geschlossen), dämpft Dämmerungsfarben
  const float overcast = (storm || rain || snow || cloudy) ? 1.0f : (code == 2 ? 0.4f : 0.0f);

  Sky day = DAY_CLEAR;
  if (storm) {
    day = DAY_STORM;
  } else if (snow) {
    day = DAY_SNOW;
  } else if (rain) {
    day = DAY_RAIN;
  } else if (cloudy) {
    day = DAY_CLOUDY;
  } else if (code == 2) {
    day = mix(DAY_CLEAR, DAY_CLOUDY, 0.35f);
  }
  Sky night = mix(NIGHT_CLEAR, NIGHT_CLOUDY, overcast);
  Sky dawn = mix(DAWN, mix(night, day, 0.5f), overcast * 0.7f);

  // Übergänge jeweils 50 Minuten vor und nach Sonnenauf- und -untergang
  constexpr float SPAN = 50;
  Sky sky = w.isDay ? day : night;
  float daylight = w.isDay ? 1.0f : 0.0f;
  if (today.sunrise >= 0 && today.sunset > today.sunrise && minute >= 0) {
    float toRise = minute - today.sunrise, toSet = minute - today.sunset;
    if (fabsf(toRise) < SPAN) {
      sky = toRise < 0 ? mix(night, dawn, clamp01(1 + toRise / SPAN)) : mix(dawn, day, clamp01(toRise / SPAN));
      daylight = clamp01(0.5f + toRise / (2 * SPAN));
    } else if (fabsf(toSet) < SPAN) {
      sky = toSet < 0 ? mix(day, dawn, clamp01(1 + toSet / SPAN)) : mix(dawn, night, clamp01(toSet / SPAN));
      daylight = clamp01(0.5f - toSet / (2 * SPAN));
    } else {
      bool isDay = minute > today.sunrise && minute < today.sunset;
      sky = isDay ? day : night;
      daylight = isDay ? 1.0f : 0.0f;
    }
  }

  Theme t;
  t.top = sky.top;
  t.bottom = sky.bottom;
  t.night = daylight < 0.5f;
  t.text = 0xFFFFFF;
  t.dim = t.night ? 0x8E9BB3 : 0xD3DEEA;
  t.glass = t.night ? 0xFFFFFF : 0x0A1420;
  t.glassAlpha = t.night ? 18 : 60;
  return t;
}
