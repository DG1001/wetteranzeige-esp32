#pragma once

#include <LovyanGFX.hpp>

// RGB-Display des ESP32-8048S043 über den ESP-IDF-Treiber mit Bounce Buffer und zwei Bildspeichern.
// Gezeichnet wird mit LovyanGFX in den gerade unsichtbaren Bildspeicher, lcdShow() schaltet um.
namespace lcd {

constexpr int WIDTH = 800;
constexpr int HEIGHT = 480;

bool begin();
// Zeichenfläche für das nächste Bild; ihr Inhalt ist veraltet und muss komplett neu gezeichnet werden
LGFX_Sprite& canvas();
// Zeigt das gezeichnete Bild ab dem nächsten Bildwechsel an
void show();
void setBrightness(uint8_t value);

}  // namespace lcd
