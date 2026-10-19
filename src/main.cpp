#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <time.h>

#include "config.h"
#include "lcd.h"
#include "model.h"
#include "radar.h"
#include "settings.h"
#include "touch.h"
#include "ui.h"
#include "ui_common.h"
#include "web.h"

// Große Glyphen werden auf dem Stack gerendert, der Standard von 8 KB reicht dafür nicht
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

namespace {

// Etwa 12 Bilder pro Sekunde; jedes Bild belastet den PSRAM, den auch das Display ständig liest
constexpr uint32_t ANIMATION_MS = 80;
constexpr uint32_t PAGE_TIMEOUT_MS = 3 * 60 * 1000;  // danach zurück zur Übersicht
constexpr uint32_t WAKE_MS = 60 * 1000;               // so lange weckt Antippen nachts das Display
constexpr uint32_t PORTAL_MS = 5 * 60 * 1000;         // danach Neustart, falls nur das WLAN gefehlt hat

Model model;
bool located = false;
bool needRender = true;
bool radarChanged = false;
uint32_t renderedAt = 0;
uint32_t lastAnimation = 0;
uint32_t lastInteraction = 0;
uint32_t lastWeatherOk = 0;
uint32_t nextWeather = 0, nextAir = 0, nextWarnings = 0;
int utcOffset = INT32_MIN;
int lastMinute = -2;
bool portalMode = false;
uint32_t portalSince = 0;
bool nightActive = false;
uint32_t wakeUntil = 0;

bool due(uint32_t at) { return (int32_t)(millis() - at) >= 0; }

void applyBrightness() {
  if (nightActive) {
    lcd::setBrightness(settings.nightOff ? 0 : settings.brightClock);
  } else {
    lcd::setBrightness(!model.weather.valid || model.weather.isDay ? settings.brightDay : settings.brightNight);
  }
}

// Verbindet mit dem gespeicherten WLAN; false, wenn keins eingerichtet ist oder es nicht klappt
bool connectWifi() {
  if (!settings.ssid[0]) return false;
  ui::status("Verbinde mit WLAN …", settings.ssid);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(web::HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(settings.ssid, settings.password);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.println("WLAN verbunden");
  ui::status("WLAN verbunden", "Lade Wetterdaten …");
  return true;
}

void startPortal() {
  portalMode = true;
  portalSince = millis();
  web::beginPortal();
  ui::portal(web::AP_NAME, settings.pin);
}

// Im Hotspot-Modus regelmäßig das gespeicherte WLAN versuchen (z. B. wenn der Router nach einem
// Stromausfall langsamer startet), aber nur, solange niemand mit dem Hotspot verbunden ist
void portalLoop() {
  static uint32_t lastTry = 0;
  if (settings.ssid[0] && WiFi.softAPgetStationNum() == 0 && millis() - lastTry > 30000) {
    lastTry = millis();
    WiFi.begin(settings.ssid, settings.password);
  }
  if (settings.ssid[0] && WiFi.status() == WL_CONNECTED) {
    Serial.println("Gespeichertes WLAN wieder erreichbar, Neustart");
    ESP.restart();
  }
  if (millis() - portalSince > PORTAL_MS && WiFi.softAPgetStationNum() == 0) ESP.restart();
  delay(5);
}

// Nach dem Speichern auf der Webseite
void onSettingsChanged(const web::Changes& c) {
  if (c.location) {
    located = false;
    nextWeather = nextAir = nextWarnings = millis();
  }
  applyBrightness();
  needRender = true;
}

// Status unter der Uhr: Zeitpunkt des letzten Abrufs oder ein Problem
void updateStatus() {
  char stand[24];
  time_t fetched = time(nullptr) - (millis() - lastWeatherOk) / 1000;
  struct tm t;
  localtime_r(&fetched, &t);
  strftime(stand, sizeof(stand), "Stand %H:%M", &t);

  model.statusWarn = true;
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(model.status, sizeof(model.status), "Kein WLAN · %s", stand);
  } else if (millis() - lastWeatherOk > 3 * WEATHER_INTERVAL_MS) {
    snprintf(model.status, sizeof(model.status), "Abruf fehlgeschlagen · %s", stand);
  } else {
    model.statusWarn = false;
    strlcpy(model.status, stand, sizeof(model.status));
  }
}

void updateWeather() {
  if (!located) {
    located = locate(model.loc);
    if (located) radar::begin(model.loc);
  }
  bool ok = located && fetchWeather(model.loc, model.weather);
  nextWeather = millis() + (ok ? WEATHER_INTERVAL_MS : WEATHER_RETRY_MS);
  if (!ok) {
    if (!model.weather.valid) ui::status("Wetterdaten nicht verfügbar", "Neuer Versuch in einer Minute");
    return;
  }
  lastWeatherOk = millis();
  if (model.weather.utcOffset != utcOffset) {
    utcOffset = model.weather.utcOffset;
    configTime(utcOffset, 0, "pool.ntp.org", "time.cloudflare.com");
  }
  evaluateOutdoor(model.weather, model.outdoor);
  applyBrightness();
}

// Höchstens ein Abruf je Durchlauf, damit Touch und Animation nicht lange warten
void network() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (due(nextWeather)) {
    updateWeather();
    needRender = true;
  } else if (!model.weather.valid) {
    return;
  } else if (due(nextWarnings)) {
    bool ok = fetchWarnings(model.loc, model.weather.utcOffset, model.warnings);
    nextWarnings = millis() + (ok ? WARNINGS_INTERVAL_MS : WEATHER_RETRY_MS);
    needRender = true;
  } else if (due(nextAir)) {
    bool ok = fetchAir(model.loc, model.air);
    nextAir = millis() + (ok ? AIR_INTERVAL_MS : WEATHER_RETRY_MS);
    if (model.page == Page::SkyAir) needRender = true;
  } else if (!radar::hasMap() || (model.page == Page::Radar && radar::needsLoad())) {
    // Die Karte wird schon vorab geladen, die Bilder erst auf der Radarseite
    radar::loadStep();
    radarChanged = true;
  }
}

