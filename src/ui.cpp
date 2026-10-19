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

Theme theme;
uint16_t* iconBackground = nullptr;  // Hintergrund unter dem animierten Wettersymbol
int shownFrame = -1;

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
  background(theme);
  header(m, theme);

  switch (m.page) {
    case Page::Overview:
      overview(m, theme);
      if (!iconBackground) iconBackground = (uint16_t*)heap_caps_malloc(ICON_W * ICON_H * 2, MALLOC_CAP_SPIRAM);
      if (iconBackground) lcd::saveRect(iconBackground, ICON_X, ICON_Y, ICON_W, ICON_H);
      drawIcon(m, 0);
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
      return Target::Back;
    default:
      return Target::None;
  }
}

}  // namespace ui
