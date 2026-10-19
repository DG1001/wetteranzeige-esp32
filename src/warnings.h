#pragma once

#include "weather.h"

struct Warning {
  int level;  // 1 = gelb, 2 = orange, 3 = rot, 4 = violett
  char event[40];
  char headline[100];
  char description[640];
  char instruction[400];
  char period[48];  // z. B. „Mo 14:00 – Di 08:00“
};

struct Warnings {
  static constexpr int MAX = 6;
  bool valid = false;
  int count = 0;
  Warning item[MAX];  // nach Stufe absteigend sortiert
};

// Amtliche Warnungen des DWD für die Gemeinde am Standort (nur Deutschland)
bool fetchWarnings(const Location& loc, int utcOffset, Warnings& out);
