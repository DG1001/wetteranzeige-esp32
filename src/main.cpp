#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "lcd.h"
#include "touch.h"
#include "ui.h"
#include "weather.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "include/secrets.h fehlt: include/secrets.h.example kopieren und WLAN-Daten eintragen"
#endif

// Große Glyphen werden auf dem Stack gerendert, der Standard von 8 KB reicht dafür nicht
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

static Location location;
static Weather weather;

static bool located = false;
static bool redraw = true;
static uint32_t lastTouch = 0;
static uint32_t nextFetch = 0;
static uint32_t lastSuccess = 0;
static int utcOffset = INT32_MIN;
static int lastMinute = -2;

static void connectWifi() {
  uiStatus("Verbinde mit WLAN …", WIFI_SSID);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WLAN verbunden, IP %s\n", WiFi.localIP().toString().c_str());
    uiStatus("WLAN verbunden", "Lade Wetterdaten …");
  } else {
    uiStatus("Keine WLAN-Verbindung", "Prüfe WIFI_SSID und WIFI_PASSWORD in include/secrets.h");
  }
}

// Status unter der Uhr: Zeitpunkt des letzten Abrufs oder ein Problem
static void statusText(char* buf, size_t len, bool& warn) {
  char stand[24] = "";
  time_t fetched = time(nullptr) - (millis() - lastSuccess) / 1000;
  struct tm t;
  localtime_r(&fetched, &t);
  strftime(stand, sizeof(stand), "Stand %H:%M", &t);

  warn = true;
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(buf, len, "Kein WLAN · %s", stand);
  } else if (millis() - lastSuccess > 3 * WEATHER_INTERVAL_MS) {
    snprintf(buf, len, "Abruf fehlgeschlagen · %s", stand);
  } else {
    warn = false;
    strlcpy(buf, stand, len);
  }
}

static void draw() {
  if (!weather.valid) return;
  char status[64];
  bool warn;
  statusText(status, sizeof(status), warn);
  uiWeather(location, weather, status, warn);
}

static void update() {
  if (!located) located = locate(location);
  bool ok = located && fetchWeather(location, weather);
  nextFetch = millis() + (ok ? WEATHER_INTERVAL_MS : WEATHER_RETRY_MS);
  if (!ok) {
    if (!weather.valid) uiStatus("Wetterdaten nicht verfügbar", "Neuer Versuch in einer Minute");
    return;
  }

  lastSuccess = millis();
  if (weather.utcOffset != utcOffset) {
    utcOffset = weather.utcOffset;
    configTime(utcOffset, 0, "pool.ntp.org", "time.cloudflare.com");
  }
  lcd::setBrightness(weather.isDay ? BRIGHTNESS_DAY : BRIGHTNESS_NIGHT);
}

void setup() {
  Serial.begin(115200);
  if (!lcd::begin()) Serial.println("Display konnte nicht initialisiert werden");
  lcd::setBrightness(BRIGHTNESS_DAY);
  if (!touch::begin()) Serial.println("Touch-Controller nicht gefunden");
  connectWifi();
}

void loop() {
  // Tippen aktualisiert sofort; solange der Finger aufliegt, nur einmal
  int x, y;
  if (touch::read(x, y)) {
    if (millis() - lastTouch > 1000 && weather.valid) {
      uiWeather(location, weather, "Aktualisiere …", false);
      nextFetch = millis();
    }
    lastTouch = millis();
  }

  if (WiFi.status() == WL_CONNECTED && (int32_t)(millis() - nextFetch) >= 0) {
    update();
    redraw = true;
  }

  struct tm now;
  int minute = getLocalTime(&now, 0) ? now.tm_min : -1;
  if (minute != lastMinute) {
    lastMinute = minute;
    redraw = true;
  }

  if (redraw) {
    redraw = false;
    draw();
  }
  delay(20);
}
