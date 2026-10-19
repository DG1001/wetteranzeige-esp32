#pragma once

#include <time.h>

struct MoonPhase {
  float age;           // Tage seit Neumond, 0 bis 29,53
  float illumination;  // beleuchteter Anteil 0 bis 1
  const char* name;
  time_t nextFull;
  time_t nextNew;
};

MoonPhase moonPhase(time_t utc);

// Tage seit 1970-01-01 für ein Datum (Algorithmus von Howard Hinnant)
long daysFromCivil(int y, int m, int d);
// Tage seit 1970-01-01 für das heutige Datum in Ortszeit
long localDayNumber();
