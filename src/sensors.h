#pragma once
#include <Arduino.h>

// Sensoren der Platine v2: Kopfhoerer-Erkennung (FSR an HP_SENSE_PIN) und I2S-Mikrofon (INMP441).
// Auf dem alten 1.3er-Board gibt es beides nicht: dann liefern die Funktionen feste Werte.
namespace sensors {
void begin();
// Ruft man in jedem loop() auf. mic = Mikrofon soll laufen (Effekt "Musik" aktiv).
void loop(bool mic);

bool hpAvailable();        // Board hat einen Sensor-Eingang
bool headphonePresent();   // Kopfhoerer liegt auf (mit Hysterese und Verzoegerung beim Abnehmen)
uint16_t hpMilliVolts();   // Messwert minus Nullpunkt (damit wird geschaltet), fuer die Weboberflaeche
uint16_t hpRawMilliVolts();  // Messwert ohne Nullpunkt, zum Nullen
uint16_t setHpZero(uint16_t mv);  // Nullpunkt setzen (Grundlast der Abdeckung ohne Kopfhoerer), gibt den begrenzten Wert zurueck

// Lautstaerke 0..255 (logarithmisch, passt sich dem Grundrauschen an); 0 ohne Mikrofon
uint8_t micLevel();
bool micOk();              // Mikrofon liefert Daten

// Spektrum fuer die Weboberflaeche: MIC_BANDS Frequenzpunkte von tief nach hoch, je 0..255.
// Gerechnet wird nur, solange jemand micBands() abfragt (danach 2 s).
constexpr uint8_t MIC_BANDS = 8;
const uint8_t *micBands();
}  // namespace sensors
