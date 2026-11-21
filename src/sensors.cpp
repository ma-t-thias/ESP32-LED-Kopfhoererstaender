#include "sensors.h"

#include "config.h"

#ifdef HP_SENSE_PIN
#include <ESP_I2S.h>
#include <math.h>
#include <string.h>

namespace sensors {
namespace {
// ---- Kopfhoerer (FSR) ----
uint16_t mv = 0;          // geglaetteter Messwert
uint16_t zero = 0;        // Nullpunkt (Grundlast der Abdeckung), wird abgezogen
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

// Abtastrate 40 kHz: Der Filter im INMP441 laesst bis 0,42 x Abtastrate durch, also bis ca. 17 kHz.
// So reicht das Spektrum bis 15 kHz. Ein Block hat 640 Werte = 16 ms, Raster 62,5 Hz.
constexpr uint32_t MIC_RATE = 40000;
constexpr uint16_t BLOCK = 640;
// Spektrum: je Band ein Goertzel-Filter ueber die Werte eines Blocks, also ein schmaler
// Frequenzpunkt (+-31 Hz) bei f Hz, keine Oktavbaender. Fuer eine Kontrollanzeige reicht das.
// Gerechnet wird nur, solange Display oder Weboberflaeche es abfragen, und nur in jedem zweiten
// Block (ca. 30 Bilder/s), weil Gleitkomma ohne FPU teuer ist.
const float BAND_HZ[MIC_BANDS] = {62.5f, 125, 250, 500, 1000, 2500, 6250, 15000};
bool specTurn = false;  // dieser Block rechnet das Spektrum
float coef[MIC_BANDS];
float g1[MIC_BANDS], g2[MIC_BANDS];
float ps[MIC_BANDS];         // geglaettete Leistung je Band
float bandFloor[MIC_BANDS];  // Grundrauschen je Band in dB
uint8_t bands[MIC_BANDS];
uint32_t bandsWantedAt = 0;  // letzte Abfrage (Display oder Weboberflaeche)

bool bandsWanted() { return bandsWantedAt && millis() - bandsWantedAt < 2000; }

void readHeadphone(uint32_t now) {
  if (now - lastRead < 50) return;
  lastRead = now;
  uint16_t v = analogReadMilliVolts(HP_SENSE_PIN);
  mv = (mv * 3 + v) / 4;  // leicht glaetten
  uint16_t load = mv > zero ? mv - zero : 0;
  if (!pressed && load > HP_ON_MV) pressed = true;
  else if (pressed && load < HP_OFF_MV) { pressed = false; releasedAt = now; }
  if (pressed) present = true;
  else if (now - releasedAt >= HP_DELAY_MS) present = false;
}

void micStart() {
  i2s.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  micRunning = i2s.begin(I2S_MODE_STD, MIC_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
  if (!micRunning) {
    Serial.println("Mikrofon: I2S startet nicht");
    micRetryAt = millis() + 5000;
  }
  micStartAt = millis();
  floorDb = -1;
  for (uint8_t b = 0; b < MIC_BANDS; b++) {
    coef[b] = 2.0f * cosf(2.0f * float(M_PI) * roundf(BAND_HZ[b] * BLOCK / MIC_RATE) / BLOCK);
    g1[b] = g2[b] = ps[b] = 0;
    bandFloor[b] = -1;
    bands[b] = 0;
  }
  count = 0;
  acc = 0;
}

// Spektrum eines Blocks: Leistung je Band (ueber Bloecke geglaettet) in dB ueber dem eigenen
// Grundrauschen, 0..255 fuer 40 dB. Das Grundrauschen folgt nach unten nur sanft, weil einzelne
// Goertzel-Werte bei Rauschen stark schwanken.
void spectrum() {
  for (uint8_t b = 0; b < MIC_BANDS; b++) {
    float p = g1[b] * g1[b] + g2[b] * g2[b] - coef[b] * g1[b] * g2[b];
    g1[b] = g2[b] = 0;
    ps[b] += (p - ps[b]) * 0.3f;
    float db = 10.0f * log10f(ps[b] + 1.0f);
    if (bandFloor[b] < 0) bandFloor[b] = db;
    else if (db < bandFloor[b]) bandFloor[b] += (db - bandFloor[b]) * 0.05f;
    else bandFloor[b] += 0.005f;
    float x = (db - bandFloor[b] - 6.0f) * (255.0f / 40.0f);
    uint8_t l = x <= 0 ? 0 : x >= 255 ? 255 : uint8_t(x);
    bands[b] = l > bands[b] ? l : (bands[b] > 12 ? bands[b] - 12 : 0);  // schnell hoch, langsam runter
  }
}

// Einen Block (16 ms) zu einem Pegel zusammenfassen: dB ueber dem Grundrauschen, 0..255 fuer ca. 36 dB
void block() {
  float env = acc / count;
  acc = 0;
  count = 0;
  float db = 20.0f * log10f(env + 1.0f);
  if (env <= 0 || millis() - micStartAt < 300) {  // Anlaufphase: nur Nullen
    for (uint8_t b = 0; b < MIC_BANDS; b++) g1[b] = g2[b] = 0;
    specTurn = true;
    return;
  }
  if (!bandsWanted()) memset(bands, 0, sizeof(bands));
  else if (specTurn) spectrum();
  specTurn = !specTurn;
  lastData = millis();
  if (floorDb < 0 || db < floorDb) floorDb = db;  // leiser geworden: Grundrauschen sofort nachziehen
  else floorDb += 0.005f;                         // sonst langsam hochlaufen (ca. 0,3 dB/s)
  float x = (db - floorDb - 3.0f) * (255.0f / 36.0f);
  uint8_t l = x <= 0 ? 0 : x >= 255 ? 255 : uint8_t(x);
  level = l > level ? l : (level > 8 ? level - 8 : 0);  // schnell hoch, langsam runter
}

// Liest, was im DMA-Puffer liegt, ohne zu warten (auch Teilbloecke)
void readMic() {
  bool spec = bandsWanted() && specTurn;
  for (int k = 0; k < 24; k++) {  // bis 1536 Werte (38 ms, so viel fasst der DMA-Puffer) je Aufruf
    size_t got = 0;
    i2s_channel_read(i2s.rxChan(), buf, sizeof(buf), &got, 0);
    size_t n = got / sizeof(int32_t);
    for (size_t i = 0; i < n; i++) {
      float x = float(buf[i] >> 8);  // 24 Bit Nutzdaten oben im 32-Bit-Wort
      dc += (x - dc) * 0.001f;       // Gleichanteil entfernen
      float y = x - dc;
      acc += fabsf(y);
      for (uint8_t b = 0; spec && b < MIC_BANDS; b++) {  // Goertzel-Schritt je Band
        float g0 = y + coef[b] * g1[b] - g2[b];
        g2[b] = g1[b];
        g1[b] = g0;
      }
      if (++count >= BLOCK) {
        block();
        spec = bandsWanted() && specTurn;
      }
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
    memset(bands, 0, sizeof(bands));
  }
  if (micRunning) readMic();
}

bool hpAvailable() { return true; }
bool headphonePresent() { return present; }
uint16_t hpMilliVolts() { return mv > zero ? mv - zero : 0; }
uint16_t hpRawMilliVolts() { return mv; }
uint16_t setHpZero(uint16_t z) { return zero = z > HP_ZERO_MAX_MV ? HP_ZERO_MAX_MV : z; }
uint8_t micLevel() { return level; }
bool micOk() { return micRunning && millis() - lastData < 1000; }
const uint8_t *micBands() {
  bandsWantedAt = millis() | 1;  // Abfrage haelt die Berechnung 2 s am Laufen
  return bands;
}
}  // namespace sensors

#else  // altes Board ohne Sensoren

namespace sensors {
void begin() {}
void loop(bool) {}
bool hpAvailable() { return false; }
bool headphonePresent() { return true; }
uint16_t hpMilliVolts() { return 0; }
uint16_t hpRawMilliVolts() { return 0; }
uint16_t setHpZero(uint16_t) { return 0; }
uint8_t micLevel() { return 0; }
bool micOk() { return false; }
const uint8_t *micBands() {
  static const uint8_t none[MIC_BANDS] = {};
  return none;
}
}  // namespace sensors

#endif
