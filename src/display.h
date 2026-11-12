#pragma once
#include <FastLED.h>

#include "state.h"
#include "touch.h"

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
// Touch fuer die Anzeige selbst: Zahnrad oben rechts oeffnet die Einstellseite mit Reglern.
// 0 = nicht fuer die Anzeige (Geste normal auswerten), 1 = verbraucht, 2 = verbraucht und
// Helligkeit/Tempo/Staerke in s geaendert.
int handleTouch(uint8_t gesture, const touch::Point &p, LedState &s);
// Aktuell angezeigte Drehung 0..3, auch bei Auto
uint8_t currentRotation();
// Grosse Meldung vor einem Neustart; bleibt bis zum Neustart stehen
void message(const char *line1, const char *line2);
// Startbild mit Fortschrittsbalken waehrend eines Updates (0..100 %)
void updateProgress(uint8_t percent);

}  // namespace display
