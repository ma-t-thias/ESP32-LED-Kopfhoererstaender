#pragma once
#include <stdint.h>
// Lagesensor QMI8658 des ESP32-C6-Touch-LCD-1.9. Bewusst einfach: nur die Schwerkraft
// lesen und daraus die passende Displaydrehung ableiten.
namespace imu {
void begin();  // I2C muss schon laufen (touch::begin)
void loop();   // liest 2x pro Sekunde
// 0..3 passend zur Lage, -1 ohne Sensor oder solange die Lage unklar ist
int rotation();
// Fuer den Selbsttest: Sensor gefunden, und eine Messung der Beschleunigung (1 g = 16384)
bool present();
bool readAccel(int16_t &ax, int16_t &ay, int16_t &az);
}  // namespace imu
