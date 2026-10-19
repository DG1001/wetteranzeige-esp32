#include "garden.h"

#include "log.h"
#include "net.h"

bool fetchGarden(const Location& loc, Garden& g) {
  char url[320];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&daily=precipitation_sum,et0_fao_evapotranspiration&past_days=%d&forecast_days=%d&timezone=auto",
           loc.lat, loc.lon, Garden::PAST, Garden::DAYS - Garden::PAST);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  JsonObject daily = doc["daily"];
  JsonArray time = daily["time"];
  g.days = min((int)time.size(), Garden::DAYS);
  if (g.days < Garden::PAST + 2) return false;
  g.balance = 0;
  for (int i = 0; i < g.days; i++) {
    const char* d = time[i];
    int y = atoi(d), m = atoi(d + 5), day = atoi(d + 8);
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int yy = m < 3 ? y - 1 : y;
    g.weekday[i] = (yy + yy / 4 - yy / 100 + yy / 400 + t[m - 1] + day) % 7;
    g.rain[i] = daily["precipitation_sum"][i] | 0.0f;
    g.et0[i] = daily["et0_fao_evapotranspiration"][i] | 0.0f;
    if (i < Garden::PAST) g.balance += g.rain[i] - g.et0[i];
  }
  g.rainSoon = g.rain[Garden::PAST] + g.rain[Garden::PAST + 1];
  g.valid = true;
  logf("Garten: Bilanz %.1f mm, Regen bald %.1f mm", g.balance, g.rainSoon);
  return true;
}