void show(Page page) {
  model.page = page;
  needRender = true;
}

void onTap(int x, int y) {
  int day = Weather::TODAY;
  switch (ui::hit(model, x, y, day)) {
    case ui::Target::Refresh:
      strlcpy(model.status, "Aktualisiere …", sizeof(model.status));
      model.statusWarn = false;
      ui::render(model);
      nextWeather = nextWarnings = nextAir = millis();
      break;
    case ui::Target::Warnings:
      show(Page::Warnings);
      break;
    case ui::Target::Hours:
      show(Page::Hours);
      break;
    case ui::Target::Day:
      model.day = day;
      show(Page::Day);
      break;
    case ui::Target::Back:
      show(Page::Overview);
      break;
    case ui::Target::Pause:
      model.radarPaused = !model.radarPaused;
      needRender = true;
      break;
    case ui::Target::None:
      break;
  }
}

// Nachtmodus im eingestellten Zeitraum, außer kurz nach einer Berührung
void updateNight(int minute) {
  bool night = inNightWindow(minute) && due(wakeUntil);
  if (night == nightActive) return;
  nightActive = night;
  show(night ? Page::Night : Page::Overview);
  applyBrightness();
}

void onSwipe(int direction) {  // +1 = nach links gewischt (weiter), -1 = nach rechts (zurück)
  int page = (int)model.page;
  if (page < MAIN_PAGES) {
    int next = page + direction;
    if (next >= 0 && next < MAIN_PAGES) show((Page)next);
  } else if (model.page == Page::Day) {
    int next = model.day + direction;
    if (next >= Weather::TODAY && next < model.weather.days) {
      model.day = next;
      needRender = true;
    }
  } else {
    show(Page::Overview);
  }
}

