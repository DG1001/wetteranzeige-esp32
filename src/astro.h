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
