#include "imu.h"

#include <Arduino.h>

#include "config.h"

#ifdef IMU_ADDR
#include <Wire.h>

namespace imu {
namespace {
bool found = false;
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
    found = writeReg(0x02, 0x40)      // CTRL1: Adresse beim Lesen automatisch weiterzaehlen
              && writeReg(0x03, 0x07)   // CTRL2: Beschleunigung +-2 g, 62,5 Hz
              && writeReg(0x08, 0x01);  // CTRL7: nur Beschleunigungssensor an
  }
  Serial.println(found ? "Lagesensor gefunden" : "Lagesensor nicht gefunden");
}

bool readAccel(int16_t &ax, int16_t &ay, int16_t &az) {
  if (!found) return false;
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(0x35);  // AX_L, AX_H, AY_L, AY_H, AZ_L, AZ_H
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(IMU_ADDR, 6) != 6) return false;
  uint8_t b[6];
  for (auto &v : b) v = Wire.read();
  ax = b[0] | (b[1] << 8);
  ay = b[2] | (b[3] << 8);
  az = b[4] | (b[5] << 8);
  return true;
}

bool present() { return found; }

void loop() {
  if (!found || millis() - lastRead < 500) return;
  lastRead = millis();
  int16_t ax, ay, az;
  if (!readAccel(ax, ay, az)) return;
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
bool present() { return false; }
bool readAccel(int16_t &, int16_t &, int16_t &) { return false; }
}  // namespace imu

#endif
