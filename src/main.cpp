// LED-Kopfhoererstaender: WLAN-Steuerung fuer einen WS2812B-Streifen mit Farben,
// Farbverlaeufen und bewegten Effekten.
// Board: Waveshare ESP32-C6-LCD-1.3. Schaltung: docs/hardware/
#include <Arduino.h>
#include <FastLED.h>

#include "config.h"
#include "fw_version.h"
#include "display.h"
#include "touch.h"
#include "imu.h"
#include "effects.h"
#include "net.h"
#include "argb_in.h"
#include "matter_dev.h"
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

// Naechster Effekt (und einschalten): kurzer Tastendruck oder Tippen aufs Display
void nextEffect(int step = 1) {
  state.on = true;
  state.effect = (state.effect + effects::COUNT + step) % effects::COUNT;
  effects::reset();
  settings::markDirty();
}

// Tippen unten: bei Farbeffekten die naechste Farbe, sonst die naechste Palette
void nextColorOrPalette() {
  static const CRGB COLORS[] = {CRGB(255, 0, 0),   CRGB(255, 120, 0), CRGB(255, 220, 0), CRGB(0, 255, 0),
                                CRGB(0, 255, 200), CRGB(0, 80, 255),  CRGB(160, 0, 255), CRGB(255, 0, 120),
                                CRGB(255, 255, 255)};
  constexpr uint8_t N = sizeof(COLORS) / sizeof(COLORS[0]);
  state.on = true;
  if (effects::usesColor(state.effect)) {
    uint8_t i = 0;
    while (i < N && COLORS[i] != state.color1) i++;
    if (i == N) i = N - 1;  // eigene Farbe aus der Weboberflaeche: mit Rot beginnen
    state.color1 = COLORS[(i + 1) % N];
    state.color2 = COLORS[(i + 1 + N / 2) % N];  // zweite Farbe gegenueber, fuer Verlauf/Scanner
  } else {
    state.palette = (state.palette + 1) % effects::PALETTE_COUNT;
  }
  settings::markDirty();
}

// Ist der Staender aus, schaltet die erste Beruehrung nur ein (Effekt und Farbe bleiben).
bool turnOnIfOff() {
  if (state.on) return false;
  state.on = true;
  settings::markDirty();
  return true;
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
    if (longHandled) return;
    if (display::confirming()) display::answerUpdate(true);  // Firmware-Update am Geraet erlauben
    else if (!turnOnIfOff()) nextEffect();                    // kurzer Druck
  }
}
}  // namespace

void setup() {
  Serial.setRxBufferSize(2048);  // Platz fuer ganze Adalight-Rahmen vom PC (USB)
  Serial.begin(115200);
  delay(200);
  Serial.println("\nLED-Kopfhoererstaender startet, Firmware " FW_FULL);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  settings::begin(state);

  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, LED_COUNT).setCorrection(TypicalLEDStrip);
  applyPowerLimit();
  FastLED.setBrightness(0);
  FastLED.clear(true);

  if (!display::begin()) Serial.println("Display nicht gefunden - laeuft ohne Anzeige weiter");
  touch::begin();
  imu::begin();  // nach touch::begin, das den I2C-Bus startet

  net::setLeds(leds, LED_COUNT);
  net::begin(state, onWebChange);
}

void loop() {
  net::loop();
  realtime::loop(state.syncEnabled);
  argb::loop(state.argbEnabled);
  handleButton();
  touch::Gesture g = touch::read(display::currentRotation());
  if (g != touch::NONE && !display::confirming() && turnOnIfOff()) {
    g = touch::NONE;
  } else if (int r = display::handleTouch(g, touch::point(), state)) {
    if (r == 2) settings::markDirty();  // Regler auf der Einstellseite
    g = touch::NONE;
  }
  switch (g) {
    case touch::TAP_TOP:
    case touch::SWIPE_RIGHT: nextEffect(); break;
    case touch::TAP_BOTTOM: nextColorOrPalette(); break;
    case touch::SWIPE_LEFT: nextEffect(-1); break;
    default: break;
  }
  imu::loop();
  settings::loop(state);

  if (state.matterEnabled && matterdev::available() && !matterdev::started() && net::info().connected) matterdev::begin(state);
  bool fxCh = false;
  if (matterdev::loop(state, fxCh)) onWebChange(fxCh);

  uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    // Reihenfolge: Mainboard (Aura) vor PC-Sync (WLAN oder USB) vor eigenen Effekten.
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
