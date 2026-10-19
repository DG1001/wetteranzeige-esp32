#pragma once

#include <Arduino.h>

// Einstellungen im Flash (NVS). Beim ersten Start kommen sie aus config.h und, falls vorhanden, secrets.h.
struct Settings {
  char ssid[33] = "";
  char password[65] = "";
  char city[48] = "";  // leer = Ort über die IP-Adresse bestimmen
  float lat = 0;
  float lon = 0;
  uint8_t brightDay;
  uint8_t brightNight;
  bool nightEnabled;
  uint16_t nightStart;  // Minuten seit Mitternacht
  uint16_t nightEnd;
  bool nightOff;  // true = Display aus, false = gedimmte Uhr
  uint8_t brightClock;  // Helligkeit der Nachtuhr
  char pin[9] = "";  // Zugang zur Webseite, zum Einrichtungs-WLAN und für Updates
  // Neue Felder nur hinten anfügen: ältere gespeicherte Einstellungen werden dann übernommen
  int16_t pollenRegion;  // DWD-Pollenregion (Teilregion), siehe health.cpp
  char bioZone;          // DWD-Biowetterzone 'A' bis 'K'
};

extern Settings settings;

// Wird beim Bauen aus Git gesetzt (tools/version.py)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unbekannt"
#endif

void loadSettings();
void saveSettings();
// Liegt die Minute (seit Mitternacht) im Nachtzeitraum? Der Zeitraum darf über Mitternacht gehen.
bool inNightWindow(int minute);
