#pragma once

#include <stdint.h>

#include "weather.h"

struct Theme {
  uint32_t top;     // Farbe oben im Himmelsverlauf
  uint32_t bottom;  // Farbe unten
  uint32_t text;
  uint32_t dim;
  uint32_t glass;  // Tönung der Karten
  uint8_t glassAlpha;
  bool night;
};

// Himmel passend zu Tageszeit (Dämmerung um Sonnenauf- und -untergang) und Bewölkung
Theme skyTheme(const Weather& w, int minuteOfDay);
uint32_t mixColor(uint32_t a, uint32_t b, float t);
// Farbe des Verlaufs in Höhe y (0 bis 479)
uint32_t skyAt(const Theme& t, int y);
