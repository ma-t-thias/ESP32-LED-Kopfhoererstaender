#include "net.h"

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include "config.h"
#include "selftest.h"
#include "fw_version.h"
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

// ---------- Uhrzeit ----------
// Minuten seit Mitternacht (Ortszeit), -1 solange die Uhrzeit noch nicht per NTP gesetzt ist
int minutesNow() {
  time_t t = time(nullptr);
  if (t < 1700000000) return -1;
  struct tm lt;
  localtime_r(&t, &lt);
  return lt.tm_hour * 60 + lt.tm_min;
}
String clockText() {
  int m = minutesNow();
  if (m < 0) return "";
  char b[6];
  snprintf(b, sizeof(b), "%02d:%02d", m / 60, m % 60);
  return b;
}

// ---------- Geraetepasswort ----------
// Alles, was etwas aendert, braucht das Passwort im Kopf "X-Key". Lesen (Zustand, Info, Live)
// bleibt offen. Nach 5 falschen Versuchen wird diese Adresse 30 s gesperrt (gegen Durchprobieren);
// andere Geraete im WLAN koennen den Besitzer so nicht aussperren.
uint8_t badTries = 0;
uint32_t lockUntil = 0;
IPAddress badIp;

bool locked() {
  if (lockUntil && (int32_t)(millis() - lockUntil) >= 0) lockUntil = 0;
  return lockUntil && server.client().remoteIP() == badIp;
}

bool keyOk() {
  if (locked()) return false;
  String k = server.header("X-Key");
  if (k.isEmpty()) return false;  // ohne Passwort: kein Versuch, zaehlt nicht
  const String pw = settings::devicePassword();
  uint8_t diff = k.length() != pw.length();
  for (size_t i = 0; i < pw.length(); i++) diff |= (i < k.length() ? k[i] : 0) ^ pw[i];  // gleich lange Pruefung
  if (!diff) {
    badTries = 0;
    return true;
  }
  IPAddress ip = server.client().remoteIP();
  if (ip != badIp) {  // Zaehler gilt je Adresse (die zuletzt falsch versuchende)
    badIp = ip;
    badTries = 0;
  }
  if (++badTries >= 5) {
    badTries = 0;
    lockUntil = millis() + 30000;
  }
  return false;
}

// Anfrage kommt ueber das Einrichtungs-WLAN (nicht ueber das Heimnetz)
bool fromSetupAp() { return setupAp && server.client().localIP() == WiFi.softAPIP(); }

// true = darf; sonst ist die Antwort 401 schon gesendet
bool guard() {
  if (keyOk()) return true;
  server.send(401, "text/plain; charset=utf-8",
              locked() ? "Zu viele falsche Versuche. Bitte 30 s warten." : "Geraetepasswort fehlt oder ist falsch.");
  return false;
}
// Handler nur mit Passwort ausfuehren
std::function<void()> prot(std::function<void()> h) {
  return [h]() { if (guard()) h(); };
}

void handlePassword() {
  String pw = server.arg("new");
  bool okChars = pw.length() >= 8 && pw.length() <= 32;
  for (char c : pw) okChars = okChars && c > 32 && c < 127;
  if (!okChars) {
    server.send(400, "text/plain; charset=utf-8", "Das Passwort braucht 8 bis 32 Zeichen ohne Leerzeichen und Umlaute.");
    return;
  }
  settings::setDevicePassword(pw);
  server.send(200, "text/plain; charset=utf-8", "Passwort geaendert.");
}

