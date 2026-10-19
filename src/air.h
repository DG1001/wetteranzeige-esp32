#pragma once

#include "weather.h"

struct Air {
  static constexpr int POLLEN = 5;

  bool valid = false;
  int aqi;  // Europäischer Luftqualitätsindex
  float pm25;
  float pm10;
  float ozone;
  float no2;
  float pollenToday[POLLEN];  // höchster Wert der restlichen Stunden heute, Körner/m³
};

// Reihenfolge der Pollenwerte in Air::pollenToday
extern const char* const POLLEN_NAMES[Air::POLLEN];

bool fetchAir(const Location& loc, Air& air);
// 0 = keine, 1 = gering, 2 = mittel, 3 = hoch (grobe Einteilung je Pollenart)
int pollenLevel(int type, float value);
