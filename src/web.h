#pragma once

#include "model.h"

// Webseite für Einstellungen, Bildschirmfoto und Firmware-Update sowie Einrichtungs-Hotspot
namespace web {

constexpr const char* HOSTNAME = "wetteranzeige";
constexpr const char* AP_NAME = "Wetteranzeige";

struct Changes {
  bool location = false;  // Ort geändert: Daten neu laden
  bool display = false;   // Helligkeit oder Nachtmodus geändert
};

// Normalbetrieb im Heimnetz; onChange wird nach dem Speichern von Einstellungen aufgerufen
void beginStation(const Model& model, void (*onChange)(const Changes&));
// Einrichtungs-Hotspot; nach dem Speichern startet das Board neu
void beginPortal();
void loop();

}  // namespace web
