#pragma once

#include <Arduino.h>

struct Location {
  char city[48] = "";
  float lat = 0;
  float lon = 0;
};

struct DayForecast {
  int year, month, mday;
  int weekday;  // 0 = Sonntag
  int code;
  float tMax;
  float tMin;
  int precipProb;  // höchste Regenwahrscheinlichkeit in %
  float precipSum;  // mm
  float gustMax;    // km/h
  float uvMax;
  int sunrise;  // Minuten seit Mitternacht
  int sunset;
  float daylight;  // Sekunden
};

struct Weather {
  // Open-Meteo liefert gestern und 7 Tage: Tag 0 = gestern, Tag 1 = heute
  static constexpr int DAYS = 8;
  static constexpr int HOURS = DAYS * 24 + 2;  // Reserve für die Umstellung auf Winterzeit
  static constexpr int QUARTERS = 8;           // Niederschlag in 15-Minuten-Schritten, 2 Stunden
  static constexpr int TODAY = 1;

  bool valid = false;
  int utcOffset = 0;

  float temp;
  float feels;
  float wind;
  float gusts;
  float pressure;
  float uv;
  int humidity;
  int windDir;
  int code;
  bool isDay;

  int hours;
  int nowHour;  // Index der aktuellen Stunde in den Stundenwerten
  uint8_t hHour[HOURS];
  uint8_t hDay[HOURS];  // Index in day[]
  uint8_t hCode[HOURS];
  uint8_t hProb[HOURS];
  uint8_t hHumidity[HOURS];
  bool hIsDay[HOURS];
  float hTemp[HOURS];
  float hPrecip[HOURS];  // mm
  float hWind[HOURS];
  float hGust[HOURS];

  int days;
  DayForecast day[DAYS];

  int quarters;
  char qTime[QUARTERS][6];  // "HH:MM"
  float qPrecip[QUARTERS];  // mm je 15 Minuten
};

// Bestimmt den Standort aus den Einstellungen: Koordinaten, sonst Ortsname, sonst IP-Adresse
bool locate(Location& loc);
// Sucht einen Ort per Name (Open-Meteo-Geocoding); bevorzugt Treffer in Deutschland
bool geocode(const char* name, Location& loc);
bool fetchWeather(const Location& loc, Weather& w);
const char* weatherText(int code);
// Kurzer Hinweis zum Regen der nächsten 2 Stunden, z. B. „Regen ab ca. 14:30“.
// Liefert true, wenn es um Regen geht (für die Farbe), sonst „trocken“.
bool nowcastText(const Weather& w, char* buf, size_t len);
