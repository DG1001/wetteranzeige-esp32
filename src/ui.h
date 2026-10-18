#pragma once

#include "weather.h"

// Vollbild-Meldung, z. B. während der WLAN-Verbindung
void uiStatus(const char* title, const char* detail);
// status erscheint klein unter der Uhr; warn färbt ihn als Warnung ein
void uiWeather(const Location& loc, const Weather& w, const char* status, bool warn);
