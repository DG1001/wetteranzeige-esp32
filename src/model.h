#pragma once

#include "air.h"
#include "warnings.h"
#include "weather.h"

// Seiten; die ersten vier erreicht man per Wischen, die übrigen per Antippen
enum class Page { Overview, Radar, Hours, SkyAir, Day, Warnings };
constexpr int MAIN_PAGES = 4;

struct Model {
  Location loc;
  Weather weather;
  Air air;
  Warnings warnings;

  Page page = Page::Overview;
  int day = Weather::TODAY;  // Tag für die Tagesansicht
  bool radarPaused = false;

  char status[64] = "";  // klein unter der Uhr
  bool statusWarn = false;
};
