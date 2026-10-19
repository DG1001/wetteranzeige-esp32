#pragma once

// Touch-Controller GT911 des ESP32-8048S043
namespace touch {

enum class Gesture { None, Tap, SwipeLeft, SwipeRight };

bool begin();
// Wertet Berührungen aus; meldet eine Geste, sobald der Finger losgelassen wurde.
// x/y: Startpunkt der Berührung in Display-Pixeln
Gesture poll(int& x, int& y);

}  // namespace touch
