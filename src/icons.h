#pragma once

#include <LovyanGFX.hpp>

// Zeichnet ein Wettersymbol (WMO-Code) zentriert in ein Quadrat der Kantenlänge size.
// bg ist die Hintergrundfarbe darunter (Mondsichel, Ein- und Ausblenden).
// t ist die Animationszeit in Sekunden; bei t < 0 wird das Symbol unbewegt gezeichnet.
void drawWeatherIcon(lgfx::LovyanGFX& g, int code, bool isDay, int cx, int cy, int size, uint32_t bg,
                     float t = -1);
