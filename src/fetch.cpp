#include "fetch.h"

#include <WiFi.h>
#include <esp_heap_caps.h>

#include <atomic>

#include "config.h"
#include "log.h"
#include "radar.h"
#include "settings.h"

SemaphoreHandle_t modelMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
  return mutex;
}

namespace fetch {
namespace {

constexpr uint32_t STACK = 16 * 1024;
constexpr uint32_t RETRY_MS = WEATHER_RETRY_MS;

Model* model = nullptr;
std::atomic<bool> changed{false};
std::atomic<bool> radarChanged{false};
std::atomic<bool> refresh{false};
std::atomic<bool> relocate{true};
std::atomic<uint32_t> beat{0};
std::atomic<bool> paused{false};
char startup[64] = "Lade Wetterdaten …";

Source sources[SOURCES] = {{"Wetter"}, {"Warnungen"}, {"Luftqualität"}, {"Pollenflug"},
                           {"Biowetter"}, {"Garten"},   {"Radar"}};
uint32_t nextAt[SOURCES] = {};
const uint32_t INTERVAL[SOURCES] = {WEATHER_INTERVAL_MS, WARNINGS_INTERVAL_MS, AIR_INTERVAL_MS, HEALTH_INTERVAL_MS,
                                    HEALTH_INTERVAL_MS,  GARDEN_INTERVAL_MS,   0};

// Zwischenspeicher im PSRAM: Abrufe dauern, das Modell bleibt währenddessen unverändert
Weather* weather = nullptr;
Warnings* warnings = nullptr;
Health* health = nullptr;
int utcOffset = INT32_MIN;
Location location;
bool located = false;

bool due(int id) { return (int32_t)(millis() - nextAt[id]) >= 0; }

void done(int id, bool ok) {
  Source& s = sources[id];
  s.lastTry = millis();
  s.ok = ok;
  if (ok) {
    s.lastOk = s.lastTry;
  } else {
    s.failures++;
  }
  nextAt[id] = millis() + (ok ? INTERVAL[id] : RETRY_MS);
}

void reevaluate() {
  ModelLock lock;
  evaluateOutdoor(model->weather, model->health, model->garden, model->outdoor);
}

void updateWeather() {
  if (!located) {
    located = locate(location);
    if (located) {
      radar::begin(location);
      ModelLock lock;
      model->loc = location;
    }
  }
  bool ok = located && fetchWeather(location, *weather);
  done(WEATHER, ok);
  if (!ok) {
    strlcpy(startup, "Wetterdaten nicht verfügbar, neuer Versuch in einer Minute", sizeof(startup));
    changed = true;
    return;
  }
  if (weather->utcOffset != utcOffset) {
    utcOffset = weather->utcOffset;
    configTime(utcOffset, 0, "pool.ntp.org", "time.cloudflare.com");
  }
  {
    ModelLock lock;
    model->weather = *weather;
  }
  reevaluate();
  changed = true;
}

void updateWarnings() {
  bool ok = fetchWarnings(location, utcOffset, *warnings);
  done(WARNINGS, ok);
  if (!ok) return;
  ModelLock lock;
  model->warnings = *warnings;
  changed = true;
}

void updateAir() {
  Air air;
  bool ok = fetchAir(location, air);
  done(AIR, ok);
  if (!ok) return;
  ModelLock lock;
  model->air = air;
  changed = true;
}

void updateHealth(int id) {
  {
    ModelLock lock;
    *health = model->health;  // Pollen und Biowetter teilen sich die Struktur
  }
  bool ok = id == POLLEN ? fetchPollen(settings.pollenRegion, *health) : fetchBio(settings.bioZone, *health);
  done(id, ok);
  if (!ok) return;
  {
    ModelLock lock;
    if (id == POLLEN) {
      memcpy(model->health.pollen, health->pollen, sizeof(health->pollen));
      model->health.pollenValid = true;
    } else {
      memcpy(model->health.bio, health->bio, sizeof(health->bio));
      memcpy(model->health.bioLabel, health->bioLabel, sizeof(health->bioLabel));
      strlcpy(model->health.advice, health->advice, sizeof(health->advice));
      model->health.bioValid = true;
    }
  }
  reevaluate();
  changed = true;
}

void updateGarden() {
  Garden garden;
  bool ok = fetchGarden(location, garden);
  done(GARDEN, ok);
  if (!ok) return;
  {
    ModelLock lock;
    model->garden = garden;
  }
  reevaluate();
  changed = true;
}

// Ein Abruf je Durchlauf in fester Reihenfolge der Wichtigkeit
bool step() {
  if (relocate.exchange(false)) {
    located = false;
    for (auto& t : nextAt) t = millis();
  }
  if (refresh.exchange(false)) {
    for (auto& t : nextAt) t = millis();
  }
  if (due(WEATHER)) {
    updateWeather();
    return true;
  }
  if (!located || !model->weather.valid) return false;
  if (due(WARNINGS)) {
    updateWarnings();
  } else if (due(POLLEN)) {
    updateHealth(POLLEN);
  } else if (due(BIO)) {
    updateHealth(BIO);
  } else if (due(AIR)) {
    updateAir();
  } else if (due(GARDEN)) {
    updateGarden();
  } else if (!radar::hasMap() || (model->page == Page::Radar && radar::needsLoad())) {
    // Die Karte vorab, die Bilder nur auf der Radarseite
    bool ok = radar::loadStep();
    sources[RADAR].lastTry = millis();
    sources[RADAR].ok = ok;
    if (ok) {
      sources[RADAR].lastOk = millis();
    } else {
      sources[RADAR].failures++;
    }
    radarChanged = true;
  } else {
    return false;
  }
  return true;
}

void task(void*) {
  for (;;) {
    beat = millis();
    bool worked = !paused && WiFi.status() == WL_CONNECTED && step();
    vTaskDelay(pdMS_TO_TICKS(worked ? 20 : 250));
  }
}

}  // namespace

void begin(Model& m) {
  model = &m;
  weather = (Weather*)heap_caps_calloc(1, sizeof(Weather), MALLOC_CAP_SPIRAM);
  warnings = (Warnings*)heap_caps_calloc(1, sizeof(Warnings), MALLOC_CAP_SPIRAM);
  health = (Health*)heap_caps_calloc(1, sizeof(Health), MALLOC_CAP_SPIRAM);
  // Kern 0 (dort läuft auch das WLAN), die Oberfläche bleibt auf Kern 1
  xTaskCreatePinnedToCore(task, "abrufe", STACK, nullptr, 1, nullptr, 0);
}

void refreshAll() { refresh = true; }
void pause(bool p) { paused = p; }
void settingsChanged() { relocate = true; }
bool takeChanged() { return changed.exchange(false); }
bool takeRadarChanged() { return radarChanged.exchange(false); }
uint32_t heartbeat() { return beat; }
const Source& source(int id) { return sources[id]; }
const char* startupMessage() { return startup; }

}  // namespace fetch
