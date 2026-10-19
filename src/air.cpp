#include "air.h"

#include "net.h"

const char* const POLLEN_NAMES[Air::POLLEN] = {"Erle", "Birke", "Gräser", "Beifuß", "Ambrosia"};
static const char* const POLLEN_KEYS[Air::POLLEN] = {"alder_pollen", "birch_pollen", "grass_pollen",
                                                     "mugwort_pollen", "ragweed_pollen"};
// Schwellen für gering / mittel / hoch in Körnern pro m³
static const float POLLEN_LIMITS[Air::POLLEN][3] = {
    {1, 10, 50}, {1, 10, 50}, {1, 10, 50}, {1, 10, 30}, {1, 5, 15},
};

bool fetchAir(const Location& loc, Air& air) {
  char url[512];
  snprintf(url, sizeof(url),
           "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
           "&current=european_aqi,pm2_5,pm10,ozone,nitrogen_dioxide"
           "&hourly=alder_pollen,birch_pollen,grass_pollen,mugwort_pollen,ragweed_pollen"
           "&forecast_days=1&timezone=auto",
           loc.lat, loc.lon);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  JsonObject cur = doc["current"];
  air.aqi = cur["european_aqi"] | 0;
  air.pm25 = cur["pm2_5"] | 0.0f;
  air.pm10 = cur["pm10"] | 0.0f;
  air.ozone = cur["ozone"] | 0.0f;
  air.no2 = cur["nitrogen_dioxide"] | 0.0f;

  // Stundenwerte ab der aktuellen Stunde
  const char* now = cur["time"] | "";
  JsonObject hourly = doc["hourly"];
  JsonArray times = hourly["time"];
  for (int p = 0; p < Air::POLLEN; p++) {
    float maxValue = 0;
    JsonArray values = hourly[POLLEN_KEYS[p]];
    for (size_t i = 0; i < times.size(); i++) {
      if (strncmp(times[i] | "", now, 13) < 0) continue;
      maxValue = fmaxf(maxValue, values[i] | 0.0f);
    }
    air.pollenToday[p] = maxValue;
  }
  air.valid = true;
  Serial.printf("Luft: AQI %d, PM2.5 %.1f\n", air.aqi, air.pm25);
  return true;
}

int pollenLevel(int type, float value) {
  int level = 0;
  while (level < 3 && value >= POLLEN_LIMITS[type][level]) level++;
  return level;
}
