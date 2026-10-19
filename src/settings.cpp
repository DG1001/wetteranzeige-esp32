#include "settings.h"

#include "log.h"

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
  s.pollenRegion = POLLEN_REGION;
  s.bioZone = BIO_ZONE;
  snprintf(s.pin, sizeof(s.pin), "%08lu", (unsigned long)(esp_random() % 100000000UL));
}

}  // namespace

void loadSettings() {
  Preferences prefs;
  prefs.begin(NAMESPACE, true);
  size_t len = prefs.getBytesLength("data");
  bool stored = prefs.getUChar("version", 0) == VERSION && len > 0 && len <= sizeof(Settings);
  // Vorgaben zuerst, dann die gespeicherten Bytes darüber: Felder, die eine ältere Firmware noch
  // nicht kannte (hinten angefügt), behalten so ihre Vorgabe
  Settings fresh;
  defaults(fresh);
  settings = fresh;
  if (stored) {
    prefs.getBytes("data", &settings, len);
    if (!settings.pin[0]) stored = false;  // unbrauchbar
  }
  prefs.end();
  if (!stored) {
    settings = fresh;
    logf("Einstellungen neu angelegt");
  }
  if (!stored || len < sizeof(Settings)) saveSettings();
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
