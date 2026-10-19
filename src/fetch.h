#pragma once

#include "model.h"

// Alle Abrufe laufen in einem eigenen Task, damit Touch und Animation nie auf das Netz warten.
// Ergebnisse werden unter ModelLock ins Modell übernommen.
namespace fetch {

struct Source {
  const char* name;
  uint32_t lastTry = 0;  // millis()
  uint32_t lastOk = 0;
  bool ok = false;
  uint32_t failures = 0;
};

enum SourceId { WEATHER, WARNINGS, AIR, POLLEN, BIO, GARDEN, RADAR, SOURCES };

void begin(Model& model);
// Alles sofort neu abrufen (Antippen der Übersicht)
void refreshAll();
// Ort oder Regionen wurden geändert
void settingsChanged();
// Abrufe anhalten, z. B. während eines Firmware-Updates (sonst konkurrieren beide um WLAN und Speicher)
void pause(bool paused);
// Neue Daten seit dem letzten Aufruf (Oberfläche neu zeichnen)?
bool takeChanged();
// Neue Radarbilder seit dem letzten Aufruf?
bool takeRadarChanged();
// Zeitpunkt (millis) des letzten Durchlaufs der Abrufschleife, zur Überwachung
uint32_t heartbeat();
const Source& source(int id);
// Kurztext für den Startbildschirm, solange es noch keine Wetterdaten gibt
const char* startupMessage();

}  // namespace fetch
