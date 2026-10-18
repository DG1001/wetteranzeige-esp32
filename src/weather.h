#pragma once

#include <Arduino.h>

struct Location {
  char city[48] = "";
  float lat = 0;
  float lon = 0;
};

struct DayForecast {
  int weekday;  // 0 = Sonntag
  int code;
  float tMax;
  float tMin;
  int precip;  // Regenwahrscheinlichkeit in %
};

struct Weather {
  static constexpr int HOURS = 24;
  static constexpr int DAYS = 6;

  bool valid = false;
  int utcOffset = 0;

  float temp;
  float feels;
  float wind;
  float pressure;
  int humidity;
  int windDir;
  int code;
  bool isDay;
  char sunrise[6];
  char sunset[6];

  int hours;
  float hTemp[HOURS];
  int hPrecip[HOURS];
  int hHour[HOURS];

  int days;
  DayForecast day[DAYS];
};

// Bestimmt den Standort über config.h oder die IP-Adresse
bool locate(Location& loc);
bool fetchWeather(const Location& loc, Weather& w);
const char* weatherText(int code);
