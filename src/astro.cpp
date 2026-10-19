#include "astro.h"

#include <math.h>

namespace {

constexpr float SYNODIC_MONTH = 29.530589f;  // Tage
constexpr time_t REFERENCE_NEW_MOON = 947182440;  // 2000-01-06 18:14 UTC

const char* phaseName(float age) {
  if (age < 1.0f || age >= 28.5f) return "Neumond";
  if (age < 6.4f) return "Zunehmende Sichel";
  if (age < 8.4f) return "Erstes Viertel";
  if (age < 13.8f) return "Zunehmender Mond";
  if (age < 15.8f) return "Vollmond";
  if (age < 21.1f) return "Abnehmender Mond";
  if (age < 23.1f) return "Letztes Viertel";
  return "Abnehmende Sichel";
}

}  // namespace

// Mittlerer Mondlauf; die echte Phase weicht davon um bis zu etwa einen halben Tag ab
MoonPhase moonPhase(time_t utc) {
  MoonPhase m;
  double days = (utc - REFERENCE_NEW_MOON) / 86400.0;
  m.age = fmod(days, SYNODIC_MONTH);
  if (m.age < 0) m.age += SYNODIC_MONTH;
  m.illumination = (1 - cosf(2 * M_PI * m.age / SYNODIC_MONTH)) / 2;
  m.name = phaseName(m.age);
  float toFull = fmodf(SYNODIC_MONTH / 2 - m.age + SYNODIC_MONTH, SYNODIC_MONTH);
  m.nextFull = utc + (time_t)(toFull * 86400);
  m.nextNew = utc + (time_t)((SYNODIC_MONTH - m.age) * 86400);
  return m;
}
