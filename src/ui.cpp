#include "ui.h"

#include <esp_heap_caps.h>
#include <math.h>

#include "icons.h"
#include "lcd.h"
#include "radar.h"
#include "ui_common.h"
#include "ui_pages.h"

namespace ui {

namespace {

constexpr int MAP_Y = HEADER_H + 2;
constexpr float FRAME_S = 0.5f;  // Anzeigedauer eines Radarbilds
constexpr float HOLD_S = 1.2f;   // Pause am Ende der Bildfolge
constexpr float TICKER_S = 6;    // Anzeigedauer eines Eintrags der wechselnden Zeile

Theme theme;
uint16_t* iconBackground = nullptr;    // Hintergrund unter dem animierten Wettersymbol
uint16_t* tickerBackground = nullptr;  // Hintergrund unter der wechselnden Zeile
int shownFrame = -1;

// Wechselnde Zeile der Übersicht: Regenhinweis, beste Zeit draußen, wichtige Hinweise
struct TickerItem {
  char text[80];
  uint32_t color;
  bool rain;
};
TickerItem ticker[4];
int tickerCount = 0;
int tickerShown = 0;
int tickerPending = 0;  // in wie viele Bildspeicher der aktuelle Eintrag noch muss

void buildTicker(const Model& m) {
  tickerCount = 0;
  TickerItem& rain = ticker[tickerCount];
  rain.rain = nowcastText(m.weather, rain.text, sizeof(rain.text));
  rain.color = rain.rain ? RAIN : theme.dim;
  if (rain.text[0]) tickerCount++;
  TickerItem& best = ticker[tickerCount++];
  bestTimeText(m.outdoor, best.text, sizeof(best.text));
  best.color = theme.dim;
  best.rain = false;
  for (int i = 0; i < m.outdoor.hintCount && tickerCount < 4; i++) {
    const Hint& h = m.outdoor.hints[i];
    if (h.priority < 5) break;
    TickerItem& item = ticker[tickerCount++];
    strlcpy(item.text, h.text, sizeof(item.text));
    item.color = h.priority >= 8 ? WARN : theme.text;
    item.rain = false;
  }
}

void drawTicker(int index) {
  const TickerItem& item = ticker[index];
  int x = TICKER_X + 4;
  if (item.rain) {
    canvas.fillSmoothCircle(x + 5, TICKER_Y + 17, 4, RAIN);
    canvas.fillTriangle(x + 1, TICKER_Y + 16, x + 9, TICKER_Y + 16, x + 5, TICKER_Y + 8, RAIN);
    x += 16;
  }
  // Auf die verfügbare Breite kürzen, UTF-8-Zeichen nicht zerschneiden
  char buf[84];
  strlcpy(buf, item.text, sizeof(buf));
  const int maxW = TICKER_X + TICKER_W - x;
  if (textWidth(buf, FONT_SMALL) > maxW) {
    size_t len = strlen(buf);
    do {
      do {
        len--;
      } while (len > 0 && (buf[len] & 0xC0) == 0x80);
      strcpy(buf + len, "…");
    } while (len > 0 && textWidth(buf, FONT_SMALL) > maxW);
  }
  text(buf, x, TICKER_Y + 4, FONT_SMALL, item.color);
}

uint32_t timeColor(int frame) { return frame < radar::NOW_FRAME ? 0xB0BCCCu : frame == radar::NOW_FRAME ? 0xFFFFFFu : RAIN; }

void shadowText(const char* s, int x, int y, const uint8_t* f, uint32_t color, textdatum_t datum) {
  text(s, x + 1, y + 1, f, 0x000000, datum);
  text(s, x, y, f, color, datum);
}

// Beschriftung der Karte: eigener Ort hervorgehoben, andere Orte nur ohne Überlappung
void mapLabels(const Model& m) {
  struct Box {
    int x, y, w, h;
  };
  Box used[48];
  int usedCount = 0;
  auto isFree = [&](const Box& b) {
    if (b.x < 4 || b.x + b.w > W - 4 || b.y < MAP_Y + 4 || b.y + b.h > 412) return false;
    for (int i = 0; i < usedCount; i++) {
      const Box& u = used[i];
      if (b.x < u.x + u.w && u.x < b.x + b.w && b.y < u.y + u.h && u.y < b.y + b.h) return false;
    }
    return true;
  };
  used[usedCount++] = {8, MAP_Y + 6, 250, 56};  // Titel oben links

  int hx, hy;
  if (radar::project(m.loc.lat, m.loc.lon, hx, hy)) {
    hy += MAP_Y;
    canvas.drawCircle(hx, hy, 7, 0xFFFFFFu);
    canvas.drawCircle(hx, hy, 8, 0x000000u);
    canvas.fillSmoothCircle(hx, hy, 3, 0xFFFFFFu);
    int w = textWidth(m.loc.city, FONT_TITLE);
    shadowText(m.loc.city, hx + 12, hy, FONT_TITLE, 0xFFFFFFu, middle_left);
    used[usedCount++] = {hx - 10, hy - 14, w + 26, 28};
  }

  int count;
  const radar::City* cities = radar::cities(count);
  for (int i = 0; i < count && usedCount < 48; i++) {
    int x, y;
    if (!radar::project(cities[i].lat, cities[i].lon, x, y)) continue;
    y += MAP_Y;
    int w = textWidth(cities[i].name, FONT_SMALL);
    Box b = {x - 5, y - 10, w + 16, 20};
    if (!isFree(b)) continue;
    used[usedCount++] = b;
    canvas.fillSmoothCircle(x, y, 3, 0xD8DEE8u);
    shadowText(cities[i].name, x + 8, y, FONT_SMALL, 0xD8DEE8u, middle_left);
  }
}

void timeline(const Model& m, int frame) {
  const int y = 418, h = 54;
  canvas.fillSmoothRoundRect(12, y, 776, h, 12, 0x0B1220u);
  char buf[48];

  const int bx0 = 290, bx1 = 764, by = y + 20;
  auto fx = [&](int i) { return bx0 + (bx1 - bx0) * i / (radar::FRAMES - 1); };

  bool loading = radar::needsLoad();
  if (!radar::hasMap() || (loading && !radar::hasFrame(radar::NOW_FRAME))) {
    snprintf(buf, sizeof(buf), "Lade Radar … %d %%", (int)lroundf(radar::progress() * 100));
    text(buf, 30, y + h / 2, FONT_TITLE, 0xFFFFFFu, middle_left);
  } else {
    time_t local = radar::frameTime(frame) + m.weather.utcOffset;
    struct tm tm;
    gmtime_r(&local, &tm);
    const char* kind = frame < radar::NOW_FRAME ? "Messung" : frame == radar::NOW_FRAME ? "Aktuell" : "Vorhersage";
    snprintf(buf, sizeof(buf), "%s %02d:%02d", kind, tm.tm_hour, tm.tm_min);
    text(buf, 30, y + h / 2, FONT_TITLE, timeColor(frame), middle_left);
    if (m.radarPaused) {
      canvas.fillRect(254, y + 17, 5, 20, 0xFFFFFFu);
      canvas.fillRect(264, y + 17, 5, 20, 0xFFFFFFu);
    }
  }

  // Zeitleiste: Vergangenheit grau, Vorhersage blau, Markierung beim gezeigten Bild
  canvas.drawWideLine(fx(0), by, fx(radar::NOW_FRAME), by, 1.5f, 0x7A8699u);
  canvas.drawWideLine(fx(radar::NOW_FRAME), by, fx(radar::FRAMES - 1), by, 1.5f, RAIN_DIM);
  for (int i = 0; i < radar::FRAMES; i++) {
    uint32_t c = radar::hasFrame(i) ? timeColor(i) : 0x3A4455u;
    canvas.fillSmoothCircle(fx(i), by, i == radar::NOW_FRAME ? 4 : 2, c);
  }
  if (!loading || radar::hasFrame(frame)) {
    canvas.fillSmoothCircle(fx(frame), by, 7, timeColor(frame));
  }
  const char* const marks[] = {"-1 h", "jetzt", "+1 h", "+2 h"};
  const int markFrames[] = {0, radar::NOW_FRAME, radar::NOW_FRAME + 6, radar::NOW_FRAME + 12};
  for (int i = 0; i < 4; i++) text(marks[i], fx(markFrames[i]), by + 22, FONT_SMALL, 0x9AA6B8u, middle_center);
}

void radarView(const Model& m, int frame) {
  radar::draw(canvas, MAP_Y, frame);
  canvas.fillSmoothRoundRect(8, MAP_Y + 6, 250, 56, 10, 0x0B1220u);
  text("Regenradar", 22, MAP_Y + 12, FONT_TITLE, 0xFFFFFFu);
  text("Daten: Deutscher Wetterdienst", 22, MAP_Y + 38, FONT_SMALL, 0x9AA6B8u);
  mapLabels(m);
  timeline(m, frame);
}

// Welches Radarbild zur Zeit t gehört; während des Ladens das aktuelle
int radarFrameAt(const Model& m, float t) {
  if (m.radarPaused && shownFrame >= 0) return shownFrame;
  if (radar::needsLoad()) return radar::NOW_FRAME;
  float cycle = radar::FRAMES * FRAME_S + HOLD_S;
  int frame = (int)(fmodf(t, cycle) / FRAME_S);
  return min(frame, radar::FRAMES - 1);
}

void drawIcon(const Model& m, float t) {
  drawWeatherIcon(canvas, m.weather.code, m.weather.isDay, ICON_CX, ICON_CY, ICON_SIZE, skyAt(theme, ICON_CY), t);
}

}  // namespace

void portal(const char* apName, const char* password) {
  Theme t{0x0E1830u, 0x050A14u, 0xFFFFFFu, 0x8593ABu, 0xFFFFFFu, 18, true};
  background(t);
  text("Einrichtung", 40, 36, FONT_BOLD, t.text);
  text("1. Mit dem WLAN der Anzeige verbinden", 40, 104, FONT_TEXT, t.text);
  text("QR-Code scannen oder von Hand:", 64, 140, FONT_SMALL, t.dim);
  char buf[96];
  snprintf(buf, sizeof(buf), "Name: %s", apName);
  text(buf, 64, 166, FONT_TEXT, t.dim);
  snprintf(buf, sizeof(buf), "Passwort: %s", password);
  text(buf, 64, 198, FONT_TEXT, t.dim);
  text("2. Die Einrichtungsseite öffnet sich", 40, 252, FONT_TEXT, t.text);
  text("selbst, sonst http://192.168.4.1", 64, 286, FONT_TEXT, t.dim);
  text("3. WLAN und Ort eintragen, speichern", 40, 340, FONT_TEXT, t.text);
  text("Ist das gespeicherte WLAN wieder erreichbar, startet die Anzeige von selbst neu.", 40, 430, FONT_SMALL, t.dim);
  snprintf(buf, sizeof(buf), "WIFI:T:WPA;S:%s;P:%s;;", apName, password);
  canvas.fillSmoothRoundRect(560, 104, 210, 210, 12, 0xFFFFFFu);
  canvas.qrcode(buf, 568, 112, 194, 3, true);
  lcd::show();
  lcd::syncBack();
}

void status(const char* title, const char* detail) {
  Theme t{0x0E1830u, 0x050A14u, 0xFFFFFFu, 0x8593ABu, 0xFFFFFFu, 18, true};
  background(t);
  text(title, 400, 222, FONT_BOLD, t.text, middle_center);
  text(detail, 400, 266, FONT_TEXT, t.dim, middle_center);
  lcd::show();
  lcd::syncBack();
}

void render(const Model& m) {
  theme = skyTheme(m.weather, minuteOfDay());
  if (m.page == Page::Night) {
    night(m);
    lcd::show();
    lcd::syncBack();
    return;
  }
  background(theme);
  header(m, theme);

  switch (m.page) {
    case Page::Overview:
      overview(m, theme);
      if (!iconBackground) iconBackground = (uint16_t*)heap_caps_malloc(ICON_W * ICON_H * 2, MALLOC_CAP_SPIRAM);
      if (!tickerBackground) tickerBackground = (uint16_t*)heap_caps_malloc(TICKER_W * TICKER_H * 2, MALLOC_CAP_SPIRAM);
      if (iconBackground) lcd::saveRect(iconBackground, ICON_X, ICON_Y, ICON_W, ICON_H);
      if (tickerBackground) lcd::saveRect(tickerBackground, TICKER_X, TICKER_Y, TICKER_W, TICKER_H);
      buildTicker(m);
      tickerShown = 0;
      tickerPending = 0;
      drawTicker(0);
      drawIcon(m, 0);
      break;
    case Page::Outdoor:
      outdoor(m, theme);
      break;
    case Page::Info:
      info(m, theme);
      break;
    case Page::Night:
      break;
    case Page::Radar:
      shownFrame = radarFrameAt(m, 0);
      radarView(m, shownFrame);
      break;
    case Page::Hours:
      hours(m, theme);
      break;
    case Page::SkyAir:
      skyAir(m, theme);
      break;
    case Page::Day:
      day(m, theme);
      break;
    case Page::Warnings:
      warnings(m, theme);
      break;
  }
  lcd::show();
  lcd::syncBack();
}

bool animated(const Model& m) { return m.page == Page::Overview || m.page == Page::Radar; }

void animate(const Model& m, float t, bool force) {
  if (m.page == Page::Overview && iconBackground) {
    lcd::restoreRect(iconBackground, ICON_X, ICON_Y, ICON_W, ICON_H);
    drawIcon(m, t);
    int index = tickerCount ? (int)(t / TICKER_S) % tickerCount : 0;
    if (index != tickerShown) {
      tickerShown = index;
      tickerPending = 2;  // beide Bildspeicher
    }
    if (tickerPending > 0 && tickerBackground) {
      lcd::restoreRect(tickerBackground, TICKER_X, TICKER_Y, TICKER_W, TICKER_H);
      drawTicker(tickerShown);
      tickerPending--;
    }
    lcd::show();
  } else if (m.page == Page::Radar) {
    int frame = radarFrameAt(m, t);
    if (frame == shownFrame && !force) return;
    shownFrame = frame;
    radarView(m, frame);
    lcd::show();
  }
}

Target hit(const Model& m, int x, int y, int& day) {
  if (m.warnings.count > 0 && (int)m.page < MAIN_PAGES && inRect(x, y, PILL_X, PILL_Y, PILL_W, PILL_H)) {
    return Target::Warnings;
  }
  switch (m.page) {
    case Page::Overview:
      if (inRect(x, y, HOURLY_X, HOURLY_Y, HOURLY_W, HOURLY_H)) return Target::Hours;
      for (int i = 0; i < TILES; i++) {
        if (inRect(x, y, TILE_X + i * (TILE_W + TILE_GAP), TILE_Y, TILE_W, TILE_H)) {
          day = Weather::TODAY + i;
          return Target::Day;
        }
      }
      return Target::Refresh;
    case Page::Radar:
      return Target::Pause;
    case Page::Day:
    case Page::Warnings:
    case Page::Info:
      return Target::Back;
    default:
      return Target::None;
  }
}

}  // namespace ui
