#pragma once

// Touch-Controller GT911 des ESP32-8048S043
namespace touch {

bool begin();
// Liefert true, solange ein Finger aufliegt; x/y in Display-Pixeln
bool read(int& x, int& y);

}  // namespace touch
