#include <Arduino.h>
#include <math.h>
#include <time.h>

#include "astro.h"
#include "icons.h"
#include "ui_common.h"
#include "ui_pages.h"

namespace ui {

namespace {

const char* compass(int deg) {
  static const char* const DIRS[] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};
  return DIRS[((deg + 22) % 360) / 45];
}

void drop(int x, int y, uint32_t color) {
  canvas.fillSmoothCircle(x, y + 3, 4, color);
  canvas.fillTriangle(x - 4, y + 2, x + 4, y + 2, x, y - 6, color);
}

void formatMinutes(char* buf, size_t len, int minutes) {
  if (minutes < 0) {
    strlcpy(buf, "--:--", len);
  } else {
    snprintf(buf, len, "%02d:%02d", minutes / 60, minutes % 60);
  }
}

// Stündliches Diagramm: Symbole, Temperatur, Regen, Wind und Uhrzeiten für count Stunden ab start.
// Tageswechsel werden mit Linie markiert und ab labelMinX in Höhe labelY beschriftet.
void hourChart(const Weather& w, const Theme& t, int start, int count, int x, int y, int width, int height,
               int labelY, int labelMinX) {
  if (count < 2) return;
  const int x0 = x + 128, x1 = x + width - 24;
  const int iconY = y + 24, tempTop = y + 70, tempBottom = y + height - 146;
  const int rainTop = tempBottom + 24, rainBottom = rainTop + 44;
  const int windY = rainBottom + 18, gustY = windY + 22, hourY = y + height - 14;
  const int every = 3;
  auto px = [&](int i) { return x0 + (x1 - x0) * i / (count - 1); };

  // Beschriftung der Zeilen
  text("Regen", x + 18, (rainTop + rainBottom) / 2, FONT_SMALL, t.dim, middle_left);
  text("Wind km/h", x + 18, windY, FONT_SMALL, t.dim, middle_left);
  text("Böen km/h", x + 18, gustY, FONT_SMALL, t.dim, middle_left);
  text("Uhr", x + 18, hourY, FONT_SMALL, t.dim, middle_left);

  // Tageswechsel als Linie mit Wochentag
  for (int i = 0; i < count; i++) {
    int h = start + i;
    if (w.hHour[h] != 0 || i == 0) continue;
    const DayForecast& d = w.day[w.hDay[h]];
    canvas.drawFastVLine(px(i), labelY, y + height - 10 - labelY, mixColor(skyAt(t, y + height / 2), t.dim, 0.35f));
    char label[24];
    snprintf(label, sizeof(label), "%s %d.%d.", WEEKDAYS_SHORT[d.weekday], d.mday, d.month);
    // Rechts von der Linie; am rechten Rand links davon
    int lw = textWidth(label, FONT_SMALL);
    int lx = px(i) + 6 + lw <= x + width - 10 ? px(i) + 6 : px(i) - 6 - lw;
    if (lx < labelMinX) continue;
    text(label, lx, labelY, FONT_SMALL, t.dim);
  }

  // Regenwahrscheinlichkeit als heller, Regenmenge als kräftiger Balken (4 mm = volle Höhe)
  const int barW = max(4, (x1 - x0) / (count - 1) - 3);
  for (int i = 0; i < count; i++) {
    int h = start + i;
    int probH = w.hProb[h] * (rainBottom - rainTop) / 100;
    if (probH > 0) canvas.fillRect(px(i) - barW / 2, rainBottom - probH, barW, probH, RAIN_DIM);
    int mmH = (int)lroundf(fminf(w.hPrecip[h], 4.0f) / 4.0f * (rainBottom - rainTop));
    if (w.hPrecip[h] >= 0.1f) mmH = max(mmH, 2);
    if (mmH > 0) canvas.fillRect(px(i) - barW / 2, rainBottom - mmH, barW, mmH, RAIN);
  }
  canvas.drawFastHLine(x0 - barW, rainBottom, x1 - x0 + 2 * barW, mixColor(skyAt(t, rainBottom), t.dim, 0.4f));
  bool anyRain = false;
  for (int i = 0; i < count; i++) anyRain |= w.hProb[start + i] > 0 || w.hPrecip[start + i] >= 0.1f;
  if (!anyRain) text("kein Regen erwartet", (x0 + x1) / 2, (rainTop + rainBottom) / 2, FONT_SMALL, t.dim, middle_center);

  float temps[Weather::HOURS];
  for (int i = 0; i < count; i++) temps[i] = w.hTemp[start + i];
  tempCurve(count, temps, x0, x1, tempTop, tempBottom, every, t.text);

  char buf[16];
  for (int i = 0; i < count; i += every) {
    int h = start + i;
    drawWeatherIcon(canvas, w.hCode[h], w.hIsDay[h], px(i), iconY, 36, skyAt(t, iconY));
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(w.hWind[h]));
    text(buf, px(i), windY, FONT_SMALL, t.text, middle_center);
    snprintf(buf, sizeof(buf), "%d", (int)lroundf(w.hGust[h]));
    text(buf, px(i), gustY, FONT_SMALL, t.dim, middle_center);
    snprintf(buf, sizeof(buf), "%02d", w.hHour[h]);
    text(buf, px(i), hourY, FONT_SMALL, t.dim, middle_center);
  }
}