// Testbefehle über die serielle Schnittstelle (115200 Baud, eine Zeile je Befehl):
//   seite N       Seite N anzeigen (0 Übersicht, 1 Draußen, 2 Radar, 3 48 Stunden, 4 Himmel & Luft, 5 Tag,
//                 6 Warnungen, 7 Nacht, 8 Info)
//   tipp X Y      Antippen simulieren
//   wisch l|r     Wischen nach links bzw. rechts
//   pin           PIN ausgeben (nur über USB, für Tests der Webseite)
//   einrichtung   Einrichtungs-Hotspot starten (Einstellungen bleiben, Neustart nach 5 Minuten)
//   warnung B L   Warnungen für Breite B und Länge L laden (zum Testen der Warnungsseite)
//   foto          Bildschirmfoto: Zeile „FOTO 800 480“, dann nach einem „G“ vom Rechner die Rohdaten
//                 mit 460800 Baud
void serialCommands() {
  static char line[48];
  static size_t len = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n' && c != '\r') {
      if (len < sizeof(line) - 1) line[len++] = c;
      continue;
    }
    if (len == 0) continue;
    line[len] = 0;
    len = 0;
    lastInteraction = millis();
    int a, b;
    float lat, lon;
    char dir;
    if (sscanf(line, "seite %d", &a) == 1 && a >= 0 && a <= (int)Page::Info) {
      show((Page)a);
    } else if (sscanf(line, "tipp %d %d", &a, &b) == 2) {
      onTap(a, b);
    } else if (sscanf(line, "wisch %c", &dir) == 1) {
      onSwipe(dir == 'l' ? +1 : -1);
    } else if (!strcmp(line, "einrichtung")) {
      startPortal();
    } else if (!strcmp(line, "pin")) {
      Serial.printf("PIN %s\n", settings.pin);
    } else if (sscanf(line, "warnung %f %f", &lat, &lon) == 2) {
      Location other = model.loc;
      other.lat = lat;
      other.lon = lon;
      fetchWarnings(other, model.weather.utcOffset, model.warnings);
      nextWarnings = millis() + 30 * 60 * 1000;
      needRender = true;
    } else if (!strcmp(line, "foto")) {
      Serial.printf("FOTO %d %d\n", lcd::WIDTH, lcd::HEIGHT);
      Serial.flush();
      Serial.updateBaudRate(460800);
      // Erst senden, wenn der Rechner ebenfalls umgeschaltet hat
      uint32_t start = millis();
      while (Serial.read() != 'G' && millis() - start < 3000) delay(1);
      Serial.write((const uint8_t*)lcd::front(), lcd::WIDTH * lcd::HEIGHT * 2);
      Serial.flush();
      delay(50);
      Serial.updateBaudRate(115200);
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  loadSettings();
  if (!lcd::begin()) Serial.println("Display konnte nicht initialisiert werden");
  lcd::setBrightness(settings.brightDay);
  if (!touch::begin()) Serial.println("Touch-Controller nicht gefunden");
  Serial.printf("Freier interner Speicher: %u Bytes, PSRAM: %u Bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  if (connectWifi()) {
    web::beginStation(model, onSettingsChanged);
  } else {
    startPortal();
  }
}

void loop() {
  web::loop();
  serialCommands();
  if (portalMode) {
    portalLoop();
    return;
  }

  int x, y;
  touch::Gesture gesture = touch::poll(x, y);
  if (gesture != touch::Gesture::None && model.weather.valid) {
    lastInteraction = millis();
    if (nightActive || model.page == Page::Night) {
      // Nachts weckt jede Berührung nur das Display
      wakeUntil = millis() + WAKE_MS;
      updateNight(ui::minuteOfDay());
    } else if (gesture == touch::Gesture::LongPress) {
      show(Page::Info);
    } else if (gesture == touch::Gesture::Tap) {
      onTap(x, y);
    } else if (gesture == touch::Gesture::SwipeLeft) {
      onSwipe(+1);
    } else if (gesture == touch::Gesture::SwipeRight) {
      onSwipe(-1);
    }
  }

  network();

  int minute = ui::minuteOfDay();
  if (minute != lastMinute) {
    lastMinute = minute;
    needRender = true;
  }
  updateNight(minute);
  if (!nightActive && model.page != Page::Overview && millis() - lastInteraction > PAGE_TIMEOUT_MS) {
    show(Page::Overview);
  }

  if (!model.weather.valid) return;
  if (needRender) {
    needRender = false;
    updateStatus();
    ui::render(model);
    renderedAt = lastAnimation = millis();
  } else if (ui::animated(model) && millis() - lastAnimation >= ANIMATION_MS) {
    lastAnimation = millis();
    ui::animate(model, (millis() - renderedAt) / 1000.0f, radarChanged);
    radarChanged = false;
  }
  delay(2);
}
