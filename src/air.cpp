#include "air.h"

#include "log.h"
#include "net.h"

bool fetchAir(const Location& loc, Air& air) {
  char url[384];
  snprintf(url, sizeof(url),
           "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&longitude=%.4f"
           "&current=european_aqi,pm2_5,pm10,ozone,nitrogen_dioxide&timezone=auto",
           loc.lat, loc.lon);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  JsonObject cur = doc["current"];
  air.aqi = cur["european_aqi"] | 0;
  air.pm25 = cur["pm2_5"] | 0.0f;
  air.pm10 = cur["pm10"] | 0.0f;
  air.ozone = cur["ozone"] | 0.0f;
  air.no2 = cur["nitrogen_dioxide"] | 0.0f;
  air.valid = true;
  logf("Luft: AQI %d, PM2.5 %.1f", air.aqi, air.pm25);
  return true;
}
