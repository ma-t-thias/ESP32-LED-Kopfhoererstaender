#pragma once
#include "display.h"
#include "state.h"

namespace net {

// Callback, wenn die Weboberflaeche etwas aendert (Effektwechsel usw.)
using ChangeHandler = void (*)(bool effectChanged);

void begin(LedState &state, ChangeHandler onChange);
// LED-Puffer fuer die Live-Vorschau in der Weboberflaeche (/api/live)
void setLeds(const CRGB *leds, uint16_t n);
void loop();
display::NetInfo info();
// Loescht WLAN-Daten und startet neu (Taster 5 s halten oder Weboberflaeche)
void resetWifiAndRestart();
// Nachtmodus gerade aktiv (eingeschaltet, Uhrzeit bekannt, jetzt im Zeitfenster)
bool nightActive(const LedState &s);

}  // namespace net
