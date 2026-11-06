#include "imu.h"

#include <Arduino.h>

#include "config.h"

#ifdef IMU_ADDR
#include <Wire.h>

namespace imu {
namespace {
bool present = false;
int current = -1;    // zuletzt bestaetigte Drehung
int candidate = -1;  // neue Lage, muss zweimal hintereinander gemessen werden
uint32_t lastRead = 0;

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

int fromGravity(int16_t ax, int16_t ay) {
  constexpr int MIN = 6000;  // ca. 0,37 g bei +-2 g; flach liegend bleibt die alte Lage
  if (abs(ax) >= abs(ay)) {
    if (abs(ax) < MIN) return -1;
    return ax > 0 ? IMU_ROT_XPOS : IMU_ROT_XNEG;
  }
  if (abs(ay) < MIN) return -1;
  return ay > 0 ? IMU_ROT_YPOS : IMU_ROT_YNEG;
}
}  // namespace

void begin() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x00);  // WHO_AM_I, erwartet 0x05
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(IMU_ADDR, 1) == 1 && Wire.read() == 0x05) {
    present = writeReg(0x02, 0x40)      // CTRL1: Adresse beim Lesen automatisch weiterzaehlen
              && writeReg(0x03, 0x07)   // CTRL2: Beschleunigung +-2 g, 62,5 Hz
              && writeReg(0x08, 0x01);  // CTRL7: nur Beschleunigungssensor an
  }
  Serial.println(present ? "Lagesensor gefunden" : "Lagesensor nicht gefunden");
}

void loop() {
  if (!present || millis() - lastRead < 500) return;
  lastRead = millis();
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x35);  // AX_L, danach AX_H, AY_L, AY_H
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(IMU_ADDR, 4) != 4) return;
  uint8_t b[4];
  for (auto &v : b) v = Wire.read();
  int16_t ax = b[0] | (b[1] << 8);
  int16_t ay = b[2] | (b[3] << 8);
  int r = fromGravity(ax, ay);
  if (r < 0 || r == current) {
    candidate = -1;
    return;
  }
  if (r == candidate) {  // zweimal gleich: Lage uebernehmen
    current = r;
    Serial.printf("Lage: ax=%d ay=%d -> Drehung %d\n", ax, ay, r);
  }
  candidate = r;
}

int rotation() { return current; }
}  // namespace imu

#else

namespace imu {
void begin() {}
void loop() {}
int rotation() { return -1; }
}  // namespace imu

#endif
