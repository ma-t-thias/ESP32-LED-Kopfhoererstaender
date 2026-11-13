// LED-Kopfhoererstaender: WLAN-Steuerung fuer einen WS2812B-Streifen mit Farben,
// Farbverlaeufen und bewegten Effekten.
// Board: Waveshare ESP32-C6-LCD-1.3. Schaltung: docs/hardware/
#include <Arduino.h>
#include <FastLED.h>
#include <esp_mac.h>

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
#include "selftest.h"
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
constexpr uint32_t LONG_PRESS_MS = 5000;      // loslassen: WLAN-Daten loeschen
constexpr uint32_t FACTORY_PRESS_MS = 10000;  // weiter halten: Werkseinstellungen
bool btnDown = false;
uint32_t btnSince = 0;
bool longHandled = false;  // 5 s erreicht, Hinweis steht am Display

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
    display::message("Loslassen:", "WLAN-Reset");  // weiter halten bis 10 s = Werksreset
  } else if (pressed && btnDown && longHandled && now - btnSince > FACTORY_PRESS_MS) {
    display::message("Werksreset", "Neustart...");
    delay(800);
    settings::factoryReset();
  } else if (!pressed && btnDown && now - btnSince > DEBOUNCE_MS) {
    btnDown = false;
    btnSince = now;
    if (longHandled) return net::resetWifiAndRestart();
    if (display::confirming()) display::answerUpdate(true);  // Firmware-Update am Geraet erlauben
    else if (!turnOnIfOff()) nextEffect();                    // kurzer Druck
  }
}
// Befehle ueber USB (serieller Monitor, 115200 Baud), z. B. fuer die Fertigung
void onSerialCommand(const String &raw) {
  String cmd = raw;
  cmd.trim();
  if (cmd == "selftest" || cmd == "selbsttest") {
    if (selftest::running()) Serial.println("Der Selbsttest laeuft schon.");
    else selftest::start();
  } else if (cmd == "sn") {
    String sn = settings::serialNumber();
    Serial.printf("Seriennummer: %s\n", sn.length() ? sn.c_str() : "(keine)");
  } else if (cmd.startsWith("sn ") || cmd.startsWith("sn! ")) {
    bool force = cmd[2] == '!';
    String v = cmd.substring(force ? 4 : 3);
    v.trim();
    if (settings::setSerialNumber(v, force)) Serial.printf("Seriennummer gespeichert: %s\n", v.c_str());
    else if (settings::serialNumber().length() && !force)
      Serial.println("Seriennummer ist schon vergeben. Ueberschreiben mit: sn! <neue Nummer>");
    else Serial.println("Ungueltig: 1-20 Zeichen A-Z a-z 0-9 - _ . (ohne \"Ada\")");
  } else if (cmd == "info") {
    uint8_t m[6];
    esp_read_mac(m, ESP_MAC_WIFI_STA);  // fest im Chip, auch ohne eingeschaltetes WLAN
    String sn = settings::serialNumber();
    Serial.printf("Firmware %s, Board %s, Seriennummer %s, MAC %02X:%02X:%02X:%02X:%02X:%02X\n", FW_FULL, BOARD_TAG,
                  sn.length() ? sn.c_str() : "(keine)", m[0], m[1], m[2], m[3], m[4], m[5]);
  } else {
    Serial.println("Befehle: selftest | sn | sn <Nummer> | sn! <Nummer> | info");
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
  Serial.printf("Geraetepasswort fuer die Weboberflaeche: %s\n", settings::devicePassword().c_str());

  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, LED_COUNT).setCorrection(TypicalLEDStrip);
  applyPowerLimit();
  FastLED.setBrightness(0);
  FastLED.clear(true);

  if (!display::begin()) Serial.println("Display nicht gefunden - laeuft ohne Anzeige weiter");
  touch::begin();
  imu::begin();  // nach touch::begin, das den I2C-Bus startet

  net::setLeds(leds, LED_COUNT);
  net::begin(state, onWebChange);
  realtime::onSerialLine(onSerialCommand);
  String sn = settings::serialNumber();
  Serial.printf("Seriennummer: %s (Befehle: hilfe)\n", sn.length() ? sn.c_str() : "(keine)");
}

// Selbsttest: LEDs, Taste und Touch gehoeren dem Test, Netz und Weboberflaeche laufen weiter
void loopSelftest() {
  net::loop();
  realtime::loop(false);  // liest weiter USB-Befehle, verwirft Lichtdaten
  bool btn = digitalRead(BUTTON_PIN) == LOW;
  bool touched = touch::read(display::currentRotation()) != touch::NONE;
  uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    selftest::loop(leds, LED_COUNT, btn, touched);
    FastLED.setBrightness(selftest::brightness());
    FastLED.show();
  } else if (touched) {
    selftest::loop(leds, LED_COUNT, btn, touched);  // Geste kommt nur einmal: nicht verlieren
  }
  if (!selftest::running()) {  // fertig: Taste neu einlesen, Helligkeit weich auf den Zustand
    btnDown = longHandled = false;
    btnSince = now;
    outputLevel = 0;
  }
}


void loop() {
  if (selftest::running()) return loopSelftest();
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

    // Nachtmodus: hoechstens die Nacht-Helligkeit; der Uebergang laeuft langsam (ca. 10 s)
    static bool night = false;
    static uint32_t nightChecked = 0;
    static bool firstCheck = true;
    if (firstCheck || now - nightChecked >= 1000) {
      firstCheck = false;
      nightChecked = now;
      // Beim Start ist die Uhrzeit noch unbekannt: mit eingeschaltetem Nachtmodus die ersten 30 s
      // vorsichtshalber nur Nacht-Helligkeit, damit der Staender nachts nicht kurz voll aufleuchtet.
      // Ohne Internet gilt danach wieder die normale Helligkeit.
      night = net::nightActive(state) || (state.nightEnabled && !net::clockKnown() && now < 30000);
    }
    uint8_t target = state.on ? (night ? min(state.brightness, state.nightBri) : state.brightness) : 0;
    if (night && outputLevel > target && state.on) {  // sanft herunterdimmen statt schnell ausblenden
      static uint32_t lastDim = 0;
      if (now - lastDim >= 40) {
        lastDim = now;
        outputLevel--;
      }
      target = outputLevel;
    }
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
