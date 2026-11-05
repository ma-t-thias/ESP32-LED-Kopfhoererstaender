#pragma once
// Touch des ESP32-C6-Touch-LCD-1.9 (CST816D). Bewusst einfach: nur "Tippen erkannt".
// Boards ohne Touch (kein TOUCH_SDA in config.h) liefern nie ein Tippen.
namespace touch {
void begin();
// true genau einmal, wenn ein Finger losgelassen wurde
bool tapped();
}  // namespace touch
