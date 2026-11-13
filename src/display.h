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
// Rueckfrage am Display, bevor ein Firmware-Update aus dem Browser angenommen wird.
uint32_t askUpdate();           // Frage "Update erlauben?" fuer 60 s; Kennung der Anfrage (0 = laeuft schon)
bool confirming();              // Frage steht gerade (BOOT-Taste/Tippen beantworten sie)
void answerUpdate(bool yes);    // Antwort, z. B. BOOT-Taste auf Boards ohne Touch
int8_t updateAnswer(uint32_t token);  // fuer diese Anfrage: 1 = erlaubt (2 min), -1 = nein/abgelaufen, 0 = offen
void clearUpdateAnswer();       // Erlaubnis verbraucht (Upload hat begonnen)
// Aktuell angezeigte Drehung 0..3, auch bei Auto
uint8_t currentRotation();
// Grosse Meldung vor einem Neustart; bleibt bis zum Neustart stehen
void message(const char *line1, const char *line2);
// Startbild mit Fortschrittsbalken waehrend eines Updates (0..100 %)
void updateProgress(uint8_t percent);
// Selbsttest: eigene Anzeige mit Titel und bis zu 5 Zeilen (getrennt durch '\n').
// status 0 = laeuft, 1 = bestanden, 2 = Fehler. testEnd() kehrt zur normalen Anzeige zurueck.
void testScreen(const char *title, const String &body, uint8_t status);
void testEnd();

}  // namespace display
