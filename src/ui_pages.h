#pragma once

#include "model.h"
#include "theme.h"

// Seiteninhalte unterhalb der Kopfzeile; der Hintergrund ist bereits gezeichnet
namespace ui {

// Übersicht: Bereich des animierten Wettersymbols
constexpr int ICON_CX = 120, ICON_CY = 172, ICON_SIZE = 170;
constexpr int ICON_X = 20, ICON_Y = 82, ICON_W = 200, ICON_H = 182;
// Übersicht: Stundenkarte und Tageskacheln (für das Antippen)
constexpr int HOURLY_X = 16, HOURLY_Y = 270, HOURLY_W = 768, HOURLY_H = 92;
constexpr int TILES = 6, TILE_X = 16, TILE_Y = 370, TILE_H = 102, TILE_GAP = 8;
constexpr int TILE_W = (768 - TILE_GAP * (TILES - 1)) / TILES;

void overview(const Model& m, const Theme& t);  // ohne das große Wettersymbol
void hours(const Model& m, const Theme& t);
void day(const Model& m, const Theme& t);
void skyAir(const Model& m, const Theme& t);
void warnings(const Model& m, const Theme& t);
void outdoor(const Model& m, const Theme& t);
void night(const Model& m);  // eigener schwarzer Hintergrund, ohne Kopfzeile
void info(const Model& m, const Theme& t);

// Übersicht: wechselnde Zeile unter der Beschreibung
constexpr int TICKER_X = 224, TICKER_Y = 240, TICKER_W = 272, TICKER_H = 28;

}  // namespace ui
