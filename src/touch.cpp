#include "touch.h"

#include <Arduino.h>

#include "config.h"

#ifdef TOUCH_SDA
#include <Wire.h>

namespace touch {
namespace {
bool present = false;
bool wasDown = false;
int x = 0, y = 0, x0 = 0, y0 = 0;  // aktuelle Position und Startpunkt des Fingers
uint32_t lastPoll = 0;
Point pt;  // letzte Position in Bildkoordinaten

// Panel-Koordinaten (hochkant LCD_W x LCD_H) in Bildkoordinaten der Drehung umrechnen
void toScreen(uint8_t rotation, int px, int py, int &sx, int &sy) {
  switch (rotation & 3) {
    case 0: sx = px;              sy = py;              break;
    case 1: sx = py;              sy = LCD_W - 1 - px;  break;
    case 2: sx = LCD_W - 1 - px;  sy = LCD_H - 1 - py;  break;
    default: sx = LCD_H - 1 - py; sy = px;              break;
  }
}

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
}  // namespace

void begin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  // Der Chip antwortet erst kurz nach dem Einschalten; wie im Waveshare-Beispiel mehrfach versuchen.
  for (int i = 0; i < 10 && !present; i++) {
    present = writeReg(0x00, 0x00);  // Normalbetrieb
    if (!present) delay(100);
  }
  if (present) writeReg(0xFE, 0x01);  // nicht automatisch einschlafen, sonst antwortet er nicht mehr
  Serial.println(present ? "Touch gefunden" : "Touch nicht gefunden");
}

Gesture read(uint8_t rotation) {
  if (!present || millis() - lastPoll < 30) return NONE;
  lastPoll = millis();
  uint8_t b[7];  // Register 0..6: Modus, Geste, Fingerzahl, X hoch, X tief, Y hoch, Y tief
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(TOUCH_ADDR, 7) != 7) return NONE;
  for (auto &v : b) v = Wire.read();
  bool down = b[2] > 0;
  if (down) {
    x = ((b[3] & 0x0F) << 8) | b[4];
    y = ((b[5] & 0x0F) << 8) | b[6];
    if (!wasDown) {
      x0 = x;
      y0 = y;
    }
  }
  bool released = wasDown && !down;
  wasDown = down;
  pt.down = down;
  toScreen(rotation, x, y, pt.x, pt.y);
  if (!released) return NONE;

  // Bewegung in Panel-Koordinaten (hochkant) in Bildkoordinaten umrechnen
  int dx = x - x0, dy = y - y0, h, v;
  switch (rotation & 3) {
    case 0: h = dx;  v = dy;  break;
    case 1: h = dy;  v = -dx; break;
    case 2: h = -dx; v = -dy; break;
    default: h = -dy; v = dx; break;
  }
  if (abs(h) > 40 && abs(h) > abs(v)) return h > 0 ? SWIPE_RIGHT : SWIPE_LEFT;
  if (abs(h) < 20 && abs(v) < 20) {
    // untere Haelfte des angezeigten Bilds (Panel hochkant LCD_W x LCD_H)
    bool bottom;
    switch (rotation & 3) {
      case 0: bottom = y > LCD_H / 2; break;
      case 1: bottom = x < LCD_W / 2; break;
      case 2: bottom = y < LCD_H / 2; break;
      default: bottom = x > LCD_W / 2; break;
    }
    return bottom ? TAP_BOTTOM : TAP_TOP;
  }
  return NONE;  // senkrecht gewischt: ignorieren
}

Point point() { return pt; }
}  // namespace touch

#else

namespace touch {
void begin() {}
Gesture read(uint8_t) { return NONE; }
Point point() { return Point(); }
}  // namespace touch

#endif
