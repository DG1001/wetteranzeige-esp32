#include "weather.h"

#include "config.h"
#include "net.h"

bool locate(Location& loc) {
  if (strlen(LOCATION_NAME) > 0) {
    strlcpy(loc.city, LOCATION_NAME, sizeof(loc.city));
    loc.lat = LOCATION_LAT;
    loc.lon = LOCATION_LON;
    return true;
  }
  JsonDocument doc;
  if (!httpGetJson("http://ip-api.com/json/?fields=status,city,lat,lon&lang=de", doc)) return false;
  if (doc["status"] != "success") return false;
  strlcpy(loc.city, doc["city"] | "", sizeof(loc.city));
  loc.lat = doc["lat"];
  loc.lon = doc["lon"];
  Serial.printf("Standort: %s (%.4f, %.4f)\n", loc.city, loc.lat, loc.lon);
  return true;
}

// Wochentag aus Jahr, Monat, Tag (Sakamoto-Verfahren), 0 = Sonntag
static int weekdayOf(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// Minuten seit Mitternacht aus "JJJJ-MM-TTTHH:MM"
static int minutesOf(const char* iso) {
  if (!iso || strlen(iso) < 16) return -1;
  return atoi(iso + 11) * 60 + atoi(iso + 14);
}

bool fetchWeather(const Location& loc, Weather& w) {
  char url[768];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
           "wind_speed_10m,wind_direction_10m,wind_gusts_10m,is_day,surface_pressure,uv_index"
           "&hourly=temperature_2m,precipitation_probability,precipitation,weather_code,"
           "wind_speed_10m,wind_gusts_10m,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
           "precipitation_sum,wind_gusts_10m_max,uv_index_max,sunrise,sunset,daylight_duration"
           "&minutely_15=precipitation&forecast_minutely_15=%d"
           "&past_days=1&forecast_days=%d&timezone=auto",
           loc.lat, loc.lon, Weather::QUARTERS, Weather::DAYS - 1);

  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;

  w.utcOffset = doc["utc_offset_seconds"] | 0;

  JsonObject cur = doc["current"];
  w.temp = cur["temperature_2m"];
  w.feels = cur["apparent_temperature"];
  w.humidity = cur["relative_humidity_2m"];
  w.code = cur["weather_code"];
  w.wind = cur["wind_speed_10m"];
  w.windDir = cur["wind_direction_10m"];
  w.gusts = cur["wind_gusts_10m"];
  w.isDay = cur["is_day"] == 1;
  w.pressure = cur["surface_pressure"];
  w.uv = cur["uv_index"] | 0.0f;
  const char* curTime = cur["time"] | "";

  JsonObject daily = doc["daily"];
  JsonArray dTime = daily["time"];
  w.days = min((int)dTime.size(), Weather::DAYS);
  for (int i = 0; i < w.days; i++) {
    DayForecast& d = w.day[i];
    const char* date = dTime[i];
    d.year = atoi(date);
    d.month = atoi(date + 5);
    d.mday = atoi(date + 8);
    d.weekday = weekdayOf(d.year, d.month, d.mday);
    d.code = daily["weather_code"][i];
    d.tMax = daily["temperature_2m_max"][i];
    d.tMin = daily["temperature_2m_min"][i];
    d.precipProb = daily["precipitation_probability_max"][i] | 0;
    d.precipSum = daily["precipitation_sum"][i] | 0.0f;
    d.gustMax = daily["wind_gusts_10m_max"][i] | 0.0f;
    d.uvMax = daily["uv_index_max"][i] | 0.0f;
    d.sunrise = minutesOf(daily["sunrise"][i]);
    d.sunset = minutesOf(daily["sunset"][i]);
    d.daylight = daily["daylight_duration"][i] | 0.0f;
  }

  JsonObject hourly = doc["hourly"];
  JsonArray hTime = hourly["time"];
  w.hours = min((int)hTime.size(), Weather::HOURS);
  w.nowHour = 0;
  int dayIndex = 0;
  for (int i = 0; i < w.hours; i++) {
    const char* t = hTime[i];
    // Zuordnung zum Tag über das Datum, damit Tage mit 23 oder 25 Stunden stimmen
    while (dayIndex + 1 < w.days && atoi(t + 8) != w.day[dayIndex].mday) dayIndex++;
    w.hDay[i] = dayIndex;
    w.hHour[i] = atoi(t + 11);
    if (strncmp(t, curTime, 13) == 0) w.nowHour = i;
    w.hTemp[i] = hourly["temperature_2m"][i];
    w.hProb[i] = hourly["precipitation_probability"][i] | 0;
    w.hPrecip[i] = hourly["precipitation"][i] | 0.0f;
    w.hCode[i] = hourly["weather_code"][i] | 0;
    w.hWind[i] = hourly["wind_speed_10m"][i] | 0.0f;
    w.hGust[i] = hourly["wind_gusts_10m"][i] | 0.0f;
    w.hIsDay[i] = hourly["is_day"][i] == 1;
  }

  JsonObject quarter = doc["minutely_15"];
  JsonArray qTime = quarter["time"];
  w.quarters = min((int)qTime.size(), Weather::QUARTERS);
  for (int i = 0; i < w.quarters; i++) {
    const char* t = qTime[i];
    strlcpy(w.qTime[i], strlen(t) >= 16 ? t + 11 : "--:--", sizeof(w.qTime[i]));
    w.qPrecip[i] = quarter["precipitation"][i] | 0.0f;
  }

  w.valid = true;
  Serial.printf("Wetter: %.1f °C, Code %d, %d Stunden (jetzt %d), %d Tage\n", w.temp, w.code, w.hours, w.nowHour,
                w.days);
  return true;
}

bool nowcastText(const Weather& w, char* buf, size_t len) {
  constexpr float WET = 0.1f;  // mm je 15 Minuten
  if (w.quarters == 0) {
    buf[0] = 0;
    return false;
  }
  bool rainingNow = w.qPrecip[0] >= WET;
  for (int i = 1; i < w.quarters; i++) {
    if ((w.qPrecip[i] >= WET) != rainingNow) {
      snprintf(buf, len, rainingNow ? "Regen endet gegen %s" : "Regen ab ca. %s", w.qTime[i]);
      return true;
    }
  }
  strlcpy(buf, rainingNow ? "Regen hält an" : "Nächste 2 Stunden trocken", len);
  return rainingNow;
}

const char* weatherText(int code) {
  switch (code) {
    case 0: return "Klar";
    case 1: return "Überwiegend klar";
    case 2: return "Teilweise bewölkt";
    case 3: return "Bedeckt";
    case 45: return "Nebel";
    case 48: return "Nebel mit Reif";
    case 51: return "Leichter Nieselregen";
    case 53: return "Nieselregen";
    case 55: return "Starker Nieselregen";
    case 56:
    case 57: return "Gefrierender Niesel";
    case 61: return "Leichter Regen";
    case 63: return "Regen";
    case 65: return "Starker Regen";
    case 66:
    case 67: return "Gefrierender Regen";
    case 71: return "Leichter Schneefall";
    case 73: return "Schneefall";
    case 75: return "Starker Schneefall";
    case 77: return "Schneegriesel";
    case 80: return "Leichte Regenschauer";
    case 81: return "Regenschauer";
    case 82: return "Heftige Regenschauer";
    case 85:
    case 86: return "Schneeschauer";
    case 95: return "Gewitter";
    case 96:
    case 99: return "Gewitter mit Hagel";
    default: return "Unbekannt";
  }
}