void handleFactoryReset() {
  server.send(200, "text/plain; charset=utf-8", "Werkseinstellungen: Alle Daten werden geloescht, der Staender startet ins Einrichtungs-WLAN.");
  display::message("Werksreset", "Neustart...");
  delay(500);
  settings::factoryReset();
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
  j += ",\"wled\":" + String(s.wledOpen ? "true" : "false");
  j += ",\"night\":" + String(s.nightEnabled ? "true" : "false");
  j += ",\"nFrom\":" + String(s.nightFrom) + ",\"nTo\":" + String(s.nightTo) + ",\"nBri\":" + String(s.nightBri);
  j += ",\"nActive\":" + String(nightActive(s) ? "true" : "false");
  j += ",\"time\":\"" + clockText() + "\"";
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

// Platz fuer eine neue Firmware: Groesse des freien App-Slots (0 = kein OTA-Slot)
size_t otaSpace() {
  const esp_partition_t *p = esp_ota_get_next_update_partition(nullptr);
  return p ? p->size : 0;
}
// Klartext, wenn die Datei nicht passt, sonst leer. size 0 = unbekannt, prueft dann Update.begin().
String spaceError(size_t size) {
  size_t space = otaSpace();
  if (!space) return "Auf diesem Staender ist kein Platz fuer ein Update eingerichtet.";
  if (size <= space) return "";
  return "Die Datei ist zu gross: " + String((size + 1023) / 1024) + " KB, Platz ist nur fuer " + String(space / 1024) + " KB.";
}
String infoJson() {
  String j = "{\"effects\":[";
  for (uint8_t i = 0; i < effects::COUNT; i++) j += String(i ? "," : "") + "\"" + effects::NAMES[i] + "\"";
  j += "],\"palettes\":[";
  for (uint8_t i = 0; i < effects::PALETTE_COUNT; i++) j += String(i ? "," : "") + "\"" + effects::PALETTE_NAMES[i] + "\"";
  j += "],\"leds\":" + String(LED_COUNT);
  j += ",\"fw\":\"" + jsonEscape(FW_FULL) + "\",\"board\":\"" BOARD_TAG "\"";
  j += ",\"sn\":\"" + jsonEscape(settings::serialNumber()) + "\"";
  uint32_t stTime;
  int8_t stLast = settings::lastSelftest(stTime);
  j += ",\"stLast\":" + String(stLast) + ",\"stTime\":" + String(stTime);
  j += ",\"fwMax\":" + String(otaSpace());
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
  configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2);  // Uhrzeit fuer den Nachtmodus
  if (strlen(OTA_PASSWORD) > 0) {
    static bool otaRunning = false;
    ArduinoOTA.setHostname(HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
      otaRunning = true;
      display::updateProgress(0);
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
      if (total) display::updateProgress(done * 100ULL / total);
    });
    ArduinoOTA.onError([](ota_error_t) {  // falsches Passwort o. Ae. vor dem Start: nichts tun
      if (!otaRunning) return;
      // abgebrochenes Update: Anzeige wieder freigeben, sauber neu starten
      display::message("Update Fehler", "Neustart...");
      delay(2000);
      ESP.restart();
    });
    ArduinoOTA.begin();
  }
}

// ---------- Firmware-Update ueber die Weboberflaeche ----------
// Der Browser schickt die .bin-Datei als Formular-Upload; sie wird blockweise in den freien
// App-Slot geschrieben (wie beim Update per VS Code). Geprueft werden Dateiname (Board-Kennung),
// Kopf des Images (ESP32-C6) und am Ende die Pruefsumme. Erst danach wird umgeschaltet.
#ifndef CONFIG_IDF_FIRMWARE_CHIP_ID
#define CONFIG_IDF_FIRMWARE_CHIP_ID 0x000D  // ESP32-C6
#endif
String updErr;          // leer = alles gut
bool updShown = false;  // Fortschritt laeuft auf dem Display (danach nur per Neustart weiter)
bool updFirst = false;  // naechster Block ist der Anfang der Datei
bool updStarted = false;  // eine Datei kam an
bool updOk = false;       // Update.end() erfolgreich: neue Firmware ist gesetzt
size_t updSize = 0;     // Groesse der Anfrage, fuer die Prozentanzeige
size_t updFile = 0;     // Groesse der .bin laut Browser (0 = unbekannt)

void handleUpdateUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    updErr = "";
    updShown = false;
    updFirst = true;
    updStarted = true;
    updOk = false;
    updSize = server.clientContentLength();
    updFile = strtoul(server.arg("size").c_str(), nullptr, 10);  // echte Dateigroesse vom Browser
    if (!keyOk()) {
      updErr = "Geraetepasswort fehlt oder ist falsch.";
    } else if (display::updateAnswer(strtoul(server.arg("t").c_str(), nullptr, 10)) != 1) {
      updErr = "Das Update wurde nicht am Display erlaubt.";
    } else if (u.filename.indexOf("_" BOARD_TAG "_") < 0) {
      updErr = "Die Datei passt nicht zu diesem Board. Ihr Name muss _" BOARD_TAG "_ enthalten.";
    } else if (spaceError(updFile).length()) {
      updErr = spaceError(updFile);
    } else if (!Update.begin(updFile ? updFile : UPDATE_SIZE_UNKNOWN)) {
      updErr = Update.errorString();
    } else {
      display::clearUpdateAnswer();  // Erlaubnis gilt fuer genau einen Upload
      Serial.printf("Web-Update: %s\n", u.filename.c_str());
      updShown = true;
      display::updateProgress(0);
    }
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (updErr.length()) return;
    if (updFirst) {  // Image-Kopf: Magic 0xE9, Chip-ID an Byte 12/13 (ESP32-C6 = 13)
      updFirst = false;
      if (u.currentSize < 16 || u.buf[0] != 0xE9 || (u.buf[12] | (u.buf[13] << 8)) != CONFIG_IDF_FIRMWARE_CHIP_ID) {
        updErr = "Das ist keine Firmware fuer den ESP32-C6.";
        Update.abort();
        return;
      }
    }
    if (Update.write(u.buf, u.currentSize) != u.currentSize) {
      updErr = Update.errorString();
      Update.abort();
      return;
    }
    if (updSize) display::updateProgress(min<size_t>(99, Update.progress() * 100ULL / updSize));
  } else if (u.status == UPLOAD_FILE_END) {
    if (updErr.length()) return;
    if (Update.end(true)) updOk = true;  // prueft auch die Pruefsumme
    else updErr = Update.errorString();
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    // Verbindung abgerissen: handleUpdateDone() kommt dann nicht mehr. Die Anzeige ruht
    // seit dem Start (bzw. die neue Firmware ist schon gesetzt), also hier neu starten.
    if (!updOk) Update.abort();
    updStarted = false;
    if (updShown || updOk) {
      display::message(updOk ? "Update fertig" : "Update Fehler", "Neustart...");
      delay(1500);
      ESP.restart();
    }
  }
}

