#include <Preferences.h>
#include <WiFi.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <nvs_flash.h>

#include "config.h"
#include "state.h"

namespace {
Preferences prefs;
bool dirty = false;
uint32_t dirtySince = 0;
String password;  // Geraetepasswort, einmal beim Start geladen
constexpr uint32_t SAVE_DELAY_MS = 3000;  // schont den Flash beim Schieben der Regler

uint32_t packColor(const CRGB &c) { return (uint32_t(c.r) << 16) | (uint32_t(c.g) << 8) | c.b; }
CRGB unpackColor(uint32_t v) { return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF); }
}  // namespace

// Startpresets, bis in der Weboberflaeche eigene gespeichert werden
static const char DEFAULT_PRESETS[] =
    "[{\"name\":\"Abend\",\"s\":{\"on\":1,\"bri\":90,\"fx\":4,\"pal\":3,\"spd\":60,\"int\":128,\"c1\":\"#ff6a1a\",\"c2\":\"#5a1000\"}},"
    "{\"name\":\"Gaming\",\"s\":{\"on\":1,\"bri\":220,\"fx\":13,\"pal\":1,\"spd\":170,\"int\":120,\"c1\":\"#ff0080\",\"c2\":\"#00e0ff\"}},"
    "{\"name\":\"Nordlicht\",\"s\":{\"on\":1,\"bri\":150,\"fx\":16,\"pal\":2,\"spd\":60,\"int\":90,\"c1\":\"#00c060\",\"c2\":\"#7020c0\"}},"
    "{\"name\":\"Kerze\",\"s\":{\"on\":1,\"bri\":110,\"fx\":21,\"pal\":3,\"spd\":110,\"int\":120,\"c1\":\"#ff8a20\",\"c2\":\"#ff3000\"}},"
    "{\"name\":\"Ozean\",\"s\":{\"on\":1,\"bri\":160,\"fx\":23,\"pal\":2,\"spd\":70,\"int\":120,\"c1\":\"#0070c0\",\"c2\":\"#40d0ff\"}},"
    "{\"name\":\"Feuer\",\"s\":{\"on\":1,\"bri\":200,\"fx\":11,\"pal\":6,\"spd\":140,\"int\":150,\"c1\":\"#ff3000\",\"c2\":\"#ffd000\"}}]";

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
  s.argbEnabled = prefs.getBool("argb2", s.argbEnabled);
  s.displayRotation = prefs.getUChar("rot2", s.displayRotation);
  if (s.displayRotation > ROT_AUTO) s.displayRotation = ROT_DEFAULT;
  s.matterEnabled = prefs.getBool("mtr", s.matterEnabled);
  s.wledOpen = prefs.getBool("wled", s.wledOpen);
  prefs.end();
  if (s.maxMilliamps == 0 || s.maxMilliamps > LED_MAX_MILLIAMPS_LIMIT) s.maxMilliamps = LED_MAX_MILLIAMPS_DEFAULT;

  // Geraetepasswort laden, beim ersten Start erzeugen. Ohne leicht verwechselbare Zeichen (0/O, 1/I/L).
  static const char CHARS[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
  prefs.begin("sec", false);
  password = prefs.getString("pw", "");
  if (password.length() < 8) {
    password = "";
    bootloader_random_enable();  // echte Zufallszahlen auch vor dem WLAN-Start
    for (int i = 0; i < 8; i++) password += CHARS[esp_random() % (sizeof(CHARS) - 1)];
    bootloader_random_disable();
    prefs.putString("pw", password);
  }
  prefs.end();
}

String devicePassword() { return password; }

void setDevicePassword(const String &pw) {
  prefs.begin("sec", false);
  prefs.putString("pw", pw);
  prefs.end();
  password = pw;
}

void factoryReset() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  nvs_flash_erase();  // ganzer NVS-Bereich: WLAN, Einstellungen, Presets, Passwort, Matter
  delay(100);
  ESP.restart();
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
  prefs.putBool("argb2", s.argbEnabled);
  prefs.putUChar("rot2", s.displayRotation);
  prefs.putBool("wled", s.wledOpen);
  prefs.end();
}

void saveMatter(bool enabled) {
  prefs.begin("led", false);
  prefs.putBool("mtr", enabled);
  prefs.end();
}

String loadPresets() {
  prefs.begin("presets", true);
  String j = prefs.getString("list", "");
  prefs.end();
  if (j.isEmpty()) j = DEFAULT_PRESETS;
  return j;
}

void savePresets(const String &json) {
  prefs.begin("presets", false);
  prefs.putString("list", json);
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
