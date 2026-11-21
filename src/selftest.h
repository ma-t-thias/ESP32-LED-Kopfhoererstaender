#pragma once
#include <FastLED.h>

// Selbsttest fuer die Fertigung: prueft nacheinander LEDs (Farben und Lauflicht, Sichtpruefung),
// Touch, BOOT-Taste, Drucksensor und Mikrofon (10 s live am Display) und Lagesensor und zeigt das Ergebnis am Display, im seriellen Monitor
// und unter /api/selftest. Start ueber die Weboberflaeche oder den USB-Befehl "selftest".
// Waehrend des Tests gehoeren LEDs, Taste und Touch dem Test; WLAN und Weboberflaeche laufen weiter.
namespace selftest {

void start();
bool running();
bool micWanted();  // Mikrofon soll laufen (Testschritt Drucksensor/Mikrofon)
// Einmal je Durchlauf von loop(): schreibt die LED-Farben (Helligkeit setzt der Test selbst).
// buttonDown = BOOT gerade gedrueckt, touched = Geste erkannt (Finger losgelassen)
void loop(CRGB *leds, uint16_t n, bool buttonDown, bool touched);
uint8_t brightness();  // Helligkeit fuer die Test-Bilder
String json();         // Stand und letztes Ergebnis fuer die Weboberflaeche

}  // namespace selftest
