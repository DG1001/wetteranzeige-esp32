#pragma once

#include <LovyanGFX.hpp>

#include "model.h"
#include "theme.h"

// Gemeinsame Bausteine aller Seiten
namespace ui {

constexpr int W = 800;
constexpr int H = 480;
constexpr int HEADER_H = 80;

// Warnsymbol in der Kopfzeile
constexpr int PILL_X = 270, PILL_Y = 12, PILL_W = 260, PILL_H = 42;

constexpr uint32_t RAIN = 0x5AAEFFu;
constexpr uint32_t RAIN_DIM = 0x2F6DAAu;
constexpr uint32_t CURVE = 0xFFC23Du;
constexpr uint32_t WARN = 0xFF8A65u;

extern LGFX_Sprite& canvas;

extern const uint8_t* const FONT_SMALL;  // 18 px
extern const uint8_t* const FONT_TEXT;   // 24 px
extern const uint8_t* const FONT_TITLE;  // 22 px fett
extern const uint8_t* const FONT_BOLD;   // 30 px fett
extern const uint8_t* const FONT_CLOCK;  // 52 px, nur Ziffern
extern const uint8_t* const FONT_HUGE;   // 112 px, nur Ziffern

extern const char* const WEEKDAYS[7];
extern const char* const WEEKDAYS_SHORT[7];
extern const char* const MONTHS[12];

void font(const uint8_t* f);
void text(const char* s, int x, int y, const uint8_t* f, uint32_t color, textdatum_t datum = top_left);
int textWidth(const char* s, const uint8_t* f);
// Bricht an Leerzeichen und Zeilenumbrüchen um; liefert die y-Position unter der letzten Zeile.
// Was unterhalb von maxY läge, wird weggelassen und durch „…“ angedeutet.
int wrapText(const char* s, int x, int y, int w, const uint8_t* f, uint32_t color, int lineHeight, int maxY);
// Anzahl der Zeilen, die wrapText bei Breite w bräuchte
int wrapLines(const char* s, int w, const uint8_t* f);
void formatTemp(char* buf, size_t len, float t);
// Großbuchstaben des DWD („STURMBÖEN“) in normale Schreibweise („Sturmböen“)
void titleCase(const char* in, char* out, size_t len);
uint32_t warnColor(int level);
bool inRect(int x, int y, int rx, int ry, int rw, int rh);
// Ortszeit; false, solange die Uhr nicht per NTP gestellt ist
bool localNow(struct tm& tm);
// Minuten seit Mitternacht (Ortszeit), -1 solange die Uhr nicht gestellt ist
int minuteOfDay();
// Dezimalzahl mit Komma, z. B. „2,4“
void formatDecimal(char* buf, size_t len, float v, int decimals);

// Himmelsverlauf mit Dithering, damit in RGB565 keine Farbstufen sichtbar werden
void background(const Theme& t);
// Halbtransparent wirkende Karte über dem Himmelsverlauf
void glass(const Theme& t, int x, int y, int w, int h, int r = 14);
void header(const Model& m, const Theme& t);
void tempCurve(int count, const float* temps, int x0, int x1, int top, int bottom, int labelEvery, uint32_t labelColor,
               const int* labelIndex = nullptr);

}  // namespace ui
