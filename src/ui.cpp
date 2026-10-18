#include "ui.h"

#include <math.h>
#include <time.h>

#include "fonts/font_bold.h"
#include "fonts/font_clock.h"
#include "fonts/font_huge.h"
#include "fonts/font_small.h"
#include "fonts/font_text.h"
#include "icons.h"
#include "lcd.h"

namespace {

struct Theme {
  uint32_t bgTop, bgBottom, card, text, dim;
};
constexpr Theme DAY = {0x1C4A75u, 0x0E2238u, 0x234E78u, 0xFFFFFFu, 0xA9C1DBu};
constexpr Theme NIGHT = {0x0E1830u, 0x050A14u, 0x16233Du, 0xF2F4F8u, 0x8593ABu};
constexpr uint32_t RAIN = 0x5AAEFFu;
constexpr uint32_t RAIN_BAR = 0x2F6DAAu;
constexpr uint32_t WARN = 0xFF8A65u;
constexpr uint32_t CURVE = 0xFFC23Du;

const char* const WEEKDAYS[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char* const WEEKDAYS_SHORT[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
const char* const MONTHS[] = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                              "Juli",   "August",  "September", "Oktober", "November", "Dezember"};

LGFX_Sprite& canvas = lcd::canvas();
const uint8_t* currentFont = nullptr;

void font(const uint8_t* f) {
  if (f != currentFont) {
    canvas.loadFont(f);
    currentFont = f;
  }
}

void text(const char* s, int x, int y, const uint8_t* f, uint32_t color, textdatum_t datum) {
  font(f);
  canvas.setTextColor(color);
  canvas.setTextDatum(datum);
  canvas.drawString(s, x, y);
}

int textWidth(const char* s, const uint8_t* f) {
  font(f);
  return canvas.textWidth(s);
}

void formatTemp(char* buf, size_t len, float t) { snprintf(buf, len, "%d°", (int)lroundf(t)); }

const char* compass(int deg) {
  static const char* const DIRS[] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};
  return DIRS[((deg + 22) % 360) / 45];
}

// Farbe des Hintergrundverlaufs in Höhe y
uint32_t backgroundAt(const Theme& t, int y) {
  uint32_t c = 0;
  for (int shift = 0; shift <= 16; shift += 8) {
    int a = (t.bgTop >> shift) & 0xFF, b = (t.bgBottom >> shift) & 0xFF;
    c |= (uint32_t)(a + (b - a) * y / 479) << shift;
  }
  return c;
}

void background(const Theme& t) {
  canvas.fillGradientRect(0, 0, lcd::WIDTH, lcd::HEIGHT, t.bgTop, t.bgBottom, lgfx::VLINEAR);
}

void header(const Location& loc, const Theme& t, const char* status, bool warn) {
  text(strlen(loc.city) ? loc.city : "Wetter", 24, 12, font_bold, t.text, top_left);

  struct tm now;
  char clock[8] = "--:--";
  if (getLocalTime(&now, 0)) {
    char date[48];
    snprintf(date, sizeof(date), "%s, %d. %s %d", WEEKDAYS[now.tm_wday], now.tm_mday, MONTHS[now.tm_mon],
             now.tm_year + 1900);
    text(date, 26, 52, font_small, t.dim, top_left);
    strftime(clock, sizeof(clock), "%H:%M", &now);
  }
  text(clock, 776, 48, font_clock, t.text, baseline_right);
  text(status, 776, 56, font_small, warn ? WARN : t.dim, top_right);
}

void current(const Weather& w, const Theme& t) {
  drawWeatherIcon(canvas, w.code, w.isDay, 120, 172, 170, backgroundAt(t, 150));

  char buf[32];
  formatTemp(buf, sizeof(buf), w.temp);
  text(buf, 222, 204, font_huge, t.text, baseline_left);
  text(weatherText(w.code), 228, 222, font_text, t.dim, top_left);

  const int x = 500, y = 88, wd = 284, h = 170;
  canvas.fillSmoothRoundRect(x, y, wd, h, 14, t.card);

  char rows[5][24];
  formatTemp(rows[0], sizeof(rows[0]), w.feels);
  snprintf(rows[1], sizeof(rows[1]), "%d %%", w.humidity);
  snprintf(rows[2], sizeof(rows[2]), "%d km/h %s", (int)lroundf(w.wind), compass(w.windDir));
  snprintf(rows[3], sizeof(rows[3]), "%d hPa", (int)lroundf(w.pressure));
  snprintf(rows[4], sizeof(rows[4]), "%s – %s", w.sunrise, w.sunset);
  const char* const labels[] = {"Gefühlt", "Luftfeuchte", "Wind", "Luftdruck", "Sonne"};
  for (int i = 0; i < 5; i++) {
    int ry = y + 21 + i * 32;
    text(labels[i], x + 18, ry, font_small, t.dim, middle_left);
    text(rows[i], x + wd - 18, ry, font_text, t.text, middle_right);
  }
}

void hourly(const Weather& w, const Theme& t) {
  if (w.hours < 2) return;
  canvas.fillSmoothRoundRect(16, 270, 768, 92, 14, t.card);

  const int x0 = 52, x1 = 748, yTop = 302, yBottom = 330, barBase = 340;
  float lo = w.hTemp[0], hi = w.hTemp[0];
  for (int i = 1; i < w.hours; i++) {
    lo = fminf(lo, w.hTemp[i]);
    hi = fmaxf(hi, w.hTemp[i]);
  }
  if (hi - lo < 4) {
    float mid = (hi + lo) / 2;
    lo = mid - 2;
    hi = mid + 2;
  }

  int px[Weather::HOURS], py[Weather::HOURS];
  for (int i = 0; i < w.hours; i++) {
    px[i] = x0 + (x1 - x0) * i / (w.hours - 1);
    py[i] = yBottom - (int)lroundf((w.hTemp[i] - lo) / (hi - lo) * (yBottom - yTop));
    if (w.hPrecip[i] > 0) {
      int bh = w.hPrecip[i] * 40 / 100;
      canvas.fillRect(px[i] - 10, barBase - bh, 20, bh, RAIN_BAR);
    }
  }
  for (int i = 1; i < w.hours; i++) canvas.drawWideLine(px[i - 1], py[i - 1], px[i], py[i], 1.5f, CURVE);

  char buf[16];
  for (int i = 0; i < w.hours; i += 3) {
    canvas.fillSmoothCircle(px[i], py[i], 4, CURVE);
    formatTemp(buf, sizeof(buf), w.hTemp[i]);
    text(buf, px[i], py[i] - 8, font_small, t.text, baseline_center);
    if (i == 0) {
      strcpy(buf, "Jetzt");
    } else {
      snprintf(buf, sizeof(buf), "%02d:00", w.hHour[i]);
    }
    text(buf, px[i], 351, font_small, t.dim, middle_center);
  }
}

void drop(int x, int y, uint32_t color) {
  canvas.fillSmoothCircle(x, y + 3, 4, color);
  canvas.fillTriangle(x - 4, y + 2, x + 4, y + 2, x, y - 6, color);
}

void daily(const Weather& w, const Theme& t) {
  const int count = w.days, gap = 8, y = 370, h = 102;
  const int tileW = (768 - gap * (count - 1)) / count;
  char buf[16];
  for (int i = 0; i < count; i++) {
    const DayForecast& d = w.day[i];
    int x = 16 + i * (tileW + gap);
    int cx = x + tileW / 2;
    canvas.fillSmoothRoundRect(x, y, tileW, h, 14, t.card);

    text(i == 0 ? "Heute" : WEEKDAYS_SHORT[d.weekday], cx, y + 16, font_small, t.text, middle_center);
    drawWeatherIcon(canvas, d.code, true, x + 34, y + 52, 50, t.card);
    formatTemp(buf, sizeof(buf), d.tMax);
    text(buf, x + tileW - 12, y + 44, font_text, t.text, middle_right);
    formatTemp(buf, sizeof(buf), d.tMin);
    text(buf, x + tileW - 12, y + 68, font_small, t.dim, middle_right);

    snprintf(buf, sizeof(buf), "%d %%", d.precip);
    int tw = textWidth(buf, font_small);
    drop(cx - tw / 2 - 6, y + 88, RAIN);
    text(buf, cx + 6, y + 89, font_small, RAIN, middle_center);
  }
}

}  // namespace

void uiStatus(const char* title, const char* detail) {
  background(NIGHT);
  text(title, 400, 222, font_bold, NIGHT.text, middle_center);
  text(detail, 400, 266, font_text, NIGHT.dim, middle_center);
  lcd::show();
}

void uiWeather(const Location& loc, const Weather& w, const char* status, bool warn) {
  const Theme& t = w.isDay ? DAY : NIGHT;
  background(t);
  header(loc, t, status, warn);
  current(w, t);
  hourly(w, t);
  daily(w, t);
  lcd::show();
}
