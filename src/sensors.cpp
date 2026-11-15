#include "sensors.h"

#include "config.h"

#ifdef HP_SENSE_PIN
#include <ESP_I2S.h>
#include <math.h>

namespace sensors {
namespace {
// ---- Kopfhoerer (FSR) ----
uint16_t mv = 0;          // geglaetteter Messwert
bool pressed = true;      // Schwelle mit Hysterese; beim Start "liegt auf", damit nichts dunkel startet
bool present = true;      // nach Verzoegerung
uint32_t releasedAt = 0;  // seit wann unter der Schwelle
uint32_t lastRead = 0;

// ---- Mikrofon ----
I2SClass i2s;
bool micRunning = false;
uint32_t micStartAt = 0;  // die ersten 300 ms liefert das Mikrofon nur Nullen
uint32_t micRetryAt = 0;  // nach einem Fehlstart erst spaeter erneut versuchen
int32_t buf[64];
float dc = 0, acc = 0;
uint16_t count = 0;
float floorDb = -1;     // Grundrauschen, passt sich an (-1 = noch unbekannt)
uint8_t level = 0;
uint32_t lastData = 0;  // letzter Block mit Signal

void readHeadphone(uint32_t now) {
  if (now - lastRead < 50) return;
  lastRead = now;
  uint16_t v = analogReadMilliVolts(HP_SENSE_PIN);
  mv = (mv * 3 + v) / 4;  // leicht glaetten
  if (!pressed && mv > HP_ON_MV) pressed = true;
  else if (pressed && mv < HP_OFF_MV) { pressed = false; releasedAt = now; }
  if (pressed) present = true;
  else if (now - releasedAt >= HP_DELAY_MS) present = false;
}

void micStart() {
  i2s.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  micRunning = i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
  if (!micRunning) {
    Serial.println("Mikrofon: I2S startet nicht");
    micRetryAt = millis() + 5000;
  }
  micStartAt = millis();
  floorDb = -1;
  count = 0;
  acc = 0;
}

// 256 Werte (16 ms) zu einem Pegel zusammenfassen: dB ueber dem Grundrauschen, 0..255 fuer ca. 36 dB
void block() {
  float env = acc / count;
  acc = 0;
  count = 0;
  float db = 20.0f * log10f(env + 1.0f);
  if (env <= 0 || millis() - micStartAt < 300) return;  // Anlaufphase: nur Nullen
  lastData = millis();
  if (floorDb < 0 || db < floorDb) floorDb = db;  // leiser geworden: Grundrauschen sofort nachziehen
  else floorDb += 0.005f;                         // sonst langsam hochlaufen (ca. 0,3 dB/s)
  float x = (db - floorDb - 3.0f) * (255.0f / 36.0f);
  uint8_t l = x <= 0 ? 0 : x >= 255 ? 255 : uint8_t(x);
  level = l > level ? l : (level > 8 ? level - 8 : 0);  // schnell hoch, langsam runter
}

// Liest, was im DMA-Puffer liegt, ohne zu warten (auch Teilbloecke)
void readMic() {
  for (int k = 0; k < 8; k++) {
    size_t got = 0;
    i2s_channel_read(i2s.rxChan(), buf, sizeof(buf), &got, 0);
    size_t n = got / sizeof(int32_t);
    for (size_t i = 0; i < n; i++) {
      float x = float(buf[i] >> 8);  // 24 Bit Nutzdaten oben im 32-Bit-Wort
      dc += (x - dc) * 0.001f;       // Gleichanteil entfernen
      acc += fabsf(x - dc);
      if (++count >= 256) block();
    }
    if (got < sizeof(buf)) break;
  }
}
}  // namespace

void begin() {
  analogSetPinAttenuation(HP_SENSE_PIN, ADC_11db);  // Messbereich bis ca. 3,1 V
  mv = analogReadMilliVolts(HP_SENSE_PIN);
}

void loop(bool mic) {
  uint32_t now = millis();
  readHeadphone(now);
  if (mic && !micRunning && (int32_t)(now - micRetryAt) >= 0) micStart();
  else if (!mic && micRunning) {
    i2s.end();
    micRunning = false;
    level = 0;
  }
  if (micRunning) readMic();
}

bool hpAvailable() { return true; }
bool headphonePresent() { return present; }
uint16_t hpMilliVolts() { return mv; }
uint8_t micLevel() { return level; }
bool micOk() { return micRunning && millis() - lastData < 1000; }
}  // namespace sensors

#else  // altes Board ohne Sensoren

namespace sensors {
void begin() {}
void loop(bool) {}
bool hpAvailable() { return false; }
bool headphonePresent() { return true; }
uint16_t hpMilliVolts() { return 0; }
uint8_t micLevel() { return 0; }
bool micOk() { return false; }
}  // namespace sensors

#endif
