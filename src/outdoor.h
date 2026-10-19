#pragma once

#include "garden.h"
#include "health.h"
#include "weather.h"

// Auswertung der Vorhersage für Aktivitäten draußen: bestes Zeitfenster und Hinweise
enum class HintKind {
  Thunder, Ice, Storm, HeavyRain, Snow, Frost, GroundFrost, Umbrella, Heat, Uv, Fog, Wind, Laundry, Pollen, Bio, Water
};

struct Hint {
  HintKind kind;
  int priority;  // höher = wichtiger
  char text[80];
};

struct Outdoor {
  // Bestes Zeitfenster am Tag `day` (heute oder, abends, morgen)
  int day = Weather::TODAY;
  bool found = false;
  int startHour, endHour;  // Uhrzeit, Ende ausschließlich
  char summary[64];        // z. B. „trocken · 15–17° · wenig Wind“

  // Bewertung je Stunde des Tages `day` (0 = schlecht bzw. Nacht, 100 = ideal)
  int firstIndex, count;  // Bereich in den Stundenwerten
  uint8_t score[26];

  Hint hints[10];
  int hintCount = 0;
};

constexpr int GOOD_SCORE = 50;

void evaluateOutdoor(const Weather& w, const Health& health, const Garden& garden, Outdoor& out);
// Kurztext für die Übersicht, z. B. „Draußen ideal: 14–17 Uhr“
void bestTimeText(const Outdoor& o, char* buf, size_t len);
