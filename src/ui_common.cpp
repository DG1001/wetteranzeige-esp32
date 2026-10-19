#include "ui_common.h"

#include <Arduino.h>
#include <math.h>
#include <time.h>

#include "fonts/font_bold.h"
#include "fonts/font_clock.h"
#include "fonts/font_huge.h"
#include "fonts/font_small.h"
#include "fonts/font_text.h"
#include "fonts/font_title.h"
#include "lcd.h"

namespace ui {

LGFX_Sprite& canvas = lcd::canvas();

const uint8_t* const FONT_SMALL = font_small;
const uint8_t* const FONT_TEXT = font_text;
const uint8_t* const FONT_TITLE = font_title;
const uint8_t* const FONT_BOLD = font_bold;
const uint8_t* const FONT_CLOCK = font_clock;
const uint8_t* const FONT_HUGE = font_huge;

const char* const WEEKDAYS[7] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char* const WEEKDAYS_SHORT[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
const char* const MONTHS[12] = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                                "Juli",   "August",  "September", "Oktober", "November", "Dezember"};

namespace {
const uint8_t* currentFont = nullptr;
}

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

namespace {

// Gemeinsamer Umbruch für Zeichnen und Messen; liefert die Anzahl der Zeilen
int wrap(const char* s, int x, int y, int w, const uint8_t* f, uint32_t color, int lineHeight, int maxLines,
         bool draw) {
  char line[160];
  const char* p = s;
  int lines = 0;
  while (*p && lines < maxLines) {
    // Längste Zeile suchen, die in w passt
    size_t len = 0, fit = 0;
    while (p[len] && p[len] != '\n' && len < sizeof(line) - 1) {
      len++;
      if (p[len] == ' ' || p[len] == '\n' || p[len] == 0) {
        memcpy(line, p, len);
        line[len] = 0;
        if (textWidth(line, f) > w) break;
        fit = len;
      }
    }
    if (fit == 0) fit = len;  // einzelnes zu langes Wort
    memcpy(line, p, fit);
    line[fit] = 0;
    const char* next = p + fit;
    while (*next == ' ') next++;
    bool last = lines + 1 == maxLines;
    if (last && *next) strlcat(line, " …", sizeof(line));
    if (draw) text(line, x, y + lines * lineHeight, f, color);
    lines++;
    if (*next == '\n') next++;
    p = next;
  }
  return lines;
}

}  // namespace

int wrapText(const char* s, int x, int y, int w, const uint8_t* f, uint32_t color, int lineHeight, int maxY) {
  int maxLines = max(1, (maxY - y) / lineHeight);
  return y + wrap(s, x, y, w, f, color, lineHeight, maxLines, true) * lineHeight;
}

int wrapLines(const char* s, int w, const uint8_t* f) { return wrap(s, 0, 0, w, f, 0, 0, 1000, false); }

void formatTemp(char* buf, size_t len, float t) { snprintf(buf, len, "%d°", (int)lroundf(t)); }

void titleCase(const char* in, char* out, size_t len) {
  bool start = true;
  size_t o = 0;
  for (size_t i = 0; in[i] && o + 1 < len; i++) {
    uint8_t c = in[i];
    if (c == 0xC3 && in[i + 1] && o + 2 < len) {
      // Zweites Byte von À bis Þ: um 0x20 auf den Kleinbuchstaben verschieben (außer ×)
      uint8_t c2 = in[++i];
      if (!start && c2 >= 0x80 && c2 <= 0x9E && c2 != 0x97) c2 += 0x20;
      out[o++] = c;
      out[o++] = c2;
      start = false;
      continue;
    }
    out[o++] = (!start && c >= 'A' && c <= 'Z') ? c + 32 : c;
    start = c == ' ' || c == '-' || c == '/';
  }
  out[o] = 0;
}

uint32_t warnColor(int level) {
  switch (level) {
    case 4: return 0x9C27B0u;
    case 3: return 0xE53935u;
    case 2: return 0xFB8C00u;
    default: return 0xFDD835u;
  }
}

// getLocalTime(…, 0) aus dem Arduino-Core kann fehlschlagen, wenn zwischen zwei millis()-Aufrufen
// eine Millisekunde vergeht (der Display-Interrupt macht das wahrscheinlich)
bool localNow(struct tm& tm) {
  time_t now = time(nullptr);
  localtime_r(&now, &tm);
  return tm.tm_year > 2020 - 1900;
}

int minuteOfDay() {
  struct tm now;
  return localNow(now) ? now.tm_hour * 60 + now.tm_min : -1;
}

void formatDecimal(char* buf, size_t len, float v, int decimals) {
  snprintf(buf, len, "%.*f", decimals, v);
  char* dot = strchr(buf, '.');
  if (dot) *dot = ',';
}

bool inRect(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

bool ditherEnabled = true;

namespace {

// 4x4-Bayer-Matrix, Schwellen 0 bis 15
constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Füllt w Pixel der Zeile y ab x mit der Farbe c (8 Bit je Kanal, als float für Zwischenwerte),
// gerastert auf RGB565 in der Byte-Reihenfolge von LovyanGFX
void ditherRow(int x, int y, int w, float r, float g, float b) {
  uint16_t pattern[4];
  for (int i = 0; i < 4; i++) {
    float thr = ditherEnabled ? (BAYER[y & 3][(x + i) & 3] + 0.5f) / 16.0f : 0.5f;
    int r5 = min(31, (int)(r * 31 / 255 + thr));
    int g6 = min(63, (int)(g * 63 / 255 + thr));
    int b5 = min(31, (int)(b * 31 / 255 + thr));
    pattern[i] = (uint16_t)(((r5 << 3) | (g6 >> 3)) | (((g6 & 7) << 5) | b5) << 8);
  }
  auto* row = static_cast<uint16_t*>(canvas.getBuffer()) + y * W + x;
  for (int i = 0; i < w; i++) row[i] = pattern[i & 3];
}

void ditherRow(int x, int y, int w, uint32_t top, uint32_t bottom, float f) {
  auto ch = [&](int shift) {
    float a = (top >> shift) & 0xFF, b = (bottom >> shift) & 0xFF;
    return a + (b - a) * f;
  };
  ditherRow(x, y, w, ch(16), ch(8), ch(0));
}

}  // namespace

void background(const Theme& t) {
  for (int y = 0; y < H; y++) ditherRow(0, y, W, t.top, t.bottom, y / (float)(H - 1));
}

void glass(const Theme& t, int x, int y, int w, int h, int r) {
  const float alpha = t.glassAlpha / 255.0f;
  canvas.fillSmoothRoundRect(x, y, w, h, r, mixColor(skyAt(t, y + h / 2), t.glass, alpha));
  // Zeilenweise nachfärben, damit die Karte dem Verlauf folgt; die Ecken behalten die Mittelfarbe
  const uint32_t top = mixColor(t.top, t.glass, alpha), bottom = mixColor(t.bottom, t.glass, alpha);
  for (int row = 0; row < h; row++) {
    // In den Eckzeilen nur innerhalb der Rundung; das äußerste, geglättete Pixel bleibt
    int inset = 0;
    int d = row < r ? r - row : row >= h - r ? row - (h - r - 1) : 0;
    if (d > 0) inset = r - (int)floorf(sqrtf((float)r * r - (d - 0.5f) * (d - 0.5f))) + 1;
    ditherRow(x + inset, y + row, w - 2 * inset, top, bottom, (y + row) / (float)(H - 1));
  }
}

void header(const Model& m, const Theme& t) {
  text(strlen(m.loc.city) ? m.loc.city : "Wetter", 24, 12, FONT_BOLD, t.text);

  struct tm now;
  char clock[8] = "--:--";
  if (localNow(now)) {
    char date[48];
    snprintf(date, sizeof(date), "%s, %d. %s %d", WEEKDAYS[now.tm_wday], now.tm_mday, MONTHS[now.tm_mon],
             now.tm_year + 1900);
    text(date, 26, 52, FONT_SMALL, t.dim);
    strftime(clock, sizeof(clock), "%H:%M", &now);
  }
  text(clock, 776, 48, FONT_CLOCK, t.text, baseline_right);
  text(m.status, 776, 56, FONT_SMALL, m.statusWarn ? WARN : t.dim, top_right);

  // Warnsymbol mit der höchsten Warnung; Antippen öffnet die Details
  const Warnings& ws = m.warnings;
  if (ws.valid && ws.count > 0) {
    const Warning& w = ws.item[0];
    uint32_t bg = warnColor(w.level), fg = w.level == 1 ? 0x202020u : 0xFFFFFFu;
    canvas.fillSmoothRoundRect(PILL_X, PILL_Y, PILL_W, PILL_H, PILL_H / 2, bg);
    int tx = PILL_X + 22, ty = PILL_Y + PILL_H / 2;
    canvas.fillTriangle(tx, ty - 11, tx - 11, ty + 9, tx + 11, ty + 9, fg);
    canvas.fillRect(tx - 1, ty - 4, 3, 7, bg);
    canvas.fillRect(tx - 1, ty + 5, 3, 2, bg);
    char label[64], name[40];
    titleCase(w.event, name, sizeof(name));
    if (ws.count > 1) {
      snprintf(label, sizeof(label), "%s +%d", name, ws.count - 1);
    } else {
      strlcpy(label, name, sizeof(label));
    }
    // Zu lange Namen kürzen
    while (textWidth(label, FONT_TITLE) > PILL_W - 52 && strlen(label) > 4) {
      label[strlen(label) - 1] = 0;
    }
    text(label, PILL_X + 40, ty + 1, FONT_TITLE, fg, middle_left);
  }

  // Seitenpunkte für die Wischseiten
  int page = (int)m.page;
  if (page < MAIN_PAGES) {
    for (int i = 0; i < MAIN_PAGES; i++) {
      int x = 400 + (i * 2 - (MAIN_PAGES - 1)) * 9;
      if (i == page) {
        canvas.fillSmoothCircle(x, 70, 4, t.text);
      } else {
        canvas.fillSmoothCircle(x, 70, 3, mixColor(skyAt(t, 70), t.dim, 0.6f));
      }
    }
  }
}

void tempCurve(int count, const float* temps, int x0, int x1, int top, int bottom, int labelEvery, uint32_t labelColor,
               const int* labelIndex) {
  if (count < 2) return;
  float lo = temps[0], hi = temps[0];
  for (int i = 1; i < count; i++) {
    lo = fminf(lo, temps[i]);
    hi = fmaxf(hi, temps[i]);
  }
  if (hi - lo < 4) {
    float mid = (hi + lo) / 2;
    lo = mid - 2;
    hi = mid + 2;
  }
  auto px = [&](int i) { return x0 + (x1 - x0) * i / (count - 1); };
  auto py = [&](int i) { return bottom - (int)lroundf((temps[i] - lo) / (hi - lo) * (bottom - top)); };

  for (int i = 1; i < count; i++) canvas.drawWideLine(px(i - 1), py(i - 1), px(i), py(i), 1.5f, CURVE);
  char buf[16];
  for (int i = 0; i < count; i++) {
    bool label = labelIndex ? labelIndex[i] : (i % labelEvery == 0);
    if (!label) continue;
    canvas.fillSmoothCircle(px(i), py(i), 4, CURVE);
    formatTemp(buf, sizeof(buf), temps[i]);
    text(buf, px(i), py(i) - 8, FONT_SMALL, labelColor, baseline_center);
  }
}

}  // namespace ui
