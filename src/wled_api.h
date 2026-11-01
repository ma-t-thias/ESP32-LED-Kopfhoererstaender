#pragma once
#include <Arduino.h>

#include "state.h"

// WLED-kompatible JSON-Schnittstelle, damit Home Assistant (WLED-Integration),
// die WLED-App und Sync-Programme den Staender wie ein WLED-Geraet steuern.
// Nachgebildet ist nur, was diese Programme brauchen: ein Segment ueber den
// ganzen Streifen, Effekte und Paletten nach eigenen Namen, Presets.
namespace wledapi {

String info(const LedState &s);
String state(const LedState &s);
String effectsJson();
String palettesJson();
String fxdataJson();
String presetsJson();  // Inhalt von /presets.json

// Wertet einen POST auf /json/state aus. Gibt false zurueck, wenn der Text kein
// gueltiges JSON ist. changed/effectChanged sagen, ob gespeichert werden muss.
bool apply(LedState &s, const String &body, bool &changed, bool &effectChanged);

// Presets wurden gespeichert: Zeitstempel fuer Home Assistant erhoehen
void presetsChanged();

}  // namespace wledapi
