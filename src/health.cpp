#include "health.h"

#include <ArduinoJson.h>
#include <time.h>

#include "astro.h"
#include "log.h"
#include "net.h"

const char* const POLLEN_NAMES[Health::POLLEN] = {"Hasel", "Erle", "Esche", "Birke", "Gräser", "Roggen", "Beifuß", "Ambrosia"};
static const char* const POLLEN_KEYS[Health::POLLEN] = {"Hasel", "Erle", "Esche", "Birke", "Graeser", "Roggen", "Beifuss", "Ambrosia"};

const char* const BIO_NAMES[Health::EFFECTS] = {"Allgemeinbefinden", "Niedriger Blutdruck", "Herz, Bluthochdruck",
                                                "Rheuma, entzündlich", "Rheuma, degenerativ", "Asthma",
                                                "Wärme/Kälte"};
// Erkennungsmerkmal im Namen des DWD, gleiche Reihenfolge wie BIO_NAMES
static const char* const BIO_KEYS[Health::EFFECTS] = {"allgemeine Befinden", "hypotone", "hypertone", "entzündliche",
                                                      "degenerative", "Asthma", "Thermische"};

const PollenRegion POLLEN_REGIONS[] = {
    {11, "Schleswig-Holstein/Hamburg: Inseln und Marschen"},
    {12, "Schleswig-Holstein/Hamburg: Geest"},
    {20, "Mecklenburg-Vorpommern"},
    {31, "Niedersachsen/Bremen: westlicher Teil"},
    {32, "Niedersachsen/Bremen: östlicher Teil"},
    {41, "Nordrhein-Westfalen: Tiefland"},
    {42, "Nordrhein-Westfalen: Ostwestfalen"},
    {43, "Nordrhein-Westfalen: Mittelgebirge"},
    {50, "Brandenburg und Berlin"},
    {61, "Sachsen-Anhalt: Tiefland"},
    {62, "Sachsen-Anhalt: Harz"},
    {71, "Thüringen: Tiefland"},
    {72, "Thüringen: Mittelgebirge"},
    {81, "Sachsen: Tiefland"},
    {82, "Sachsen: Mittelgebirge"},
    {91, "Hessen: Nordhessen und Mittelgebirge"},
    {92, "Hessen: Rhein-Main"},
    {101, "Rheinland-Pfalz: Rhein, Pfalz, Nahe und Mosel"},
    {102, "Rheinland-Pfalz: Mittelgebirge"},
    {103, "Saarland"},
    {111, "Baden-Württemberg: Oberrhein und unteres Neckartal"},
    {112, "Baden-Württemberg: Hohenlohe/mittlerer Neckar/Oberschwaben"},
    {113, "Baden-Württemberg: Mittelgebirge"},
    {121, "Bayern: Allgäu/Oberbayern/Bayerischer Wald"},
    {122, "Bayern: Donauniederungen"},
    {123, "Bayern: nördlich der Donau"},
    {124, "Bayern: Mainfranken"},
};
const int POLLEN_REGION_COUNT = sizeof(POLLEN_REGIONS) / sizeof(POLLEN_REGIONS[0]);

const BioZone BIO_ZONES[] = {
    {'A', "Schleswig-Holstein, Hamburg, nördl. Niedersachsen, Bremen"},
    {'B', "Mecklenburg-Vorpommern"},
    {'C', "Südwestliches Niedersachsen, Nordrhein-Westfalen"},
    {'D', "Östliches und südliches Niedersachsen"},
    {'E', "Berlin, Brandenburg, nördl. Sachsen-Anhalt"},
    {'F', "Hessen, Rheinland-Pfalz, Saarland"},
    {'G', "Südl. Sachsen-Anhalt, Thüringen, Sachsen"},
    {'H', "Baden"},
    {'I', "Württemberg, Franken ohne östl. Oberfranken"},
    {'J', "Niederbayern, Oberpfalz, östl. Oberfranken"},
    {'K', "Schwaben, Oberbayern"},
};
const int BIO_ZONE_COUNT = sizeof(BIO_ZONES) / sizeof(BIO_ZONES[0]);

const char* pollenRegionName(int id) {
  for (int i = 0; i < POLLEN_REGION_COUNT; i++) {
    if (POLLEN_REGIONS[i].id == id) return POLLEN_REGIONS[i].name;
  }
  return "?";
}

const char* bioZoneName(char id) {
  for (int i = 0; i < BIO_ZONE_COUNT; i++) {
    if (BIO_ZONES[i].id == id) return BIO_ZONES[i].name;
  }
  return "?";
}

namespace {

constexpr const char* BASE = "https://opendata.dwd.de/climate_environment/health/alerts/";

int8_t pollenLevel(const char* v) {
  static const char* const LEVELS[] = {"0", "0-1", "1", "1-2", "2", "2-3", "3"};
  for (int i = 0; i < 7; i++) {
    if (!strcmp(v, LEVELS[i])) return i;
  }
  return -1;
}

// Tag aus „2026-10-18 11:00 Uhr“ bzw. „2026-10-18“
long dayOf(const char* s) { return s && strlen(s) >= 10 ? daysFromCivil(atoi(s), atoi(s + 5), atoi(s + 8)) : -1; }

int8_t bioLevel(const char* v, char* label, size_t len) {
  int8_t level = 0;
  const char* text = "–";
  if (strstr(v, "positiv")) {
    level = 1;
    text = "günstig";
  } else if (strstr(v, "hohe") || strstr(v, "starke")) {
    level = 4;
    text = "hoch";
  } else if (strstr(v, "mäßige") || strstr(v, "mittlere") || strstr(v, "negativ")) {
    level = 3;
    text = "mittel";
  } else if (strstr(v, "geringe") || strstr(v, "schwache")) {
    level = 2;
    text = "gering";
  } else if (strstr(v, "Wärme") || strstr(v, "Kälte")) {
    level = 3;
  }
  // Wärme/Kälte: Art statt Stärke zeigen, die Farbe zeigt die Stärke
  if (level > 0 && strstr(v, "Kälte")) text = "Kälte";
  if (level > 0 && strstr(v, "Wärme")) text = "Wärme";
  strlcpy(label, text, len);
  return level;
}

}  // namespace

