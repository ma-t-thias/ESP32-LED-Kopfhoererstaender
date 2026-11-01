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
  uint8_t displayRotation = LCD_ROTATION;  // 0..3 = 0/90/180/270 Grad im Uhrzeigersinn
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
void savePresets(const String &json);
}  // namespace settings
