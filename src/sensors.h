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
uint16_t hpMilliVolts();   // aktueller Messwert, fuer die Weboberflaeche

// Lautstaerke 0..255 (logarithmisch, passt sich dem Grundrauschen an); 0 ohne Mikrofon
uint8_t micLevel();
bool micOk();              // Mikrofon liefert Daten
}  // namespace sensors
