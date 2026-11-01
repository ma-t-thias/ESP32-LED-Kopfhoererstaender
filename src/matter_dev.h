#pragma once
#include <Arduino.h>

#include "state.h"

// Apple Home, Google Home und Alexa ueber Matter (WLAN). Prototyp mit den
// Test-Zugangsdaten des Arduino-Cores: Hersteller-ID 0xFFF1, gleicher
// Koppel-Code auf jedem Geraet. Fuer ein verkauftes Produkt braucht es eigene,
// zertifizierte Zugangsdaten (siehe README).
namespace matterdev {

// Startet Matter, sobald das WLAN verbunden ist. Nur aufrufen, wenn aktiviert.
void begin(const LedState &s);
bool started();
bool paired();
// Uebernimmt Befehle aus Apple Home in den Zustand und meldet lokale Aenderungen
// (Weboberflaeche, Taster) zurueck. true = Zustand wurde von Matter geaendert.
bool loop(LedState &s, bool &effectChanged);
// Kopplung loeschen; danach kann neu gekoppelt werden
void unpair();

String manualCode();   // 11 Ziffern, in der Home-App unter "Code eingeben"
String qrPayload();    // Inhalt des QR-Codes ("MT:...")

}  // namespace matterdev
