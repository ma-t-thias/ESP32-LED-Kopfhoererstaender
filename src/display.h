#pragma once
#include <FastLED.h>

#include "state.h"

namespace display {

struct NetInfo {
  bool connected = false;
  bool setupAp = false;  // Einrichtungs-WLAN aktiv
  String ip;             // im Einrichtungsmodus die AP-Adresse
  String ssid;
  String detail;   // z. B. Grund des letzten WLAN-Abbruchs
  String detail2;  // Grund des letzten Neustarts
};

bool begin();
// Zeichnet geaenderte Texte neu und aktualisiert die LED-Vorschau (max. 10x/s).
void loop(const LedState &s, const NetInfo &net, const CRGB *leds, uint16_t n);
// Aktuell angezeigte Drehung 0..3, auch bei Auto
uint8_t currentRotation();
// Grosse Meldung vor einem Neustart; bleibt bis zum Neustart stehen
void message(const char *line1, const char *line2);
// Startbild mit Fortschrittsbalken waehrend eines Updates (0..100 %)
void updateProgress(uint8_t percent);

}  // namespace display
