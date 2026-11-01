#include "wled_api.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "argb_in.h"
#include "config.h"
#include "effects.h"
#include "realtime.h"

namespace wledapi {

namespace {
uint32_t presetsStamp = 0;  // "pmt": aendert sich, wenn Presets gespeichert werden

void putColor(JsonArray arr, const CRGB &c) {
  JsonArray a = arr.add<JsonArray>();
  a.add(c.r);
  a.add(c.g);
  a.add(c.b);
}

// Farbe aus [r,g,b(,w)] oder "RRGGBB"
bool readColor(JsonVariantConst v, CRGB &out) {
  if (v.is<JsonArrayConst>()) {
    JsonArrayConst a = v.as<JsonArrayConst>();
    if (a.size() < 3) return false;
    out = CRGB(a[0].as<uint8_t>(), a[1].as<uint8_t>(), a[2].as<uint8_t>());
    return true;
  }
  if (v.is<const char *>()) {
    const char *t = v.as<const char *>();
    if (*t == '#') t++;
    if (strlen(t) < 6) return false;
    uint32_t x = strtoul(t, nullptr, 16);
    out = CRGB((x >> 16) & 255, (x >> 8) & 255, x & 255);
    return true;
  }
  return false;
}

bool sameState(const LedState &a, const LedState &b) {
  return a.on == b.on && a.brightness == b.brightness && a.effect == b.effect && a.palette == b.palette &&
         a.speed == b.speed && a.intensity == b.intensity && a.color1 == b.color1 && a.color2 == b.color2 &&
         a.syncEnabled == b.syncEnabled;
}

uint8_t clampU8(long v, long lo, long hi) { return (uint8_t)constrain(v, lo, hi); }

// Werte eines Segments (bzw. eines Presets aus der Weboberflaeche) uebernehmen
void applySegment(LedState &s, JsonObjectConst seg, bool &effectChanged) {
  if (!seg["fx"].isNull()) {
    uint8_t fx = clampU8(seg["fx"].as<long>(), 0, effects::COUNT - 1);
    if (fx != s.effect) {
      s.effect = fx;
      effectChanged = true;
    }
  }
  if (!seg["sx"].isNull()) s.speed = clampU8(seg["sx"].as<long>(), 0, 255);
  if (!seg["ix"].isNull()) s.intensity = clampU8(seg["ix"].as<long>(), 0, 255);
  if (!seg["pal"].isNull()) s.palette = clampU8(seg["pal"].as<long>(), 0, effects::PALETTE_COUNT - 1);
  JsonArrayConst col = seg["col"];
  if (!col.isNull()) {
    if (col.size() > 0) readColor(col[0], s.color1);
    if (col.size() > 1) readColor(col[1], s.color2);
  }
  // Nur ein Segment: dessen Helligkeit und Ein/Aus wirken auf den ganzen Streifen
  if (!seg["bri"].isNull()) s.brightness = clampU8(seg["bri"].as<long>(), 1, 255);
  if (seg["on"].is<bool>()) s.on = seg["on"].as<bool>();
}

// Preset aus dem Format der Weboberflaeche: {"name":..,"s":{"on":1,"bri":..,"fx":..,...}}
bool applyPreset(LedState &s, int id, bool &effectChanged) {
  JsonDocument doc;
  if (deserializeJson(doc, settings::loadPresets())) return false;
  JsonArrayConst list = doc.as<JsonArrayConst>();
  if (id < 1 || id > (int)list.size()) return false;
  JsonObjectConst p = list[id - 1]["s"];
  if (p.isNull()) return false;
  if (!p["on"].isNull()) s.on = p["on"].as<int>() != 0;
  if (!p["bri"].isNull()) s.brightness = clampU8(p["bri"].as<long>(), 1, 255);
  JsonDocument seg;
  seg["fx"] = p["fx"];
  seg["sx"] = p["spd"];
  seg["ix"] = p["int"];
  seg["pal"] = p["pal"];
  JsonArray col = seg["col"].to<JsonArray>();
  col.add(p["c1"]);
  col.add(p["c2"]);
  applySegment(s, seg.as<JsonObjectConst>(), effectChanged);
  return true;
}

void fillState(JsonObject o, const LedState &s) {
  o["on"] = s.on;
  o["bri"] = s.brightness;
  o["transition"] = 7;
  o["ps"] = -1;
  o["pl"] = -1;
  JsonObject nl = o["nl"].to<JsonObject>();
  nl["on"] = false;
  nl["dur"] = 60;
  nl["mode"] = 1;
  nl["tbri"] = 0;
  nl["rem"] = -1;
  JsonObject udpn = o["udpn"].to<JsonObject>();
  udpn["send"] = false;
  udpn["recv"] = s.syncEnabled;  // in Home Assistant der Schalter "Sync Receive" = PC-Sync
  udpn["sgrp"] = 1;
  udpn["rgrp"] = 1;
  o["lor"] = 0;
  o["mainseg"] = 0;
  JsonObject seg = o["seg"].to<JsonArray>().add<JsonObject>();
  seg["id"] = 0;
  seg["start"] = 0;
  seg["stop"] = LED_COUNT;
  seg["len"] = LED_COUNT;
  seg["grp"] = 1;
  seg["spc"] = 0;
  seg["of"] = 0;
  seg["on"] = true;
  seg["frz"] = false;
  seg["bri"] = 255;
  seg["cct"] = 127;
  seg["n"] = DEVICE_NAME;
  JsonArray col = seg["col"].to<JsonArray>();
  putColor(col, s.color1);
  putColor(col, s.color2);
  putColor(col, CRGB::Black);
  seg["fx"] = s.effect;
  seg["sx"] = s.speed;
  seg["ix"] = s.intensity;
  seg["pal"] = s.palette;
  seg["sel"] = true;
  seg["rev"] = false;
  seg["mi"] = false;
}

void fillInfo(JsonObject o, const LedState &s) {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  bool aura = s.argbEnabled && argb::active();
  bool live = aura || (s.syncEnabled && realtime::active());
  o["ver"] = "0.15.0";
  o["vid"] = 2411010;
  JsonObject leds = o["leds"].to<JsonObject>();
  leds["count"] = LED_COUNT;
  leds["pwr"] = 0;
  leds["fps"] = 60;
  leds["maxpwr"] = s.maxMilliamps;
  leds["maxseg"] = 1;
  leds["lc"] = 1;  // nur RGB
  leds["seglc"].to<JsonArray>().add(1);
  leds["rgbw"] = false;
  leds["wv"] = false;
  leds["cct"] = false;
  o["str"] = false;
  o["name"] = DEVICE_NAME;
  o["udpport"] = 21324;
  o["live"] = live;
  o["lm"] = live ? (aura ? "Mainboard-ARGB" : realtime::source()) : "";
  o["lip"] = "";
  // Kein "ws": ohne WebSocket fragt Home Assistant regelmaessig ab.
  o["fxcount"] = effects::COUNT;
  o["palcount"] = effects::PALETTE_COUNT;
  o["cpalcount"] = 0;
  JsonObject wifi = o["wifi"].to<JsonObject>();
  wifi["bssid"] = WiFi.BSSIDstr();
  int rssi = WiFi.RSSI();
  wifi["rssi"] = rssi;
  wifi["signal"] = constrain(2 * (rssi + 100), 0, 100);
  wifi["channel"] = WiFi.channel();
  wifi["ap"] = false;
  JsonObject fs = o["fs"].to<JsonObject>();
  fs["u"] = 4;
  fs["t"] = 20;
  fs["pmt"] = presetsStamp;
  o["ndc"] = 0;
  o["arch"] = "esp32-c6";
  o["core"] = ESP.getSdkVersion();
  o["freeheap"] = ESP.getFreeHeap();
  o["uptime"] = millis() / 1000;
  o["opt"] = 0;
  o["brand"] = "WLED";
  o["product"] = "LED-Kopfhoererstaender";
  o["mac"] = mac;
  o["ip"] = WiFi.localIP().toString();
}

String serialize(const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  return out;
}
}  // namespace

String info(const LedState &s) {
  JsonDocument doc;
  fillInfo(doc.to<JsonObject>(), s);
  return serialize(doc);
}

String state(const LedState &s) {
  JsonDocument doc;
  fillState(doc.to<JsonObject>(), s);
  return serialize(doc);
}

String effectsJson() {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (uint8_t i = 0; i < effects::COUNT; i++) a.add(effects::NAMES[i]);
  return serialize(doc);
}

String palettesJson() {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (uint8_t i = 0; i < effects::PALETTE_COUNT; i++) a.add(effects::PALETTE_NAMES[i]);
  return serialize(doc);
}

String fxdataJson() {
  // WLED beschreibt hier, welche Regler ein Effekt nutzt. Leer = Standard.
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (uint8_t i = 0; i < effects::COUNT; i++) a.add("");
  return serialize(doc);
}

String presetsJson() {
  JsonDocument src;
  JsonDocument out;
  out["0"].to<JsonObject>();
  if (!deserializeJson(src, settings::loadPresets())) {
    int id = 1;
    for (JsonObjectConst p : src.as<JsonArrayConst>()) {
      JsonObject o = out[String(id++)].to<JsonObject>();
      o["n"] = p["name"] | "Preset";
      o["on"] = p["s"]["on"].as<int>() != 0;
      o["bri"] = p["s"]["bri"] | 128;
    }
  }
  return serialize(out);
}

bool apply(LedState &s, const String &body, bool &changed, bool &effectChanged) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  JsonObjectConst o = doc.as<JsonObjectConst>();
  changed = false;
  effectChanged = false;
  LedState before = s;

  if (o["on"].is<bool>()) s.on = o["on"].as<bool>();
  else if (o["on"].is<const char *>() && strcmp(o["on"].as<const char *>(), "t") == 0) s.on = !s.on;
  if (!o["bri"].isNull()) {
    long b = o["bri"].as<long>();
    if (b <= 0) s.on = false;
    else s.brightness = clampU8(b, 1, 255);
  }
  if (!o["ps"].isNull()) applyPreset(s, o["ps"].as<int>(), effectChanged);
  if (o["udpn"]["recv"].is<bool>()) s.syncEnabled = o["udpn"]["recv"].as<bool>();

  JsonVariantConst seg = o["seg"];
  if (seg.is<JsonArrayConst>()) {
    for (JsonObjectConst one : seg.as<JsonArrayConst>())
      if ((one["id"] | 0) == 0) applySegment(s, one, effectChanged);
  } else if (seg.is<JsonObjectConst>()) {
    applySegment(s, seg.as<JsonObjectConst>(), effectChanged);
  }

  changed = !sameState(before, s) || effectChanged;
  return true;
}

void presetsChanged() { presetsStamp = millis() / 1000 + 1; }

}  // namespace wledapi
