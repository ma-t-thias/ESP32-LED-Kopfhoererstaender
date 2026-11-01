#pragma once
#include <FastLED.h>

#include "state.h"

namespace display {

struct NetInfo {
  bool connected = false;
  bool setupAp = false;  // Einrichtungs-WLAN aktiv
  String ip;             // im Einrichtungsmodus die AP-Adresse
  String ssid;
};

bool begin();
// Zeichnet geaenderte Texte neu und aktualisiert die LED-Vorschau (max. 10x/s).
void loop(const LedState &s, const NetInfo &net, const CRGB *leds, uint16_t n);
// Grosse Meldung, z. B. vor einem Neustart
void message(const char *line1, const char *line2);

}  // namespace display
