#pragma once

#include "model.h"

namespace ui {

enum class Target { None, Refresh, Warnings, Hours, Day, Back, Pause };

// Vollbild-Meldung, z. B. während der WLAN-Verbindung
void status(const char* title, const char* detail);
// Zeichnet die aktuelle Seite komplett neu
void render(const Model& m);
// Ob die Seite bewegte Teile hat
bool animated(const Model& m);
// Erneuert die bewegten Teile; t = Sekunden seit render(), force = auch ohne Änderung neu zeichnen
void animate(const Model& m, float t, bool force);
// Was an der Stelle (x, y) angetippt wurde; day erhält bei Target::Day den Tag
Target hit(const Model& m, int x, int y, int& day);

}  // namespace ui
