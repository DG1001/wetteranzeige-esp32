#pragma once

#include "air.h"
#include "outdoor.h"
#include "warnings.h"
#include "weather.h"

// Seiten; die ersten fünf erreicht man per Wischen, die übrigen per Antippen, langes Drücken bzw. nachts
enum class Page { Overview, Outdoor, Radar, Hours, SkyAir, Day, Warnings, Night, Info };
constexpr int MAIN_PAGES = 5;

struct Model {
  Location loc;
  Weather weather;
  Air air;
  Warnings warnings;
  Outdoor outdoor;

  Page page = Page::Overview;
  int day = Weather::TODAY;  // Tag für die Tagesansicht
  bool radarPaused = false;

  char status[64] = "";  // klein unter der Uhr
  bool statusWarn = false;
};
