#pragma once

#include <LovyanGFX.hpp>

// Zeichnet ein Wettersymbol (WMO-Code) zentriert in ein Quadrat der Kantenlänge size.
// bg ist die Hintergrundfarbe darunter (für die Mondsichel).
void drawWeatherIcon(lgfx::LovyanGFX& g, int code, bool isDay, int cx, int cy, int size, uint32_t bg);
