#pragma once
#include <FastLED.h>

#include "state.h"

namespace effects {

// Namen fuer Weboberflaeche und Display (Reihenfolge = Index im LedState)
extern const char *const NAMES[];
extern const uint8_t COUNT;
extern const char *const PALETTE_NAMES[];
extern const uint8_t PALETTE_COUNT;

// true, wenn der Effekt mit Farbe 1/2 statt mit einer Palette arbeitet (Einfarbig, Atmen, Komet ...)
bool usesColor(uint8_t effect);

// Berechnet das naechste Bild in leds[]. Wird mit ca. 60 Bildern/s aufgerufen.
void render(CRGB *leds, uint16_t n, const LedState &s, uint32_t nowMs);

// Muss nach einem Effektwechsel aufgerufen werden (setzt interne Zustaende zurueck).
void reset();

}  // namespace effects
