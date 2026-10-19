#pragma once

#include <LovyanGFX.hpp>
#include <time.h>

#include "weather.h"

// Regenradar des DWD: Karte und Bilder von 1 Stunde vor bis 2 Stunden nach jetzt.
// Geladen wird schrittweise (ein Bild je Aufruf), damit die Oberfläche bedienbar bleibt.
namespace radar {

constexpr int WIDTH = 800;
constexpr int HEIGHT = 398;
constexpr int FRAMES = 19;     // alle 10 Minuten von -60 bis +120 Minuten
constexpr int NOW_FRAME = 6;   // Bild der letzten Messung
constexpr int STEP_MIN = 10;

struct City {
  const char* name;
  float lat, lon;
};

void begin(const Location& loc);
// true, solange noch Karte oder Bilder fehlen bzw. veraltet sind
bool needsLoad();
// Lädt das nächste fehlende Teil; true bei Erfolg
bool loadStep();
// Anteil der geladenen Teile, 0 bis 1
float progress();
bool hasMap();
// Bild verfügbar?
bool hasFrame(int i);
time_t frameTime(int i);  // UTC
// Zeichnet Karte und Radarbild i in die Zeichenfläche ab Zeile y
void draw(LGFX_Sprite& canvas, int y, int frame);
// Bildposition eines Orts; false, wenn außerhalb der Karte
bool project(float lat, float lon, int& x, int& y);
// Orte für die Beschriftung, nach Wichtigkeit sortiert
const City* cities(int& count);

}  // namespace radar
