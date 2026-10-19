#pragma once

#include "weather.h"

// Wasserbilanz für den Garten: Niederschlag minus Verdunstung (FAO-Referenzverdunstung)
struct Garden {
  static constexpr int PAST = 7;   // vergangene Tage für die Bilanz
  static constexpr int DAYS = PAST + 3;

  bool valid = false;
  int days = 0;
  int weekday[DAYS];  // 0 = Sonntag
  float rain[DAYS];   // mm, Index PAST = heute
  float et0[DAYS];    // mm
  float balance;      // Summe der vergangenen PAST Tage, negativ = zu trocken
  float rainSoon;     // erwarteter Regen heute und morgen
};

bool fetchGarden(const Location& loc, Garden& g);