void handleUpdateDone() {
  bool ok = updOk;
  if (!updStarted) updErr = "Keine Datei empfangen.";
  bool restart = ok || updShown;  // Anzeige ruht seit dem Start des Schreibens: dann immer neu starten
  String msg = ok ? String("Update erfolgreich.") : "Update fehlgeschlagen: " + updErr;
  if (restart) msg += " Der Staender startet neu.";
  server.send(ok ? 200 : 400, "text/plain; charset=utf-8", msg);
  updStarted = updOk = false;
  updErr = "";
  if (!restart) return;  // falsche Datei: es wurde nichts geschrieben, normal weiter
  server.client().stop();
  if (ok) display::updateProgress(100);
  else display::message("Update Fehler", "Neustart...");  // Anzeige ruht seit dem Start, also neu starten
  delay(ok ? 800 : 2000);
  ESP.restart();
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
  v = argInt("rot", 0, ROT_AUTO, p);          if (p) s.displayRotation = v;
  v = argInt("wled", 0, 1, p);         if (p) s.wledOpen = v;
  v = argInt("night", 0, 1, p);        if (p) s.nightEnabled = v;
  v = argInt("nFrom", 0, 1439, p);     if (p) s.nightFrom = v;
  v = argInt("nTo", 0, 1439, p);       if (p) s.nightTo = v;
  v = argInt("nBri", 0, 255, p);       if (p) s.nightBri = v;
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
  // Zeitzone sofort setzen: nach einem Neustart laeuft die Systemuhr weiter, sonst wuerde das
  // Nachtfenster bis zur WLAN-Verbindung gegen UTC geprueft
  setenv("TZ", TIMEZONE, 1);
  tzset();
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
  server.on("/api/state", HTTP_POST, prot(handleSetState));
  server.on("/api/info", HTTP_GET, []() { server.send(200, "application/json", infoJson()); });
  server.on("/api/live", HTTP_GET, handleLive);
  server.on("/api/presets", HTTP_GET, []() { server.send(200, "application/json", settings::loadPresets()); });
  server.on("/api/presets", HTTP_POST, prot(handleSavePresets));
  // WLAN-Suche und -Daten: ueber das Einrichtungs-WLAN selbst ohne Passwort (nur wer dort verbunden ist)
  server.on("/api/scan", HTTP_GET, []() { if (fromSetupAp() || guard()) handleScan(); });
  server.on("/api/wifi", HTTP_POST, []() { if (fromSetupAp() || guard()) handleSaveWifi(); });
  server.on("/api/wifi/reset", HTTP_POST, prot(handleResetWifi));
  server.on("/api/matter/unpair", HTTP_POST, prot(handleMatterUnpair));
  server.on("/api/login", HTTP_POST, prot([]() { server.send(200, "text/plain", "ok"); }));
  server.on("/api/password", HTTP_POST, prot(handlePassword));
  server.on("/api/factory-reset", HTTP_POST, prot(handleFactoryReset));
  // Selbsttest fuer die Fertigung: starten (Passwort) und Stand/Ergebnis abfragen
  server.on("/api/selftest", HTTP_GET, []() { server.send(200, "application/json", selftest::json()); });
  server.on("/api/selftest", HTTP_POST, prot([]() {
    if (selftest::running()) return server.send(409, "text/plain; charset=utf-8", "Der Selbsttest laeuft schon.");
    selftest::start();
    server.send(200, "application/json", selftest::json());
  }));
  // Ablauf: /api/update/ask zeigt am Display "Update erlauben?", der Browser fragt
  // /api/update/answer ab (1 = erlaubt, -1 = abgelehnt/abgelaufen, 0 = offen) und laedt erst dann hoch.
  // Die Kennung (t) bindet die Erlaubnis an den Browser, der gefragt hat.
  server.on("/api/update/ask", HTTP_POST, prot([]() {
    String err = spaceError(strtoul(server.arg("size").c_str(), nullptr, 10));  // vor der Frage am Display
    if (err.length()) return server.send(413, "text/plain; charset=utf-8", err);
    uint32_t t = display::askUpdate();
    if (!t) server.send(409, "text/plain; charset=utf-8", "Am Display wartet schon eine Anfrage. Bitte kurz warten.");
    else server.send(200, "application/json", "{\"token\":" + String(t) + "}");
  }));
  server.on("/api/update/answer", HTTP_GET, []() {
    uint32_t t = strtoul(server.arg("t").c_str(), nullptr, 10);
    server.send(200, "application/json", "{\"answer\":" + String(display::updateAnswer(t)) + "}");
  });
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  // WLED-kompatibel fuer Home Assistant, WLED-App und Sync-Programme (siehe wled_api.h)
  server.on("/json/info", HTTP_GET, []() { server.send(200, "application/json", wledapi::info(*st)); });
  server.on("/json/state", HTTP_GET, []() { server.send(200, "application/json", wledapi::state(*st)); });
  // Home Assistant und die WLED-App kennen kein Passwort: nur wenn in den Einstellungen erlaubt
  server.on("/json/state", HTTP_POST, []() { if (st->wledOpen || guard()) handleWledState(); });
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
  static const char *HEADERS[] = {"X-Key"};
  server.collectHeaders(HEADERS, 1);
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

bool nightActive(const LedState &s) {
  int m = minutesNow();
  if (!s.nightEnabled || m < 0 || s.nightFrom == s.nightTo) return false;
  return s.nightFrom < s.nightTo ? (m >= s.nightFrom && m < s.nightTo)   // z. B. 13:00-15:00
                                 : (m >= s.nightFrom || m < s.nightTo);  // ueber Mitternacht
}

bool clockKnown() { return minutesNow() >= 0; }

void resetWifiAndRestart() {
  settings::clearWifi();
  display::message("WLAN geloescht", "Neustart...");
  delay(1200);
  ESP.restart();
}

}  // namespace net