// Mond mit Phase: beleuchteter Teil rechts bei zunehmendem, links bei abnehmendem Mond
void drawMoon(int cx, int cy, int r, float age, uint32_t dark) {
  constexpr uint32_t LIT = 0xEDE7C8u;
  canvas.fillSmoothCircle(cx, cy, r, dark);
  float k = cosf(2 * PI * age / 29.530589f);
  bool waxing = age < 29.530589f / 2;
  for (int dy = -r; dy <= r; dy++) {
    float half = sqrtf((float)r * r - (float)dy * dy);
    float from = waxing ? half * k : -half;
    float to = waxing ? half : -half * k;
    int x0 = lroundf(cx + from), x1 = lroundf(cx + to);
    if (x1 > x0) canvas.drawFastHLine(x0, cy + dy, x1 - x0, LIT);
  }
  canvas.drawCircle(cx, cy, r, mixColor(dark, LIT, 0.4f));
}

}  // namespace

// ---------------------------------------------------------------- Übersicht

void overview(const Model& m, const Theme& t) {
  const Weather& w = m.weather;
  char buf[48];
  formatTemp(buf, sizeof(buf), w.temp);
  text(buf, 222, 204, FONT_HUGE, t.text, baseline_left);
  text(weatherText(w.code), 228, 212, FONT_TEXT, t.dim);
  bool rain = nowcastText(w, buf, sizeof(buf));
  if (buf[0]) {
    int x = 228;
    if (rain) {
      drop(x + 5, 254, RAIN);
      x += 16;
    }
    text(buf, x, 244, FONT_SMALL, rain ? RAIN : t.dim);
  }

  const int x = 500, y = 88, wd = 284, h = 170;
  glass(t, x, y, wd, h);
  const DayForecast& today = w.day[Weather::TODAY];
  char rows[5][24], rise[8], set[8];
  formatTemp(rows[0], sizeof(rows[0]), w.feels);
  snprintf(rows[1], sizeof(rows[1]), "%d %%", w.humidity);
  snprintf(rows[2], sizeof(rows[2]), "%d km/h %s", (int)lroundf(w.wind), compass(w.windDir));
  snprintf(rows[3], sizeof(rows[3]), "%d hPa", (int)lroundf(w.pressure));
  formatMinutes(rise, sizeof(rise), today.sunrise);
  formatMinutes(set, sizeof(set), today.sunset);
  snprintf(rows[4], sizeof(rows[4]), "%s – %s", rise, set);
  const char* const labels[] = {"Gefühlt", "Luftfeuchte", "Wind", "Luftdruck", "Sonne"};
  for (int i = 0; i < 5; i++) {
    int ry = y + 21 + i * 32;
    text(labels[i], x + 18, ry, FONT_SMALL, t.dim, middle_left);
    text(rows[i], x + wd - 18, ry, FONT_TEXT, t.text, middle_right);
  }

  // Nächste 24 Stunden
  const int count = min(24, w.hours - w.nowHour);
  if (count >= 2) {
    glass(t, HOURLY_X, HOURLY_Y, HOURLY_W, HOURLY_H);
    const int x0 = 52, x1 = 748, barBase = 340;
    for (int i = 0; i < count; i++) {
      int px = x0 + (x1 - x0) * i / (count - 1);
      int prob = w.hProb[w.nowHour + i];
      if (prob > 0) {
        int bh = prob * 40 / 100;
        canvas.fillRect(px - 10, barBase - bh, 20, bh, RAIN_DIM);
      }
    }
    tempCurve(count, w.hTemp + w.nowHour, x0, x1, 302, 330, 3, t.text);
    for (int i = 0; i < count; i += 3) {
      int px = x0 + (x1 - x0) * i / (count - 1);
      if (i == 0) {
        strcpy(buf, "Jetzt");
      } else {
        snprintf(buf, sizeof(buf), "%02d:00", w.hHour[w.nowHour + i]);
      }
      text(buf, px, 351, FONT_SMALL, t.dim, middle_center);
    }
  }

  // Tage ab heute
  const int days = min(TILES, w.days - Weather::TODAY);
  for (int i = 0; i < days; i++) {
    const DayForecast& d = w.day[Weather::TODAY + i];
    int tx = TILE_X + i * (TILE_W + TILE_GAP), cx = tx + TILE_W / 2;
    glass(t, tx, TILE_Y, TILE_W, TILE_H);
    text(i == 0 ? "Heute" : WEEKDAYS_SHORT[d.weekday], cx, TILE_Y + 16, FONT_SMALL, t.text, middle_center);
    drawWeatherIcon(canvas, d.code, true, tx + 34, TILE_Y + 52, 50, skyAt(t, TILE_Y + 52));
    formatTemp(buf, sizeof(buf), d.tMax);
    text(buf, tx + TILE_W - 12, TILE_Y + 44, FONT_TEXT, t.text, middle_right);
    formatTemp(buf, sizeof(buf), d.tMin);
    text(buf, tx + TILE_W - 12, TILE_Y + 68, FONT_SMALL, t.dim, middle_right);
    snprintf(buf, sizeof(buf), "%d %%", d.precipProb);
    int tw = textWidth(buf, FONT_SMALL);
    drop(cx - tw / 2 - 6, TILE_Y + 88, RAIN);
    text(buf, cx + 6, TILE_Y + 89, FONT_SMALL, RAIN, middle_center);
  }
}

