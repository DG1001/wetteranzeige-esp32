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
// Kopiert das sichtbare Bild in die Zeichenfläche. Danach unterscheiden sich beide Speicher nur
// dort, wo neu gezeichnet wird; Animationen müssen dann nur ihren Ausschnitt erneuern.
void syncBack();
// Sichert bzw. stellt einen Ausschnitt der Zeichenfläche wieder her (buf: w*h Pixel)
void saveRect(uint16_t* buf, int x, int y, int w, int h);
void restoreRect(const uint16_t* buf, int x, int y, int w, int h);
void setBrightness(uint8_t value);
// Pixeltakt im Betrieb ändern (wirkt ab dem nächsten Bild), z. B. zum Vergleich der Bildwiederholrate
void setPixelClock(uint32_t hz);
// Das gerade angezeigte Bild (RGB565 mit vertauschten Bytes), z. B. für Bildschirmfotos
const uint16_t* front();

}  // namespace lcd
