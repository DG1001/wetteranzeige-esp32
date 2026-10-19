#pragma once

#include "weather.h"

struct Air {
  bool valid = false;
  int aqi;  // Europäischer Luftqualitätsindex
  float pm25;
  float pm10;
  float ozone;
  float no2;
};

bool fetchAir(const Location& loc, Air& air);
