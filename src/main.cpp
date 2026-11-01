// LED-Kopfhoererstaender: WLAN-Steuerung fuer einen WS2812B-Streifen mit Farben,
// Farbverlaeufen und bewegten Effekten.
// Board: Waveshare ESP32-C6-LCD-1.3. Schaltung: docs/hardware/
#include <Arduino.h>
#include <FastLED.h>

#include "config.h"
#include "display.h"
#include "effects.h"
#include "net.h"
#include "argb_in.h"
#include "realtime.h"
#include "state.h"

namespace {
CRGB leds[LED_COUNT];
LedState state;

constexpr uint32_t FRAME_MS = 16;       // ca. 60 Bilder pro Sekunde
constexpr uint8_t FADE_STEP = 8;        // weiches Ein-/Ausschalten
uint8_t outputLevel = 0;                // tatsaechliche Helligkeit, laeuft auf Zielwert zu
uint32_t lastFrame = 0;

// BOOT-Taster
constexpr uint32_t DEBOUNCE_MS = 40;
constexpr uint32_t LONG_PRESS_MS = 5000;
bool btnDown = false;
uint32_t btnSince = 0;
bool longHandled = false;

void applyPowerLimit() { FastLED.setMaxPowerInVoltsAndMilliamps(5, state.maxMilliamps); }

void onWebChange(bool effectChanged) {
  if (effectChanged) effects::reset();
  applyPowerLimit();
  settings::markDirty();
}

void handleButton() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  uint32_t now = millis();
  if (pressed && !btnDown && now - btnSince > DEBOUNCE_MS) {
    btnDown = true;
    btnSince = now;
    longHandled = false;
  } else if (pressed && btnDown && !longHandled && now - btnSince > LONG_PRESS_MS) {
    longHandled = true;
    net::resetWifiAndRestart();
  } else if (!pressed && btnDown && now - btnSince > DEBOUNCE_MS) {
    btnDown = false;
    btnSince = now;
    if (!longHandled) {  // kurzer Druck: naechster Effekt (und einschalten)
      state.on = true;
      state.effect = (state.effect + 1) % effects::COUNT;
      effects::reset();
      settings::markDirty();
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nLED-Kopfhoererstaender startet");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  settings::begin(state);

  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, LED_COUNT).setCorrection(TypicalLEDStrip);
  applyPowerLimit();
  FastLED.setBrightness(0);
  FastLED.clear(true);

  if (!display::begin()) Serial.println("Display nicht gefunden - laeuft ohne Anzeige weiter");

  argb::begin();
  net::begin(state, onWebChange);
}

void loop() {
  net::loop();
  realtime::loop(state.syncEnabled);
  argb::loop(state.argbEnabled);
  handleButton();
  settings::loop(state);

  uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    // Reihenfolge: Mainboard (Aura) vor PC-Sync per WLAN vor eigenen Effekten.
    if (state.argbEnabled && argb::active()) argb::copyTo(leds, LED_COUNT);
    else if (state.syncEnabled && realtime::active()) realtime::copyTo(leds, LED_COUNT);
    else effects::render(leds, LED_COUNT, state, now);

    uint8_t target = state.on ? state.brightness : 0;
    if (outputLevel < target) outputLevel = min<int>(target, outputLevel + FADE_STEP);
    else if (outputLevel > target) outputLevel = max<int>(target, outputLevel - FADE_STEP);
    FastLED.setBrightness(outputLevel);
    FastLED.show();
  }

  static uint32_t lastDisplay = 0;
  if (now - lastDisplay >= 50) {
    lastDisplay = now;
    display::loop(state, net::info(), leds, LED_COUNT);
  }
}
