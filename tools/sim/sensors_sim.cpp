// Sensoren in der PC-Simulation: kein Kopfhoerer-Sensor, das Mikrofon liefert einen
// kuenstlichen Takt (ca. 120 BPM), damit der Effekt "Musik" etwas zeigt.
#include <chrono>

#include "sensors.h"

namespace sensors {
void begin() {}
void loop(bool) {}
bool hpAvailable() { return false; }
bool headphonePresent() { return true; }
uint16_t hpMilliVolts() { return 0; }
uint16_t hpRawMilliVolts() { return 0; }
uint16_t setHpZero(uint16_t) { return 0; }
bool micOk() { return true; }
const uint8_t *micBands() {
  static uint8_t b[MIC_BANDS] = {};
  return b;
}

uint8_t micLevel() {
  using namespace std::chrono;
  uint32_t ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
  uint32_t beat = ms % 500;                       // Schlag alle 500 ms
  int v = 230 - int(beat) * 2 / 3;                // schnell hoch, langsam abklingen
  if (v < 40) v = 40 + int((ms / 37) % 30);       // etwas "Rauschen" zwischen den Schlaegen
  return uint8_t(v);
}
}  // namespace sensors