// ---------------------------------------------------------------- 48 Stunden

void hours(const Model& m, const Theme& t) {
  const Weather& w = m.weather;
  glass(t, 16, 88, 768, 384);
  text("Nächste 48 Stunden", 34, 102, FONT_TITLE, t.text);
  int count = min(48, w.hours - w.nowHour);
  hourChart(w, t, w.nowHour, count, 16, 118, 768, 352, 104, 34 + textWidth("Nächste 48 Stunden", FONT_TITLE) + 16);
}

// ---------------------------------------------------------------- Tagesansicht

void day(const Model& m, const Theme& t) {
  const Weather& w = m.weather;
  const DayForecast& d = w.day[m.day];
  char buf[96], rise[8], set[8];

  glass(t, 16, 88, 768, 112);
  drawWeatherIcon(canvas, d.code, true, 76, 144, 84, skyAt(t, 144));
  snprintf(buf, sizeof(buf), "%s, %d. %s", m.day == Weather::TODAY ? "Heute" : WEEKDAYS[d.weekday], d.mday,
           MONTHS[d.month - 1]);
  text(buf, 136, 100, FONT_BOLD, t.text);
  text(weatherText(d.code), 138, 138, FONT_TEXT, t.dim);
  formatMinutes(rise, sizeof(rise), d.sunrise);
  formatMinutes(set, sizeof(set), d.sunset);
  char mm[12];
  formatDecimal(mm, sizeof(mm), d.precipSum, 1);
  snprintf(buf, sizeof(buf), "Regen %s mm (%d %%)  ·  Böen %d km/h  ·  UV %d  ·  Sonne %s–%s", mm, d.precipProb,
           (int)lroundf(d.gustMax), (int)lroundf(d.uvMax), rise, set);
  text(buf, 138, 172, FONT_SMALL, t.dim);
  char hi[8], lo[8];
  formatTemp(hi, sizeof(hi), d.tMax);
  formatTemp(lo, sizeof(lo), d.tMin);
  text(hi, 764, 100, FONT_BOLD, t.text, top_right);
  text(lo, 764, 140, FONT_TEXT, t.dim, top_right);

  // Stunden dieses Tages
  int start = -1, count = 0;
  for (int i = 0; i < w.hours; i++) {
    if (w.hDay[i] != m.day) continue;
    if (start < 0) start = i;
    count++;
  }
  glass(t, 16, 208, 768, 264);
  if (start >= 0) hourChart(w, t, start, count, 16, 208, 768, 264, 214, 0);

  // Hinweis auf Wischen zum Nachbartag
  if (m.day > Weather::TODAY) {
    canvas.drawWideLine(10, 330, 4, 340, 1.5f, t.dim);
    canvas.drawWideLine(4, 340, 10, 350, 1.5f, t.dim);
  }
  if (m.day < w.days - 1) {
    canvas.drawWideLine(790, 330, 796, 340, 1.5f, t.dim);
    canvas.drawWideLine(796, 340, 790, 350, 1.5f, t.dim);
  }
}

