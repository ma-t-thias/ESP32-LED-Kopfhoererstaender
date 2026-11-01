#include "matter_dev.h"

#include <Matter.h>

namespace matterdev {

namespace {
MatterEnhancedColorLight light;
bool isStarted = false;

// Matter ruft die Rueckmeldungen in seiner eigenen Task auf. Dort nur merken,
// uebernommen wird in loop() im Hauptprogramm.
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Pending {
  bool on = false, hasOn = false;
  uint8_t bri = 0; bool hasBri = false;
  CRGB color; bool hasColor = false;
  uint8_t hue = 0, sat = 0; bool fromHue = false;  // Farbe kam als Farbton/Saettigung
} pending;

// Waehrend wir selbst Werte an Matter melden, kommen dieselben Werte als
// Rueckmeldung zurueck; die sollen nichts ausloesen.
volatile bool localUpdate = false;

// Was zuletzt an Matter gemeldet wurde
bool sentOn = false;
uint8_t sentBri = 0;
CRGB sentColor;

espHsvColor_t toHsv(const CRGB &c, uint8_t v) {
  espHsvColor_t h = espRgbColorToHsvColor({c.r, c.g, c.b});
  h.v = v;
  return h;
}

CRGB fromHsv(uint8_t h, uint8_t s) {
  espRgbColor_t c = espHsvColorToRgbColor({h, s, 255});
  return CRGB(c.r, c.g, c.b);
}

CRGB fromMireds(uint16_t ct) {
  espRgbColor_t c = espCTToRgbColor(ct);
  return CRGB(c.r, c.g, c.b);
}

void report(const LedState &s) {
  localUpdate = true;
  if (s.on != sentOn) light.setOnOff(s.on);
  if (s.brightness != sentBri) light.setBrightness(s.brightness);
  if (s.color1 != sentColor) light.setColorHSV(toHsv(s.color1, s.brightness));
  localUpdate = false;
  sentOn = s.on;
  sentBri = s.brightness;
  sentColor = s.color1;
}
}  // namespace

void begin(const LedState &s) {
  if (isStarted) return;
  sentOn = s.on;
  sentBri = s.brightness;
  sentColor = s.color1;
  light.begin(s.on, toHsv(s.color1, s.brightness), s.brightness);

  light.onChangeOnOff([](bool on) {
    if (localUpdate) return true;
    portENTER_CRITICAL(&mux);
    pending.on = on;
    pending.hasOn = true;
    portEXIT_CRITICAL(&mux);
    return true;
  });
  light.onChangeBrightness([](uint8_t b) {
    if (localUpdate) return true;
    portENTER_CRITICAL(&mux);
    pending.bri = b;
    pending.hasBri = true;
    portEXIT_CRITICAL(&mux);
    return true;
  });
  light.onChangeColorHSV([](espHsvColor_t c) {
    if (localUpdate) return true;
    CRGB rgb = fromHsv(c.h, c.s);
    portENTER_CRITICAL(&mux);
    pending.color = rgb;
    pending.hasColor = true;
    pending.hue = c.h;
    pending.sat = c.s;
    pending.fromHue = true;
    portEXIT_CRITICAL(&mux);
    return true;
  });
  light.onChangeColorTemperature([](uint16_t ct) {
    if (localUpdate) return true;
    CRGB rgb = fromMireds(ct);
    portENTER_CRITICAL(&mux);
    pending.color = rgb;
    pending.hasColor = true;
    pending.fromHue = false;
    portEXIT_CRITICAL(&mux);
    return true;
  });

  Matter.begin();
  isStarted = true;
  Serial.printf("Matter gestartet, %s. Koppel-Code %s\n", paired() ? "gekoppelt" : "nicht gekoppelt",
                manualCode().c_str());
}

bool started() { return isStarted; }

bool paired() { return isStarted && Matter.isDeviceCommissioned(); }

bool loop(LedState &s, bool &effectChanged) {
  effectChanged = false;
  if (!isStarted) return false;

  Pending p;
  portENTER_CRITICAL(&mux);
  p = pending;
  pending = Pending();
  portEXIT_CRITICAL(&mux);

  bool changed = false;
  if (p.hasOn && p.on != s.on) {
    s.on = p.on;
    changed = true;
  }
  if (p.hasBri) {
    uint8_t b = max<uint8_t>(1, p.bri);
    if (b != s.brightness) {
      s.brightness = b;
      changed = true;
    }
  }
  // Falls Matter unsere eigene Meldung zurueckspielt: gleicher Farbton wie
  // zuletzt gemeldet = keine neue Farbe (sonst wuerde der Effekt umspringen).
  if (p.hasColor && p.fromHue) {
    espHsvColor_t mine = toHsv(sentColor, 255);
    if (abs(int(mine.h) - p.hue) <= 1 && abs(int(mine.s) - p.sat) <= 1) p.hasColor = false;
  }
  if (p.hasColor && p.color != s.color1) {
    // Eine Farbe aus der Home-App soll man sehen: auf "Einfarbig" wechseln.
    s.color1 = p.color;
    if (s.effect != 0) {
      s.effect = 0;
      effectChanged = true;
    }
    changed = true;
  }
  if (changed) {
    sentOn = s.on;
    sentBri = s.brightness;
    sentColor = s.color1;
  }

  // Aenderungen aus Weboberflaeche, Taster oder WLED-API an Matter melden,
  // damit die Home-App den richtigen Zustand zeigt. Hoechstens 4x pro Sekunde.
  static uint32_t lastReport = 0;
  if (millis() - lastReport > 250) {
    lastReport = millis();
    if (s.on != sentOn || s.brightness != sentBri || s.color1 != sentColor) report(s);
  }
  return changed;
}

void unpair() {
  if (!isStarted) return;
  Matter.decommission();
}

String manualCode() { return Matter.getManualPairingCode(); }

String qrPayload() {
  String url = Matter.getOnboardingQRCodeUrl();
  int i = url.indexOf("data=");
  return i >= 0 ? url.substring(i + 5) : url;
}

}  // namespace matterdev
