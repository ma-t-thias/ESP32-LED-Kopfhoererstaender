#pragma once
#include "display.h"
#include "state.h"

namespace net {

// Callback, wenn die Weboberflaeche etwas aendert (Effektwechsel usw.)
using ChangeHandler = void (*)(bool effectChanged);

void begin(LedState &state, ChangeHandler onChange);
void loop();
display::NetInfo info();
// Loescht WLAN-Daten und startet neu (Taster 5 s halten oder Weboberflaeche)
void resetWifiAndRestart();

}  // namespace net
