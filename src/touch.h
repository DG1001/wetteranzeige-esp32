#pragma once

// Touch-Controller GT911 des ESP32-8048S043
namespace touch {

enum class Gesture { None, Tap, SwipeLeft, SwipeRight, LongPress };

bool begin();
// Wertet Berührungen aus; meldet eine Geste, sobald der Finger losgelassen wurde.
// Langes Drücken (2 s ohne Bewegung) wird schon während der Berührung gemeldet.
// x/y: Startpunkt der Berührung in Display-Pixeln
Gesture poll(int& x, int& y);

}  // namespace touch
