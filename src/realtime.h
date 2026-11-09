#pragma once
#include <FastLED.h>

// Echtzeit-Empfang vom PC (SignalRGB, OpenRGB, Hyperion, xLights ...).
// Unterstuetzt werden drei verbreitete Netzwerkprotokolle
//   - DDP          UDP-Port 4048
//   - E1.31 (sACN) UDP-Port 5568, Unicast und Multicast, Universum REALTIME_E131_UNIVERSE
//   - WLED-UDP     UDP-Port 21324 (WARLS, DRGB, DNRGB)
// und ueber USB das Adalight-Protokoll (z. B. OpenRGB "Serial LED Strip").
// Solange Pakete kommen, zeigt der Streifen die empfangenen Farben. Kommt laenger
// als die Zeitgrenze nichts mehr, laufen wieder die eigenen Effekte.
namespace realtime {

void begin();               // nach dem WLAN-Verbindungsaufbau aufrufen
void loop(bool enabled);    // Pakete lesen; bei enabled == false werden sie verworfen
bool active();              // gerade Daten vom PC?
const char *source();       // Name des Protokolls, das zuletzt Daten geliefert hat
void copyTo(CRGB *leds, uint16_t n);

}  // namespace realtime
