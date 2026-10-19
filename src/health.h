#pragma once

#include <stdint.h>

// Pollenflug-Gefahrenindex und Biowetter des DWD (Medizin-Meteorologie)
struct Health {
  static constexpr int POLLEN = 8;
  static constexpr int EFFECTS = 7;

  bool pollenValid = false;
  // Belastung in halben Stufen: 0 = 0, 1 = 0–1, 2 = 1, … 6 = 3; -1 = keine Angabe.
  // Tage: heute, morgen, übermorgen
  int8_t pollen[POLLEN][3];

  bool bioValid = false;
  // 0 kein Einfluss, 1 günstig, 2 gering, 3 mittel, 4 hoch; -1 keine Angabe. Tage: heute, morgen
  int8_t bio[EFFECTS][2];
  char bioLabel[EFFECTS][2][16];  // Kurztext, z. B. „gering“ oder „Kälte“
  char advice[180];               // Empfehlung des DWD für heute
};

struct PollenRegion {
  int id;  // Teilregion, bei Regionen ohne Teilregionen die Region
  const char* name;
};
struct BioZone {
  char id;
  const char* name;
};

extern const char* const POLLEN_NAMES[Health::POLLEN];
extern const char* const BIO_NAMES[Health::EFFECTS];
extern const PollenRegion POLLEN_REGIONS[];
extern const int POLLEN_REGION_COUNT;
extern const BioZone BIO_ZONES[];
extern const int BIO_ZONE_COUNT;

const char* pollenRegionName(int id);
const char* bioZoneName(char id);

bool fetchPollen(int region, Health& h);
bool fetchBio(char zone, Health& h);
