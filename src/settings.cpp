#include <Preferences.h>

#include "config.h"
#include "state.h"

namespace {
Preferences prefs;
bool dirty = false;
uint32_t dirtySince = 0;
constexpr uint32_t SAVE_DELAY_MS = 3000;  // schont den Flash beim Schieben der Regler

uint32_t packColor(const CRGB &c) { return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b; }
CRGB unpackColor(uint32_t v) { return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF); }
}  // namespace

namespace settings {

void begin(LedState &s) {
  prefs.begin("led", true);
  s.on = prefs.getBool("on", s.on);
  s.brightness = prefs.getUChar("bri", s.brightness);
  s.effect = prefs.getUChar("fx", s.effect);
  s.palette = prefs.getUChar("pal", s.palette);
  s.speed = prefs.getUChar("spd", s.speed);
  s.intensity = prefs.getUChar("int", s.intensity);
  s.color1 = unpackColor(prefs.getUInt("c1", packColor(s.color1)));
  s.color2 = unpackColor(prefs.getUInt("c2", packColor(s.color2)));
  s.maxMilliamps = prefs.getUShort("ma", LED_MAX_MILLIAMPS_DEFAULT);
  s.syncEnabled = prefs.getBool("sync", s.syncEnabled);
  s.argbEnabled = prefs.getBool("argb", s.argbEnabled);
  prefs.end();
  if (s.maxMilliamps == 0 || s.maxMilliamps > LED_MAX_MILLIAMPS_LIMIT) s.maxMilliamps = LED_MAX_MILLIAMPS_DEFAULT;
}

void markDirty() {
  dirty = true;
  dirtySince = millis();
}

void loop(const LedState &s) {
  if (!dirty || millis() - dirtySince < SAVE_DELAY_MS) return;
  dirty = false;
  prefs.begin("led", false);
  prefs.putBool("on", s.on);
  prefs.putUChar("bri", s.brightness);
  prefs.putUChar("fx", s.effect);
  prefs.putUChar("pal", s.palette);
  prefs.putUChar("spd", s.speed);
  prefs.putUChar("int", s.intensity);
  prefs.putUInt("c1", packColor(s.color1));
  prefs.putUInt("c2", packColor(s.color2));
  prefs.putUShort("ma", s.maxMilliamps);
  prefs.putBool("sync", s.syncEnabled);
  prefs.putBool("argb", s.argbEnabled);
  prefs.end();
}

bool loadWifi(String &ssid, String &pass) {
  prefs.begin("wifi", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return ssid.length() > 0;
}

void saveWifi(const String &ssid, const String &pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void clearWifi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}

}  // namespace settings
