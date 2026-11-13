#pragma once
#include <Arduino.h>
#include <FastLED.h>

#include "config.h"

// Alles, was die Weboberflaeche einstellen kann und was ueber einen Neustart
// hinweg gespeichert wird.
struct LedState {
  bool on = true;
  uint8_t brightness = 128;  // 0..255
  uint8_t effect = 1;        // Index in die Effektliste (effects.h)
  uint8_t palette = 0;       // Index in die Palettenliste (effects.h)
  uint8_t speed = 128;       // 0..255
  uint8_t intensity = 128;   // 0..255, Bedeutung je Effekt (Dichte, Schweiflaenge ...)
  CRGB color1 = CRGB(255, 120, 0);
  CRGB color2 = CRGB(0, 80, 255);
  uint16_t maxMilliamps = 0;  // 0 = Startwert aus config.h verwenden
  bool syncEnabled = true;    // Farben vom PC (SignalRGB, OpenRGB ...) annehmen
  bool argbEnabled = false;   // Signal vom Mainboard-ARGB-Header (Aura) uebernehmen
  uint8_t displayRotation = ROT_DEFAULT;  // 0..3 = 0/90/180/270 Grad im Uhrzeigersinn, 4 = Auto
  bool matterEnabled = false;  // Apple Home & Co. ueber Matter; wirkt nach Neustart
  bool wledOpen = false;       // WLED-Schnittstelle (Home Assistant, WLED-App) ohne Passwort steuerbar
  // Nachtmodus: in der Zeit von..bis (Minuten ab Mitternacht) hoechstens nightBri hell, 0 = aus
  bool nightEnabled = false;
  uint16_t nightFrom = 22 * 60;
  uint16_t nightTo = 7 * 60;
  uint8_t nightBri = 13;  // ca. 5 %
};

namespace settings {
void begin(LedState &state);   // laedt gespeicherte Werte
void markDirty();              // Aenderung merken; gespeichert wird verzoegert
void loop(const LedState &state);  // speichert ~3 s nach der letzten Aenderung

bool loadWifi(String &ssid, String &pass);
void saveWifi(const String &ssid, const String &pass);
void clearWifi();

// Presets der Weboberflaeche als JSON-Text (max. ca. 3 KB)
String loadPresets();
// Matter ein/aus sofort speichern (danach folgt ein Neustart)
void saveMatter(bool enabled);
void savePresets(const String &json);

// Geraetepasswort fuer die Weboberflaeche: beim ersten Start zufaellig erzeugt (8 Zeichen),
// steht am Display (Einrichten) und laesst sich in der Weboberflaeche aendern.
String devicePassword();
void setDevicePassword(const String &pw);
// Werkseinstellungen: loescht alle gespeicherten Daten (WLAN, Einstellungen, Presets,
// Passwort, Matter-Kopplung) und startet neu. Kehrt nicht zurueck.
void factoryReset();
// Seriennummer aus der Fertigung. Liegt im Bereich "fctry" und bleibt auch bei Werkseinstellungen
// erhalten. Leer, solange keine vergeben ist. Setzen nur ueber USB (Befehl "sn ..."), einmalig;
// force = ueberschreiben (Befehl "sn! ..."). 1-24 Zeichen A-Z a-z 0-9 - _ . (ohne "Ada")
String serialNumber();
bool setSerialNumber(const String &sn, bool force);
}  // namespace settings
