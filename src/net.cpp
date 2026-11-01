#include "net.h"

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "effects.h"
#include "realtime.h"
#include "webui.h"

namespace net {

namespace {
WebServer server(80);
DNSServer dns;
LedState *st = nullptr;
ChangeHandler changed = nullptr;

String staSsid;
bool hasCredentials = false;
bool setupAp = false;
bool servicesStarted = false;
uint32_t connectStart = 0;
uint32_t restartAt = 0;

// ---------- Hilfsfunktionen ----------
String hex(const CRGB &c) {
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
  return buf;
}

bool parseHex(const String &in, CRGB &out) {
  String s = in;
  if (s.startsWith("#")) s.remove(0, 1);
  if (s.length() != 6) return false;
  char *end = nullptr;
  uint32_t v = strtoul(s.c_str(), &end, 16);
  if (*end != '\0') return false;
  out = CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
  return true;
}

String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 4);
  for (char c : in) {
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

String stateJson() {
  const LedState &s = *st;
  String j = "{";
  j += "\"on\":" + String(s.on ? "true" : "false");
  j += ",\"bri\":" + String(s.brightness);
  j += ",\"fx\":" + String(s.effect);
  j += ",\"pal\":" + String(s.palette);
  j += ",\"spd\":" + String(s.speed);
  j += ",\"int\":" + String(s.intensity);
  j += ",\"c1\":\"" + hex(s.color1) + "\"";
  j += ",\"c2\":\"" + hex(s.color2) + "\"";
  j += ",\"ma\":" + String(s.maxMilliamps);
  j += ",\"sync\":" + String(s.syncEnabled ? "true" : "false");
  bool live = s.syncEnabled && realtime::active();
  j += ",\"live\":" + String(live ? "true" : "false");
  j += ",\"liveSrc\":\"" + String(live ? realtime::source() : "") + "\"";
  j += "}";
  return j;
}

String infoJson() {
  String j = "{\"effects\":[";
  for (uint8_t i = 0; i < effects::COUNT; i++) j += String(i ? "," : "") + "\"" + effects::NAMES[i] + "\"";
  j += "],\"palettes\":[";
  for (uint8_t i = 0; i < effects::PALETTE_COUNT; i++) j += String(i ? "," : "") + "\"" + effects::PALETTE_NAMES[i] + "\"";
  j += "],\"leds\":" + String(LED_COUNT);
  j += ",\"maMax\":" + String(LED_MAX_MILLIAMPS_LIMIT);
  j += ",\"host\":\"" + String(HOSTNAME) + "\"";
  j += ",\"ssid\":\"" + jsonEscape(staSsid) + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  j += "}";
  return j;
}

long argInt(const char *name, long lo, long hi, bool &present) {
  present = server.hasArg(name);
  if (!present) return lo;
  long v = server.arg(name).toInt();
  return constrain(v, lo, hi);
}

void startSetupAp() {
  if (setupAp) return;
  WiFi.mode(hasCredentials ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(SETUP_AP_SSID);
  dns.start(53, "*", WiFi.softAPIP());
  setupAp = true;
  Serial.printf("Einrichtungs-WLAN \"%s\" aktiv, Adresse %s\n", SETUP_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void stopSetupAp() {
  if (!setupAp) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  setupAp = false;
  Serial.println("Einrichtungs-WLAN beendet");
}

void startServices() {
  if (servicesStarted) return;
  servicesStarted = true;
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("wled", "tcp", 80);  // damit SignalRGB & Co. den Controller finden
  }
  realtime::begin();
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() { display::message("Update...", "bitte warten"); });
    ArduinoOTA.begin();
  }
}

// ---------- HTTP-Handler ----------
void handleRoot() {
  if (WiFi.status() == WL_CONNECTED) server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  else server.send_P(200, "text/html; charset=utf-8", SETUP_HTML);
}

void handleSetState() {
  LedState &s = *st;
  bool p;
  bool effectChanged = false;
  long v;
  v = argInt("on", 0, 1, p);           if (p) s.on = v;
  v = argInt("bri", 1, 255, p);        if (p) s.brightness = v;
  v = argInt("fx", 0, effects::COUNT - 1, p);
  if (p && v != s.effect) { s.effect = v; effectChanged = true; }
  v = argInt("pal", 0, effects::PALETTE_COUNT - 1, p); if (p) s.palette = v;
  v = argInt("spd", 0, 255, p);        if (p) s.speed = v;
  v = argInt("int", 0, 255, p);        if (p) s.intensity = v;
  v = argInt("ma", 200, LED_MAX_MILLIAMPS_LIMIT, p); if (p) s.maxMilliamps = v;
  v = argInt("sync", 0, 1, p);         if (p) s.syncEnabled = v;
  if (server.hasArg("c1")) parseHex(server.arg("c1"), s.color1);
  if (server.hasArg("c2")) parseHex(server.arg("c2"), s.color2);
  if (changed) changed(effectChanged);
  server.send(200, "application/json", stateJson());
}

void handleScan() {
  int found = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < found; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;
    if (j.length() > 1) j += ",";
    j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  j += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", j);
}

void handleSaveWifi() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (ssid.isEmpty()) {
    server.send(400, "text/plain; charset=utf-8", "Bitte ein WLAN auswaehlen.");
    return;
  }
  settings::saveWifi(ssid, pass);
  server.send(200, "text/plain; charset=utf-8", "Gespeichert. Der Controller startet neu und verbindet sich mit " + ssid + ".");
  display::message("WLAN gespeichert", "Neustart...");
  restartAt = millis() + 1500;
}

void handleResetWifi() {
  server.send(200, "text/plain; charset=utf-8", "WLAN-Daten geloescht. Neustart ins Einrichtungs-WLAN.");
  settings::clearWifi();
  display::message("WLAN geloescht", "Neustart...");
  restartAt = millis() + 1500;
}

// ---------- WLED-kompatible Abfrage fuer SignalRGB & Co. ----------
// Nur so viel, wie Sync-Programme zum Erkennen brauchen. Gesteuert wird dann per UDP.
String wledInfoJson() {
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  bool live = st->syncEnabled && realtime::active();
  String j = "{\"ver\":\"0.15.0\",\"vid\":2411010,\"name\":\"" DEVICE_NAME "\"";
  j += ",\"leds\":{\"count\":" + String(LED_COUNT) + ",\"rgbw\":false,\"wv\":0,\"cct\":0,\"pwr\":0,\"fps\":60";
  j += ",\"maxpwr\":" + String(st->maxMilliamps) + ",\"maxseg\":1,\"lc\":1,\"seglc\":[1]}";
  j += ",\"str\":false,\"udpport\":21324,\"live\":" + String(live ? "true" : "false");
  j += ",\"lm\":\"" + String(live ? realtime::source() : "") + "\",\"lip\":\"\",\"ws\":-1";
  j += ",\"fxcount\":" + String(effects::COUNT) + ",\"palcount\":" + String(effects::PALETTE_COUNT);
  j += ",\"wifi\":{\"rssi\":" + String(WiFi.RSSI()) + ",\"channel\":" + String(WiFi.channel()) + "}";
  j += ",\"arch\":\"esp32\",\"core\":\"" + String(ESP.getSdkVersion()) + "\"";
  j += ",\"freeheap\":" + String(ESP.getFreeHeap()) + ",\"uptime\":" + String(millis() / 1000);
  j += ",\"brand\":\"WLED\",\"product\":\"" DEVICE_NAME "\",\"mac\":\"" + mac + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  return j;
}

String wledStateJson() {
  const LedState &s = *st;
  String j = "{\"on\":" + String(s.on ? "true" : "false") + ",\"bri\":" + String(s.brightness);
  j += ",\"transition\":7,\"ps\":-1,\"pl\":-1,\"lor\":0";
  j += ",\"seg\":[{\"id\":0,\"start\":0,\"stop\":" + String(LED_COUNT) + ",\"len\":" + String(LED_COUNT);
  j += ",\"on\":true,\"bri\":255,\"fx\":" + String(s.effect) + ",\"sx\":" + String(s.speed);
  j += ",\"ix\":" + String(s.intensity) + ",\"pal\":" + String(s.palette) + "}]}";
  return j;
}

void handleNotFound() {
  if (setupAp) {  // Captive Portal: jede Anfrage auf die Einrichtungsseite umleiten
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Nicht gefunden");
}
}  // namespace

void begin(LedState &state, ChangeHandler onChange) {
  st = &state;
  changed = onChange;

  String pass;
  hasCredentials = settings::loadWifi(staSsid, pass);
  WiFi.setHostname(HOSTNAME);
  if (hasCredentials) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(staSsid.c_str(), pass.c_str());
    connectStart = millis();
    Serial.printf("Verbinde mit \"%s\" ...\n", staSsid.c_str());
  } else {
    startSetupAp();
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", SETUP_HTML); });
  server.on("/api/state", HTTP_GET, []() { server.send(200, "application/json", stateJson()); });
  server.on("/api/state", HTTP_POST, handleSetState);
  server.on("/api/info", HTTP_GET, []() { server.send(200, "application/json", infoJson()); });
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleSaveWifi);
  server.on("/api/wifi/reset", HTTP_POST, handleResetWifi);
  server.on("/json/info", HTTP_GET, []() { server.send(200, "application/json", wledInfoJson()); });
  server.on("/json/state", HTTP_GET, []() { server.send(200, "application/json", wledStateJson()); });
  // Sync-Programme schicken hier z. B. {"on":true,"live":true}. Die Farben kommen per UDP,
  // deshalb wird der Inhalt nur bestaetigt und nicht ausgewertet.
  server.on("/json/state", HTTP_POST, []() { server.send(200, "application/json", "{\"success\":true}"); });
  server.on("/json", HTTP_GET, []() {
    server.send(200, "application/json", "{\"state\":" + wledStateJson() + ",\"info\":" + wledInfoJson() + "}");
  });
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected) {
    if (!servicesStarted) Serial.printf("Verbunden, IP %s\n", WiFi.localIP().toString().c_str());
    startServices();
    // Einrichtungs-WLAN abschalten, sobald die Verbindung steht und niemand mehr darin ist
    if (setupAp && WiFi.softAPgetStationNum() == 0) stopSetupAp();
  } else if (hasCredentials && !setupAp && !servicesStarted && millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
    // Erster Verbindungsversuch nach dem Start fehlgeschlagen -> Einrichtung anbieten
    startSetupAp();
  }

  if (setupAp) dns.processNextRequest();
  server.handleClient();
  if (servicesStarted && strlen(OTA_PASSWORD) > 0) ArduinoOTA.handle();

  if (restartAt && (int32_t)(millis() - restartAt) > 0) ESP.restart();
}

display::NetInfo info() {
  display::NetInfo i;
  i.connected = WiFi.status() == WL_CONNECTED;
  i.setupAp = setupAp && !i.connected;
  i.ssid = i.setupAp ? String(SETUP_AP_SSID) : staSsid;
  i.ip = i.setupAp ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  return i;
}

void resetWifiAndRestart() {
  settings::clearWifi();
  display::message("WLAN geloescht", "Neustart...");
  delay(1200);
  ESP.restart();
}

}  // namespace net
