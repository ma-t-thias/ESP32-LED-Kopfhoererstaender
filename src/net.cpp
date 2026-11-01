#include "net.h"

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "config.h"
#include "effects.h"
#include "argb_in.h"
#include "matter_dev.h"
#include "wled_api.h"
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
uint32_t connectedSince = 0;      // 0 = gerade nicht verbunden
volatile uint8_t lastDisconnectReason = 0;
uint16_t disconnectCount = 0;
String resetReason;

const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_SW: return "Software";
    case ESP_RST_PANIC: return "Absturz";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_BROWNOUT: return "Unterspannung";
    case ESP_RST_DEEPSLEEP: return "Tiefschlaf";
    case ESP_RST_USB: return "USB";
    default: return "unbekannt";
  }
}

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
  }
}

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
  j += ",\"argb\":" + String(s.argbEnabled ? "true" : "false");
  j += ",\"rot\":" + String(s.displayRotation);
  j += ",\"matterAvail\":" + String(matterdev::available() ? "true" : "false");
  j += ",\"matter\":" + String(s.matterEnabled ? "true" : "false");
  j += ",\"matterPaired\":" + String(matterdev::paired() ? "true" : "false");
  if (matterdev::started() && !matterdev::paired()) j += ",\"matterCode\":\"" + matterdev::manualCode() + "\"";
  j += ",\"argbBlocked\":" + String(argb::blocked() ? "true" : "false");
  bool aura = s.argbEnabled && argb::active();
  bool live = aura || (s.syncEnabled && realtime::active());
  String src = aura ? "Mainboard-ARGB, " + String(argb::detectedLeds()) + " LEDs" : String(realtime::source());
  j += ",\"live\":" + String(live ? "true" : "false");
  j += ",\"liveSrc\":\"" + String(live ? src : "") + "\"";
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
  j += ",\"reset\":\"" + resetReason + "\",\"drops\":" + String(disconnectCount);
  j += ",\"dropReason\":\"" + String(WiFi.disconnectReasonName((wifi_err_reason_t)lastDisconnectReason)) + "\"";
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
  // Nur den AP-Teil abschalten; ein Moduswechsel kann die bestehende STA-Verbindung trennen.
  WiFi.softAPdisconnect(false);
  WiFi.enableAP(false);
  setupAp = false;
  Serial.println("Einrichtungs-WLAN beendet");
}