// ---------------------------------------------------------------- Himmel & Luft

void skyAir(const Model& m, const Theme& t) {
  const Weather& w = m.weather;
  const Air& air = m.air;
  const DayForecast& today = w.day[Weather::TODAY];
  char buf[64];

  // Sonne
  glass(t, 16, 88, 376, 384);
  text("Sonne", 34, 102, FONT_TITLE, t.text);
  const int cx = 204, cy = 250, rx = 146, ry = 104;
  int minute = minuteOfDay();
  float f = today.sunset > today.sunrise ? (minute - today.sunrise) / (float)(today.sunset - today.sunrise) : -1;
  uint32_t arcDim = mixColor(skyAt(t, cy), t.dim, 0.45f);
  canvas.drawFastHLine(34, cy, 340, arcDim);
  for (int i = 0; i < 60; i++) {
    float a0 = PI * (1 - i / 60.0f), a1 = PI * (1 - (i + 1) / 60.0f);
    bool passed = f >= 0 && (i + 0.5f) / 60 <= f;
    if (!passed && i % 2) continue;  // Rest gestrichelt
    canvas.drawWideLine(cx + rx * cosf(a0), cy - ry * sinf(a0), cx + rx * cosf(a1), cy - ry * sinf(a1),
                        passed ? 1.8f : 1.0f, passed ? CURVE : arcDim);
  }
  formatMinutes(buf, sizeof(buf), today.sunrise);
  text(buf, cx - rx, cy + 10, FONT_TEXT, t.text, top_center);
  text("Aufgang", cx - rx, cy + 38, FONT_SMALL, t.dim, top_center);
  formatMinutes(buf, sizeof(buf), today.sunset);
  text(buf, cx + rx, cy + 10, FONT_TEXT, t.text, top_center);
  text("Untergang", cx + rx, cy + 38, FONT_SMALL, t.dim, top_center);
  if (f >= 0 && f <= 1) {
    float a = PI * (1 - f);
    int sx = cx + rx * cosf(a), sy = cy - ry * sinf(a);
    canvas.fillSmoothCircle(sx, sy, 17, mixColor(skyAt(t, sy), CURVE, 0.3f));
    canvas.fillSmoothCircle(sx, sy, 11, CURVE);
  } else {
    const DayForecast& tomorrow = w.day[min(Weather::TODAY + 1, w.days - 1)];
    char rise[8];
    formatMinutes(rise, sizeof(rise), tomorrow.sunrise);
    snprintf(buf, sizeof(buf), "Aufgang morgen %s", rise);
    text(buf, cx, cy - 66, FONT_SMALL, t.dim, middle_center);
  }
  int dl = lroundf(today.daylight / 60);
  snprintf(buf, sizeof(buf), "Tageslänge %d:%02d h", dl / 60, dl % 60);
  text(buf, cx, cy - 34, FONT_TEXT, t.text, middle_center);
  int diff = lroundf((today.daylight - w.day[Weather::TODAY - 1].daylight) / 60);
  if (diff == 0) {
    strlcpy(buf, "so lang wie gestern", sizeof(buf));
  } else {
    snprintf(buf, sizeof(buf), "%d min %s als gestern", abs(diff), diff > 0 ? "länger" : "kürzer");
  }
  text(buf, cx, cy - 10, FONT_SMALL, t.dim, middle_center);

  // Mond
  canvas.drawFastHLine(34, 330, 340, mixColor(skyAt(t, 330), t.dim, 0.3f));
  text("Mond", 34, 342, FONT_TITLE, t.text);
  time_t now = time(nullptr);
  MoonPhase moon = moonPhase(now);
  drawMoon(84, 414, 44, moon.age, mixColor(skyAt(t, 414), 0x5A6478, 0.35f));
  text(moon.name, 150, 372, FONT_TEXT, t.text);
  snprintf(buf, sizeof(buf), "%d %% beleuchtet", (int)lroundf(moon.illumination * 100));
  text(buf, 150, 402, FONT_SMALL, t.dim);
  struct tm tm;
  time_t full = moon.nextFull + w.utcOffset, fresh = moon.nextNew + w.utcOffset;
  gmtime_r(&full, &tm);
  snprintf(buf, sizeof(buf), "Vollmond %s %d.%d.", WEEKDAYS_SHORT[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
  text(buf, 150, 426, FONT_SMALL, t.dim);
  gmtime_r(&fresh, &tm);
  snprintf(buf, sizeof(buf), "Neumond %s %d.%d.", WEEKDAYS_SHORT[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
  text(buf, 150, 448, FONT_SMALL, t.dim);

  // UV-Index
  glass(t, 408, 88, 376, 384);
  text("UV-Index", 426, 102, FONT_TITLE, t.text);
  static const uint32_t UV_COLORS[] = {0x4CAF50u, 0xFDD835u, 0xFB8C00u, 0xE53935u, 0x9C27B0u};
  static const char* const UV_NAMES[] = {"niedrig", "mäßig", "hoch", "sehr hoch", "extrem"};
  auto uvClass = [](float uv) { return uv < 3 ? 0 : uv < 6 ? 1 : uv < 8 ? 2 : uv < 11 ? 3 : 4; };
  int uvNow = uvClass(w.uv);
  snprintf(buf, sizeof(buf), "%.0f", w.uv);
  text(buf, 426, 128, FONT_BOLD, t.text);
  int nx = 426 + textWidth(buf, FONT_BOLD) + 12;
  text(UV_NAMES[uvNow], nx, 134, FONT_TEXT, UV_COLORS[uvNow]);
  snprintf(buf, sizeof(buf), "max. heute %.0f", today.uvMax);
  text(buf, 766, 136, FONT_SMALL, t.dim, top_right);
  // Skala 0 bis 11+ mit Markierung
  const int sx0 = 426, sx1 = 766, sy = 172;
  for (int i = 0; i < 11; i++) {
    int a = sx0 + (sx1 - sx0) * i / 11, b = sx0 + (sx1 - sx0) * (i + 1) / 11;
    canvas.fillRect(a + 1, sy, b - a - 2, 8, UV_COLORS[uvClass(i + 0.5f)]);
  }
  int mx = sx0 + (int)((sx1 - sx0) * fminf(w.uv, 11.0f) / 11);
  canvas.fillTriangle(mx, sy - 2, mx - 6, sy - 10, mx + 6, sy - 10, t.text);

  // Luftqualität
  text("Luftqualität", 426, 198, FONT_TITLE, t.text);
  if (air.valid) {
    static const uint32_t AQI_COLORS[] = {0x50F0E6u, 0x50CCAAu, 0xF0E641u, 0xFF5050u, 0xC0305Au, 0x9C27B0u};
    static const char* const AQI_NAMES[] = {"gut", "akzeptabel", "mäßig", "schlecht", "sehr schlecht", "extrem schlecht"};
    int c = min(5, air.aqi / 20 - (air.aqi % 20 == 0 && air.aqi > 0 ? 1 : 0));
    canvas.fillSmoothCircle(436, 238, 8, AQI_COLORS[c]);
    snprintf(buf, sizeof(buf), "%s (%d)", AQI_NAMES[c], air.aqi);
    text(buf, 454, 238, FONT_TEXT, t.text, middle_left);
    text("µg/m³", 766, 204, FONT_SMALL, t.dim, top_right);
    snprintf(buf, sizeof(buf), "PM2,5 %.0f · PM10 %.0f · O3 %.0f · NO2 %.0f", air.pm25, air.pm10, air.ozone, air.no2);
    text(buf, 426, 260, FONT_SMALL, t.dim);

    text("Pollen heute", 426, 296, FONT_TITLE, t.text);
    static const char* const LEVELS[] = {"keine", "gering", "mittel", "hoch"};
    static const uint32_t LEVEL_COLORS[] = {0x4CAF50u, 0xFDD835u, 0xFB8C00u, 0xE53935u};
    for (int p = 0; p < Air::POLLEN; p++) {
      int py = 336 + p * 27;
      int level = pollenLevel(p, air.pollenToday[p]);
      text(POLLEN_NAMES[p], 426, py, FONT_SMALL, t.text, middle_left);
      for (int s = 0; s < 3; s++) {
        uint32_t c = s < level ? LEVEL_COLORS[level] : mixColor(skyAt(t, py), t.dim, 0.25f);
        canvas.fillSmoothRoundRect(552 + s * 40, py - 5, 34, 10, 4, c);
      }
      text(LEVELS[level], 766, py, FONT_SMALL, level ? t.text : t.dim, middle_right);
    }
  } else {
    text("Keine Daten", 426, 238, FONT_SMALL, t.dim, middle_left);
  }
}

// ---------------------------------------------------------------- Warnungen

void warnings(const Model& m, const Theme& t) {
  const Warnings& ws = m.warnings;
  constexpr int LINE = 23, TEXT_W = 726, BOTTOM = 472, GAP = 8;
  int y = 88;
  for (int i = 0; i < ws.count && y < BOTTOM - 80; i++) {
    const Warning& w = ws.item[i];
    int descLines = wrapLines(w.description, TEXT_W, FONT_SMALL);
    int instLines = w.instruction[0] ? wrapLines(w.instruction, TEXT_W, FONT_SMALL) : 0;
    int h = 72 + descLines * LINE + (instLines ? 8 + instLines * LINE : 0) + 12;
    // Die letzte sichtbare Karte füllt den Rest der Seite, ihr Text wird dann gekürzt
    bool more = i + 1 < ws.count;
    if (y + h > BOTTOM || (more && y + h + GAP + 80 > BOTTOM)) h = BOTTOM - y;

    glass(t, 16, y, 768, h);
    canvas.fillSmoothRoundRect(24, y + 12, 6, h - 24, 3, warnColor(w.level));
    text(w.headline, 42, y + 14, FONT_TITLE, t.text);
    text(w.period, 42, y + 44, FONT_SMALL, t.dim);
    int ty = wrapText(w.description, 42, y + 72, TEXT_W, FONT_SMALL, t.text, LINE, y + h - 8);
    if (instLines && ty + LINE + 8 <= y + h - 8) wrapText(w.instruction, 42, ty + 8, TEXT_W, FONT_SMALL, t.dim, LINE, y + h - 8);
    y += h + GAP;
  }
  if (ws.count == 0) text("Keine Warnungen", 400, 270, FONT_TEXT, t.dim, middle_center);
}

}  // namespace ui
