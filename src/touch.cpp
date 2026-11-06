#include "touch.h"

#include <Arduino.h>

#include "config.h"

#ifdef TOUCH_SDA
#include <Wire.h>

namespace touch {
namespace {
bool present = false;
bool wasDown = false;
uint32_t lastPoll = 0;

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

bool tapped() {
  if (!present || millis() - lastPoll < 30) return false;
  lastPoll = millis();
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x02);  // Register 2: Anzahl der Finger
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(TOUCH_ADDR, 1) != 1) return false;
  bool down = Wire.read() > 0;
  bool tap = wasDown && !down;
  wasDown = down;
  return tap;
}
}  // namespace touch

#else

namespace touch {
void begin() {}
bool tapped() { return false; }
}  // namespace touch

#endif