void startServices() {
  if (servicesStarted) return;
  servicesStarted = true;
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("wled", "tcp", 80);  // damit SignalRGB & Co. den Controller finden
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toLowerCase();
    MDNS.addServiceTxt("wled", "tcp", "mac", mac);  // Home Assistant erkennt das Geraet daran wieder
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
  v = argInt("argb", 0, 1, p);         if (p) s.argbEnabled = v;
  v = argInt("rot", 0, 3, p);          if (p) s.displayRotation = v;
  v = argInt("matter", 0, 1, p);
  if (p && matterdev::available() && bool(v) != s.matterEnabled) {  // Matter laesst sich nur beim Start ein- oder ausschalten
    s.matterEnabled = v;
    settings::saveMatter(s.matterEnabled);
    display::message(v ? "Matter an" : "Matter aus", "Neustart...");
    restartAt = millis() + 1500;
  }
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

void handleMatterUnpair() {
  if (!matterdev::started()) {
    server.send(409, "text/plain; charset=utf-8", "Matter ist nicht eingeschaltet.");
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "Kopplung geloescht. Der Staender startet neu und kann dann wieder gekoppelt werden.");
  display::message("Matter entkoppelt", "Neustart...");
  delay(300);
  matterdev::unpair();  // startet selbst neu
}

void handleResetWifi() {
  server.send(200, "text/plain; charset=utf-8", "WLAN-Daten geloescht. Neustart ins Einrichtungs-WLAN.");
  settings::clearWifi();
  display::message("WLAN geloescht", "Neustart...");
  restartAt = millis() + 1500;
}

// ---------- WLED-kompatible Abfrage fuer SignalRGB & Co. ----------
// Nur so viel, wie Sync-Programme zum Erkennen brauchen. Gesteuert wird dann per UDP.
void handleWledState() {
  bool ch = false, fxCh = false;
  if (!wledapi::apply(*st, server.arg("plain"), ch, fxCh)) {
    server.send(400, "application/json", "{\"error\":9}");
    return;
  }
  if (ch && changed) changed(fxCh);
  server.send(200, "application/json", wledapi::state(*st));
}

const CRGB *liveLeds = nullptr;
uint16_t liveCount = 0;

// Aktuelle LED-Farben als Hex-Text (6 Zeichen je LED, ohne globale Helligkeit)
void handleLive() {
  static const char HEXCHARS[] = "0123456789abcdef";
  String out;
  if (liveLeds) {
    out.reserve(liveCount * 6);
    for (uint16_t i = 0; i < liveCount; i++) {
      const CRGB &c = liveLeds[i];
      const uint8_t v[3] = {c.r, c.g, c.b};
      for (uint8_t k = 0; k < 3; k++) {
        out += HEXCHARS[v[k] >> 4];
        out += HEXCHARS[v[k] & 15];
      }
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", out);
}

void handleSavePresets() {
  String j = server.arg("data");
  if (j.length() > 3000 || !j.startsWith("[")) {
    server.send(400, "text/plain", "Presets ungueltig oder zu gross");
    return;
  }
  settings::savePresets(j);
  wledapi::presetsChanged();
  server.send(200, "text/plain", "ok");
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

  resetReason = resetReasonName(esp_reset_reason());
  Serial.printf("Startgrund: %s\n", resetReason.c_str());

  String pass;
  hasCredentials = settings::loadWifi(staSsid, pass);
  WiFi.onEvent(onWifiEvent);
  WiFi.setHostname(HOSTNAME);
  if (hasCredentials) {
    WiFi.mode(WIFI_STA);
    // Wie bei WLED: kein WLAN-Stromsparen. Mit Modem-Sleep brechen bei manchen Routern
    // Verbindungen ab, und UDP-Pakete (PC-Sync) kommen verzoegert an.
    WiFi.setSleep(false);
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
  server.on("/api/live", HTTP_GET, handleLive);
  server.on("/api/presets", HTTP_GET, []() { server.send(200, "application/json", settings::loadPresets()); });
  server.on("/api/presets", HTTP_POST, handleSavePresets);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleSaveWifi);
  server.on("/api/wifi/reset", HTTP_POST, handleResetWifi);
  server.on("/api/matter/unpair", HTTP_POST, handleMatterUnpair);
  // WLED-kompatibel fuer Home Assistant, WLED-App und Sync-Programme (siehe wled_api.h)
  server.on("/json/info", HTTP_GET, []() { server.send(200, "application/json", wledapi::info(*st)); });
  server.on("/json/state", HTTP_GET, []() { server.send(200, "application/json", wledapi::state(*st)); });
  server.on("/json/state", HTTP_POST, handleWledState);
  server.on("/json/si", HTTP_GET, []() {
    server.send(200, "application/json", "{\"state\":" + wledapi::state(*st) + ",\"info\":" + wledapi::info(*st) + "}");
  });
  server.on("/json/eff", HTTP_GET, []() { server.send(200, "application/json", wledapi::effectsJson()); });
  server.on("/json/pal", HTTP_GET, []() { server.send(200, "application/json", wledapi::palettesJson()); });
  server.on("/json/fxdata", HTTP_GET, []() { server.send(200, "application/json", wledapi::fxdataJson()); });
  server.on("/presets.json", HTTP_GET, []() { server.send(200, "application/json", wledapi::presetsJson()); });
  server.on("/json", HTTP_GET, []() {
    server.send(200, "application/json",
                "{\"state\":" + wledapi::state(*st) + ",\"info\":" + wledapi::info(*st) +
                    ",\"effects\":" + wledapi::effectsJson() + ",\"palettes\":" + wledapi::palettesJson() + "}");
  });
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && connectedSince == 0) {
    connectedSince = millis();
    Serial.printf("Verbunden mit \"%s\", IP %s, Signal %d dBm, Kanal %d\n", staSsid.c_str(),
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
  } else if (!connected && connectedSince != 0) {
    connectedSince = 0;
    disconnectCount++;
    Serial.printf("WLAN getrennt (Grund %u: %s), Abbruch Nr. %u\n", lastDisconnectReason,
                  WiFi.disconnectReasonName((wifi_err_reason_t)lastDisconnectReason), disconnectCount);
  }

  if (connected) {
    startServices();
    // Einrichtungs-WLAN erst abschalten, wenn die Verbindung 10 s stabil steht
    // und niemand mehr damit verbunden ist
    if (setupAp && millis() - connectedSince > 10000 && WiFi.softAPgetStationNum() == 0) stopSetupAp();
  } else if (hasCredentials && !setupAp && !servicesStarted && millis() - connectStart > WIFI_CONNECT_TIMEOUT_MS) {
    // Erster Verbindungsversuch nach dem Start fehlgeschlagen -> Einrichtung anbieten
    startSetupAp();
  }

  if (setupAp) dns.processNextRequest();
  server.handleClient();
  if (servicesStarted && strlen(OTA_PASSWORD) > 0) ArduinoOTA.handle();

  if (restartAt && (int32_t)(millis() - restartAt) > 0) ESP.restart();
}

void setLeds(const CRGB *leds, uint16_t n) {
  liveLeds = leds;
  liveCount = n;
}

display::NetInfo info() {
  display::NetInfo i;
  i.connected = WiFi.status() == WL_CONNECTED;
  i.setupAp = setupAp && !i.connected;
  i.ssid = i.setupAp ? String(SETUP_AP_SSID) : staSsid;
  i.ip = i.setupAp ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  if (!i.connected && lastDisconnectReason)
    i.detail = "Abbruch " + String(disconnectCount) + ": " +
               WiFi.disconnectReasonName((wifi_err_reason_t)lastDisconnectReason);
  i.detail2 = "Start: " + resetReason;
  return i;
}

void resetWifiAndRestart() {
  settings::clearWifi();
  display::message("WLAN geloescht", "Neustart...");
  delay(1200);
  ESP.restart();
}

}  // namespace net
