#pragma once
#include <FastLED.h>

// ARGB-Eingang: liest das WS2812-Signal eines Mainboard-ARGB-Headers (z. B. ASUS Aura)
// an ARGB_IN_PIN mit. So steuert Armoury Crate/Aura den Staender wie einen normalen
// ARGB-Streifen. Die Anzahl LEDs, die Aura sendet, wird automatisch erkannt und auf
// LED_COUNT gestreckt. Kommt kein Signal (PC aus), laufen die eigenen Effekte.
//
// Hardware: Header-Pin "D" ueber Spannungsteiler 1 kOhm / 2 kOhm an ARGB_IN_PIN,
// Header-Pin "G" an GND. Den 5V-Pin des Headers NICHT anschliessen.
namespace argb {

bool begin();               // RMT-Empfangskanal anlegen; false bei Fehler
void loop(bool enabled);    // fertige Bilder auswerten und Empfang neu starten
bool active();              // gerade gueltiges Signal vom Mainboard?
uint16_t detectedLeds();    // Anzahl LEDs, die das Mainboard sendet
void copyTo(CRGB *leds, uint16_t n);

}  // namespace argb
