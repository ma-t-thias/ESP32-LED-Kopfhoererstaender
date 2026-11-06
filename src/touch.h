#pragma once
#include <stdint.h>
// Touch des ESP32-C6-Touch-LCD-1.9 (CST816D). Bewusst einfach: Tippen (obere/untere Bildhaelfte)
// und Wischen links/rechts.
// Boards ohne Touch (kein TOUCH_SDA in config.h) liefern nie eine Geste.
namespace touch {
enum Gesture { NONE, TAP_TOP, TAP_BOTTOM, SWIPE_RIGHT, SWIPE_LEFT };
void begin();
// Geste genau einmal, wenn der Finger losgelassen wird. rotation = aktuelle Displaydrehung 0..3,
// damit "rechts" und "unten" immer im angezeigten Bild gelten.
Gesture read(uint8_t rotation);
}  // namespace touch
