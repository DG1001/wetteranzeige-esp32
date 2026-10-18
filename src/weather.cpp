#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

static bool httpGet(const String& url, JsonDocument& doc) {
  WiFiClientSecure secure;
  WiFiClient plain;
  HTTPClient http;
  bool https = url.startsWith("https");
  if (https) secure.setInsecure();
  if (!http.begin(https ? static_cast<WiFiClient&>(secure) : plain, url)) return false;
  http.setTimeout(10000);

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("HTTP %d bei %s\n", status, url.c_str());
    http.end();
    return false;
  }
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) {
    Serial.printf("JSON-Fehler: %s\n", err.c_str());
    return false;
  }
  return true;
}

bool locate(Location& loc) {
  if (strlen(LOCATION_NAME) > 0) {
    strlcpy(loc.city, LOCATION_NAME, sizeof(loc.city));
    loc.lat = LOCATION_LAT;
    loc.lon = LOCATION_LON;
    return true;
  }
  JsonDocument doc;
  if (!httpGet("http://ip-api.com/json/?fields=status,city,lat,lon&lang=de", doc)) return false;
  if (doc["status"] != "success") return false;
  strlcpy(loc.city, doc["city"] | "", sizeof(loc.city));
  loc.lat = doc["lat"];
  loc.lon = doc["lon"];
  Serial.printf("Standort: %s (%.4f, %.4f)\n", loc.city, loc.lat, loc.lon);
  return true;
}

// Wochentag aus "JJJJ-MM-TT" (Sakamoto-Verfahren), 0 = Sonntag
static int weekdayOf(const char* date) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  int y = atoi(date), m = atoi(date + 5), d = atoi(date + 8);
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// Uhrzeit "HH:MM" aus "JJJJ-MM-TTTHH:MM"
static void copyTime(char* dst, const char* iso) {
  strlcpy(dst, iso && strlen(iso) >= 16 ? iso + 11 : "--:--", 6);
}

bool fetchWeather(const Location& loc, Weather& w) {
  char url[512];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,"
           "wind_speed_10m,wind_direction_10m,is_day,surface_pressure"
           "&hourly=temperature_2m,precipitation_probability"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
           "sunrise,sunset"
           "&timezone=auto&forecast_days=%d&forecast_hours=%d",
           loc.lat, loc.lon, Weather::DAYS, Weather::HOURS);

  JsonDocument doc;
  if (!httpGet(url, doc)) return false;

  w.utcOffset = doc["utc_offset_seconds"] | 0;

  JsonObject cur = doc["current"];
  w.temp = cur["temperature_2m"];
  w.feels = cur["apparent_temperature"];
  w.humidity = cur["relative_humidity_2m"];
  w.code = cur["weather_code"];
  w.wind = cur["wind_speed_10m"];
  w.windDir = cur["wind_direction_10m"];
  w.isDay = cur["is_day"] == 1;
  w.pressure = cur["surface_pressure"];

  JsonObject hourly = doc["hourly"];
  JsonArray hTime = hourly["time"];
  w.hours = min((int)hTime.size(), Weather::HOURS);
  for (int i = 0; i < w.hours; i++) {
    const char* t = hTime[i];
    w.hHour[i] = atoi(t + 11);
    w.hTemp[i] = hourly["temperature_2m"][i];
    w.hPrecip[i] = hourly["precipitation_probability"][i] | 0;
  }

  JsonObject daily = doc["daily"];
  JsonArray dTime = daily["time"];
  w.days = min((int)dTime.size(), Weather::DAYS);
  for (int i = 0; i < w.days; i++) {
    DayForecast& d = w.day[i];
    d.weekday = weekdayOf(dTime[i]);
    d.code = daily["weather_code"][i];
    d.tMax = daily["temperature_2m_max"][i];
    d.tMin = daily["temperature_2m_min"][i];
    d.precip = daily["precipitation_probability_max"][i] | 0;
  }
  copyTime(w.sunrise, daily["sunrise"][0]);
  copyTime(w.sunset, daily["sunset"][0]);

  w.valid = true;
  Serial.printf("Wetter: %.1f °C, Code %d, %d Stunden, %d Tage\n", w.temp, w.code, w.hours, w.days);
  return true;
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