bool fetchPollen(int region, Health& h) {
  JsonDocument doc;
  if (!httpGetJson(String(BASE) + "s31fg.json", doc)) return false;

  // „today“ bezieht sich auf den Ausgabetag, nicht auf heute
  long issued = dayOf(doc["last_update"] | ""), today = localDayNumber();
  int offset = issued >= 0 ? (int)(today - issued) : 0;
  for (JsonObject r : doc["content"].as<JsonArray>()) {
    int id = r["partregion_id"] | -1;
    if (id < 0) id = r["region_id"] | -1;
    if (id != region) continue;
    static const char* const DAYS[] = {"today", "tomorrow", "dayafter_to"};
    for (int p = 0; p < Health::POLLEN; p++) {
      JsonObject values = r["Pollen"][POLLEN_KEYS[p]];
      for (int d = 0; d < 3; d++) {
        int src = d + offset;
        h.pollen[p][d] = src >= 0 && src < 3 ? pollenLevel(values[DAYS[src]] | "-1") : -1;
      }
    }
    h.pollenValid = true;
    logf("Pollenflug: Region %d, Ausgabe vor %d Tag(en)", region, offset);
    return true;
  }
  logf("Pollenflug: Region %d nicht gefunden", region);
  return false;
}

bool fetchBio(char zone, Health& h) {
  static const char* const PERIODS[] = {"today_morning",   "today_afternoon",   "tomorrow_morning",
                                        "tomorrow_afternoon", "dayafter_to_morning", "dayafter_to_afternoon"};
  // Nur benötigte Felder übernehmen, die Datei ist über 100 KB groß
  JsonDocument filter;
  filter["zone"][0]["id"] = true;
  for (const char* p : PERIODS) {
    JsonObject f = filter["zone"][0][p].to<JsonObject>();
    f["date"] = true;
    f["name"] = true;
    f["effect"][0]["name"] = true;
    f["effect"][0]["value"] = true;
    f["recomms"][0]["value"] = true;
  }
  JsonDocument doc;
  if (!httpGetJson(String(BASE) + "biowetter.json", doc, filter)) return false;

  JsonObject z;
  for (JsonObject candidate : doc["zone"].as<JsonArray>()) {
    const char* id = candidate["id"] | "";
    if (id[0] == zone) z = candidate;
  }
  if (z.isNull()) {
    logf("Biowetter: Zone %c nicht gefunden", zone);
    return false;
  }

  for (int e = 0; e < Health::EFFECTS; e++) {
    for (int d = 0; d < 2; d++) {
      h.bio[e][d] = -1;
      strlcpy(h.bioLabel[e][d], "?", sizeof(h.bioLabel[e][d]));
    }
  }
  h.advice[0] = 0;

  // Heute: aktuelle Tageshälfte, sonst die andere; morgen: die ungünstigere Hälfte
  struct tm now;
  time_t t = time(nullptr);
  localtime_r(&t, &now);
  const char* half = now.tm_hour < 12 ? "1." : "2.";
  long today = localDayNumber();
  for (int pass = 0; pass < 2; pass++) {
    for (const char* p : PERIODS) {
      JsonObject period = z[p];
      if (period.isNull()) continue;
      long day = dayOf(period["date"] | "");
      int d = day == today ? 0 : day == today + 1 ? 1 : -1;
      if (d < 0) continue;
      bool currentHalf = strncmp(period["name"] | "", half, 2) == 0;
      // Erster Durchgang: heute nur die aktuelle Hälfte; zweiter: Lücken füllen
      if (d == 0 && pass == 0 && !currentHalf) continue;
      for (JsonObject eff : period["effect"].as<JsonArray>()) {
        const char* name = eff["name"] | "";
        for (int e = 0; e < Health::EFFECTS; e++) {
          if (!strstr(name, BIO_KEYS[e])) continue;
          char label[16];
          int8_t level = bioLevel(eff["value"] | "", label, sizeof(label));
          bool take = d == 0 ? h.bio[e][0] < 0 : level > h.bio[e][1];
          if (take) {
            h.bio[e][d] = level;
            strlcpy(h.bioLabel[e][d], label, sizeof(h.bioLabel[e][d]));
          }
        }
      }
      if (d == 0 && !h.advice[0]) {
        // Empfehlungen ohne Wiederholungen zusammenfassen
        for (JsonObject r : period["recomms"].as<JsonArray>()) {
          const char* v = r["value"] | "";
          if (!*v || strstr(h.advice, v)) continue;
          if (h.advice[0]) strlcat(h.advice, " · ", sizeof(h.advice));
          strlcat(h.advice, v, sizeof(h.advice));
        }
      }
    }
  }
  h.bioValid = true;
  logf("Biowetter: Zone %c", zone);
  return true;
}
