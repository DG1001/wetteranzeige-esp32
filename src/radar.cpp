#include "radar.h"

#include "log.h"

#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <math.h>

#include "lcd.h"
#include "net.h"

namespace radar {
namespace {

constexpr float MAP_WIDTH_KM = 300;
constexpr uint32_t FRAME_MAX_AGE_S = 10 * 60;
constexpr const char* WMS =
    "https://maps.dwd.de/geoserver/dwd/wms?service=WMS&version=1.1.1&request=GetMap&srs=EPSG:4326";

const City CITIES[] = {
    // Region Hohenlohe und Umgebung
    {"Heilbronn", 49.1427f, 9.2109f},      {"Schwäbisch Hall", 49.1124f, 9.7373f},
    {"Öhringen", 49.2003f, 9.5017f},       {"Crailsheim", 49.1347f, 10.0706f},
    {"Bad Mergentheim", 49.4925f, 9.7739f}, {"Mosbach", 49.3530f, 9.1450f},
    {"Tauberbischofsheim", 49.6225f, 9.6628f}, {"Rothenburg", 49.3772f, 10.1794f},
    {"Aalen", 48.8378f, 10.0933f},
    // Großstädte in Deutschland
    {"Stuttgart", 48.7758f, 9.1829f},      {"Würzburg", 49.7913f, 9.9534f},
    {"Nürnberg", 49.4521f, 11.0767f},      {"Mannheim", 49.4875f, 8.4660f},
    {"Heidelberg", 49.3988f, 8.6724f},     {"Karlsruhe", 49.0069f, 8.4037f},
    {"Ansbach", 49.3007f, 10.5719f},       {"Pforzheim", 48.8922f, 8.6946f},
    {"Darmstadt", 49.8728f, 8.6512f},      {"Bamberg", 49.8988f, 10.9028f},
    {"Frankfurt", 50.1109f, 8.6821f},      {"München", 48.1372f, 11.5755f},
    {"Augsburg", 48.3705f, 10.8978f},      {"Ulm", 48.4011f, 9.9876f},
    {"Regensburg", 49.0134f, 12.1016f},    {"Ingolstadt", 48.7665f, 11.4258f},
    {"Freiburg", 47.9990f, 7.8421f},       {"Konstanz", 47.6603f, 9.1758f},
    {"Mainz", 49.9929f, 8.2473f},          {"Wiesbaden", 50.0782f, 8.2398f},
    {"Saarbrücken", 49.2402f, 6.9969f},    {"Koblenz", 50.3569f, 7.5890f},
    {"Kassel", 51.3127f, 9.4797f},         {"Erfurt", 50.9787f, 11.0328f},
    {"Köln", 50.9375f, 6.9603f},           {"Düsseldorf", 51.2277f, 6.7735f},
    {"Dortmund", 51.5136f, 7.4653f},       {"Essen", 51.4556f, 7.0116f},
    {"Münster", 51.9607f, 7.6261f},        {"Bielefeld", 52.0302f, 8.5325f},
    {"Hannover", 52.3759f, 9.7320f},       {"Bremen", 53.0793f, 8.8017f},
    {"Hamburg", 53.5511f, 9.9937f},        {"Kiel", 54.3233f, 10.1228f},
    {"Rostock", 54.0924f, 12.0991f},       {"Berlin", 52.5200f, 13.4050f},
    {"Potsdam", 52.3906f, 13.0645f},       {"Magdeburg", 52.1205f, 11.6276f},
    {"Leipzig", 51.3397f, 12.3731f},       {"Dresden", 51.0504f, 13.7373f},
    {"Chemnitz", 50.8278f, 12.9214f},
};

// Der Hintergrund-Task lädt, die Oberfläche zeichnet; getauscht wird nur unter dieser Sperre
SemaphoreHandle_t mutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}
struct Guard {
  Guard() { xSemaphoreTake(mutex(), portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(mutex()); }
};

float minLon, minLat, maxLon, maxLat;
char bbox[64];

uint16_t* mapPixels = nullptr;  // abgedunkelte Karte, RGB565 mit vertauschten Bytes wie LovyanGFX

struct Frame {
  time_t time = 0;
  uint8_t* png = nullptr;
  size_t len = 0;
};
Frame frames[FRAMES];
time_t loadedAt = 0;  // Zeitpunkt, zu dem die Bildfolge festgelegt wurde

void formatTime(time_t t, char* buf, size_t len) {
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(buf, len, "%Y-%m-%dT%H:%M:00.000Z", &tm);
}

// Dunkelt die Karte ab und tönt sie leicht blau, damit Radar und Schrift hervortreten
void darken(uint16_t* px, size_t count) {
  for (size_t i = 0; i < count; i++) {
    uint16_t v = px[i];
    int r = (v & 0xF8) >> 3, g = ((v & 0x07) << 3) | (v >> 13), b = (v >> 8) & 0x1F;
    r = r * 42 / 100;
    g = g * 45 / 100;
    b = min(31, b * 55 / 100 + 1);
    px[i] = (uint16_t)(((r << 3) | (g >> 3)) | (((g & 7) << 5) | b) << 8);
  }
}

bool loadMap() {
  String url = String(WMS) + "&bbox=" + bbox + "&width=" + WIDTH + "&height=" + HEIGHT +
               "&layers=dwd:Natural_Earth_Map,dwd:Gewaesser,dwd:Laender"
               "&styles=natural_earth_map,fluesse,Laender_wei%C3%9F&format=image/png";
  size_t len;
  uint8_t* png = httpGetBytes(url, len, 600 * 1024);
  if (!png) return false;
  auto* pixels = (uint16_t*)heap_caps_malloc(WIDTH * HEIGHT * 2, MALLOC_CAP_SPIRAM);
  bool ok = false;
  if (pixels) {
    LGFX_Sprite sprite;
    sprite.setBuffer(pixels, WIDTH, HEIGHT, 16);
    sprite.fillScreen(0);
    ok = sprite.drawPng(png, len, 0, 0);
    darken(pixels, WIDTH * HEIGHT);
  }
  free(png);
  if (ok) {
    Guard g;
    free(mapPixels);
    mapPixels = pixels;
  } else {
    free(pixels);
  }
  logf("Radarkarte %s", ok ? "geladen" : "fehlerhaft");
  return ok;
}

// Legt die Zeitpunkte fest: letzte sichere Messung (jetzt minus 10 Minuten, auf 5 Minuten
// abgerundet) und davon ausgehend alle 10 Minuten
void planFrames(time_t now) {
  Guard g;
  time_t latest = (now - 10 * 60) / 300 * 300;
  for (int i = 0; i < FRAMES; i++) {
    free(frames[i].png);
    frames[i].png = nullptr;
    frames[i].len = 0;
    frames[i].time = latest + (i - NOW_FRAME) * STEP_MIN * 60;
  }
  loadedAt = now;
}

bool loadFrame(Frame& f) {
  char time[32];
  formatTime(f.time, time, sizeof(time));
  String url = String(WMS) + "&bbox=" + bbox + "&width=" + WIDTH + "&height=" + HEIGHT +
               "&layers=dwd:Niederschlagsradar&styles=&format=image/png&transparent=true&time=" + time;
  size_t len;
  uint8_t* png = httpGetBytes(url, len, 400 * 1024);
  if (!png) return false;
  Guard g;
  free(f.png);
  f.png = png;
  f.len = len;
  return true;
}

bool timeKnown() { return time(nullptr) > 1700000000; }

}  // namespace

void begin(const Location& loc) {
  Guard g;
  free(mapPixels);
  mapPixels = nullptr;
  for (auto& f : frames) {
    free(f.png);
    f = Frame();
  }
  loadedAt = 0;
  float lat = loc.lat, lon = loc.lon;
  float heightKm = MAP_WIDTH_KM * HEIGHT / WIDTH;
  float dLon = MAP_WIDTH_KM / (111.32f * cosf(lat * PI / 180));
  float dLat = heightKm / 110.57f;
  minLon = lon - dLon / 2;
  maxLon = lon + dLon / 2;
  minLat = lat - dLat / 2;
  maxLat = lat + dLat / 2;
  snprintf(bbox, sizeof(bbox), "%.4f,%.4f,%.4f,%.4f", minLon, minLat, maxLon, maxLat);
}

bool needsLoad() {
  if (!mapPixels) return true;
  if (!timeKnown()) return false;
  if (loadedAt == 0 || time(nullptr) - loadedAt > FRAME_MAX_AGE_S) return true;
  for (auto& f : frames) {
    if (!f.png) return true;
  }
  return false;
}

bool loadStep() {
  if (!mapPixels) return loadMap();
  if (!timeKnown()) return false;
  time_t now = time(nullptr);
  if (loadedAt == 0 || now - loadedAt > FRAME_MAX_AGE_S) planFrames(now);
  // Zuerst das aktuelle Bild, dann rückwärts die Vergangenheit, dann die Vorhersage
  for (int k = 0; k < FRAMES; k++) {
    int i = k <= NOW_FRAME ? NOW_FRAME - k : k;
    if (!frames[i].png) return loadFrame(frames[i]);
  }
  return true;
}

float progress() {
  int done = mapPixels ? 1 : 0;
  for (auto& f : frames) done += f.png ? 1 : 0;
  return done / (float)(FRAMES + 1);
}

bool hasMap() { return mapPixels != nullptr; }
bool hasFrame(int i) { return frames[i].png != nullptr; }
time_t frameTime(int i) { return frames[i].time; }

void draw(LGFX_Sprite& canvas, int y, int frame) {
  Guard g;
  if (mapPixels) {
    lcd::restoreRect(mapPixels, 0, y, WIDTH, HEIGHT);
  } else {
    canvas.fillRect(0, y, WIDTH, HEIGHT, 0x101820u);
  }
  if (frame >= 0 && frame < FRAMES && frames[frame].png) {
    canvas.drawPng(frames[frame].png, frames[frame].len, 0, y);
  }
}

bool project(float lat, float lon, int& x, int& y) {
  x = lroundf((lon - minLon) / (maxLon - minLon) * WIDTH);
  y = lroundf((maxLat - lat) / (maxLat - minLat) * HEIGHT);
  return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

const City* cities(int& count) {
  count = sizeof(CITIES) / sizeof(CITIES[0]);
  return CITIES;
}

}  // namespace radar
