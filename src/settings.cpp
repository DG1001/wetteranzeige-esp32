#include "settings.h"

#include <Preferences.h>
#include <esp_random.h>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif

Settings settings;

namespace {

constexpr const char* NAMESPACE = "wetter";
constexpr uint8_t VERSION = 1;

void defaults(Settings& s) {
  s = Settings();
  strlcpy(s.ssid, WIFI_SSID, sizeof(s.ssid));
  strlcpy(s.password, WIFI_PASSWORD, sizeof(s.password));
  strlcpy(s.city, LOCATION_NAME, sizeof(s.city));
  s.lat = LOCATION_LAT;
  s.lon = LOCATION_LON;
  s.brightDay = BRIGHTNESS_DAY;
  s.brightNight = BRIGHTNESS_NIGHT;
  s.nightEnabled = false;
  s.nightStart = 22 * 60 + 30;
  s.nightEnd = 6 * 60 + 30;
  s.nightOff = false;
  s.brightClock = 12;
  snprintf(s.pin, sizeof(s.pin), "%08lu", (unsigned long)(esp_random() % 100000000UL));
}

}  // namespace

void loadSettings() {
  Preferences prefs;
  prefs.begin(NAMESPACE, true);
  bool stored = prefs.getUChar("version", 0) == VERSION && prefs.getBytesLength("data") == sizeof(Settings);
  if (stored) prefs.getBytes("data", &settings, sizeof(Settings));
  prefs.end();
  if (!stored) {
    defaults(settings);
    saveSettings();
    Serial.println("Einstellungen neu angelegt");
  }
}

void saveSettings() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putUChar("version", VERSION);
  prefs.putBytes("data", &settings, sizeof(Settings));
  prefs.end();
}

bool inNightWindow(int minute) {
  if (!settings.nightEnabled || minute < 0) return false;
  int start = settings.nightStart, end = settings.nightEnd;
  if (start == end) return false;
  return start < end ? (minute >= start && minute < end) : (minute >= start || minute < end);
}
