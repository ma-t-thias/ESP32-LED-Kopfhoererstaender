#include "realtime.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include "config.h"

namespace realtime {

namespace {
WiFiUDP ddp, e131, wledUdp;
bool started = false;

CRGB frame[LED_COUNT];
uint32_t lastPacketMs = 0;
uint32_t timeoutMs = REALTIME_TIMEOUT_MS;
const char *lastSource = "";

constexpr uint16_t PORT_DDP = 4048;
constexpr uint16_t PORT_E131 = 5568;
constexpr uint16_t PORT_WLED = 21324;
constexpr size_t BUF_SIZE = 1460;
uint8_t buf[BUF_SIZE];

void received(const char *src, uint32_t timeout) {
  lastPacketMs = millis();
  timeoutMs = timeout;
  lastSource = src;
}

// Schreibt RGB-Tripel ab LED "start" in den Rahmen
void writeRgb(uint32_t start, const uint8_t *data, size_t len) {
  for (size_t k = 0; k + 2 < len && start < LED_COUNT; k += 3, start++)
    frame[start] = CRGB(data[k], data[k + 1], data[k + 2]);
}

// ---------- DDP (www.3waylabs.com/ddp) ----------
void handleDdp(size_t len) {
  if (len < 10) return;
  uint8_t flags = buf[0];
  if ((flags & 0xC0) != 0x40) return;                    // Version 1
  size_t header = (flags & 0x10) ? 14 : 10;              // mit Zeitstempel 4 Byte mehr
  if (flags & 0x02) return;                              // Abfrage-Paket, ignorieren
  // buf[2] = Datentyp. Sender setzen ihn uneinheitlich (0x00, 0x01, 0x0B); wir nehmen immer RGB 8 Bit an.
  uint32_t offset = (uint32_t(buf[4]) << 24) | (uint32_t(buf[5]) << 16) | (uint32_t(buf[6]) << 8) | buf[7];
  uint16_t dataLen = (uint16_t(buf[8]) << 8) | buf[9];
  if (len < header) return;
  dataLen = min<size_t>(dataLen, len - header);
  writeRgb(offset / 3, buf + header, dataLen);
  received("DDP", REALTIME_TIMEOUT_MS);
}

// ---------- E1.31 / sACN ----------
void handleE131(size_t len) {
  if (len < 126) return;
  if (memcmp(buf + 4, "ASC-E1.17", 9) != 0) return;
  uint16_t universe = (uint16_t(buf[113]) << 8) | buf[114];
  if (buf[125] != 0) return;  // Startcode 0 = DMX-Daten
  uint16_t count = ((uint16_t(buf[123]) << 8) | buf[124]);
  if (count < 1) return;
  count = min<size_t>(count - 1, len - 126);
  // Ein Universum fasst 170 RGB-LEDs; weitere LEDs liegen im naechsten Universum.
  if (universe < REALTIME_E131_UNIVERSE) return;
  uint32_t start = uint32_t(universe - REALTIME_E131_UNIVERSE) * 170;
  if (start >= LED_COUNT) return;
  writeRgb(start, buf + 126, count);
  received("E1.31", REALTIME_TIMEOUT_MS);
}

// ---------- WLED-UDP-Echtzeit ----------
void handleWled(size_t len) {
  if (len < 2) return;
  uint8_t proto = buf[0];
  uint8_t secs = buf[1];
  // Byte 1: Sekunden bis zur Rueckkehr zu den Effekten, 255 = bis zum naechsten Paket bleiben
  uint32_t timeout = secs == 255 ? 3600000u : (secs == 0 ? REALTIME_TIMEOUT_MS : secs * 1000u);
  switch (proto) {
    case 1:  // WARLS: Index, R, G, B
      for (size_t k = 2; k + 3 < len; k += 4)
        if (buf[k] < LED_COUNT) frame[buf[k]] = CRGB(buf[k + 1], buf[k + 2], buf[k + 3]);
      break;
    case 2:  // DRGB: R, G, B ab LED 0
      writeRgb(0, buf + 2, len - 2);
      break;
    case 3:  // DRGBW: Weisskanal wird ignoriert
      for (size_t k = 2, i = 0; k + 3 < len && i < LED_COUNT; k += 4, i++)
        frame[i] = CRGB(buf[k], buf[k + 1], buf[k + 2]);
      break;
    case 4:  // DNRGB: Startindex (2 Byte), dann R, G, B
      if (len < 4) return;
      writeRgb((uint16_t(buf[2]) << 8) | buf[3], buf + 4, len - 4);
      break;
    default:
      return;
  }
  received("WLED-UDP", timeout);
}

// ---------- Adalight ueber USB (OpenRGB "Serial LED Strip", Prismatik, Hyperion) ----------
// Rahmen: 'A' 'd' 'a', Anzahl LEDs - 1 (High, Low), Pruefsumme High ^ Low ^ 0x55, dann R, G, B je LED.
// Kommt ueber die USB-Datenleitungen der Sockel-Buchse (Jumper JP1) oder die USB-C-Buchse am ESP.
// Die Baudrate ist bei USB egal.
LineHandler lineHandler = nullptr;
char line[48];
uint8_t lineLen = 0;

void handleUsb(bool enabled) {
  static uint8_t hdr[6];
  static uint8_t hdrLen = 0, rgbLen = 0;
  static uint8_t rgb[3];
  static uint32_t ledsLeft = 0, idx = 0;
  for (int guard = 0; guard < 2048 && Serial.available() > 0; guard++) {
    uint8_t c = Serial.read();
    if (ledsLeft) {
      rgb[rgbLen++] = c;
      if (rgbLen < 3) continue;
      rgbLen = 0;
      if (enabled && idx < LED_COUNT) frame[idx] = CRGB(rgb[0], rgb[1], rgb[2]);
      idx++;
      if (--ledsLeft == 0 && enabled) received("Adalight USB", REALTIME_TIMEOUT_MS);
      continue;
    }
    if (hdrLen < 3) {  // "Ada" suchen; daneben Textzeilen fuer Befehle sammeln
      if (c == '\n' || c == '\r') {
        line[lineLen] = 0;
        if (lineLen && lineHandler) lineHandler(String(line));
        lineLen = 0;
      } else if (c >= 32 && c < 127 && lineLen < sizeof(line) - 1) {
        line[lineLen++] = c;
      }
      hdrLen = (c == "Ada"[hdrLen]) ? hdrLen + 1 : (c == 'A' ? 1 : 0);
      continue;
    }
    hdr[hdrLen++] = c;
    if (hdrLen < 6) continue;
    hdrLen = 0;
    if ((hdr[3] ^ hdr[4] ^ 0x55) != hdr[5]) continue;  // Pruefsumme falsch: weiter suchen
    ledsLeft = ((uint32_t(hdr[3]) << 8) | hdr[4]) + 1;
    idx = 0;
    lineLen = 0;  // "Ada..." war kein Befehl
  }
}

void drain(WiFiUDP &udp, void (*handler)(size_t), bool enabled) {
  for (int guard = 0; guard < 8; guard++) {  // nicht endlos blockieren
    int size = udp.parsePacket();
    if (size <= 0) return;
    size_t len = udp.read(buf, min<size_t>(size, BUF_SIZE));
    if (enabled) handler(len);
  }
}
}  // namespace

void begin() {
  if (started) return;
  started = true;
  ddp.begin(PORT_DDP);
  e131.beginMulticast(IPAddress(239, 255, (REALTIME_E131_UNIVERSE >> 8) & 0xFF, REALTIME_E131_UNIVERSE & 0xFF), PORT_E131);
  wledUdp.begin(PORT_WLED);
  Serial.println("Echtzeit-Empfang aktiv: DDP 4048, E1.31 5568, WLED-UDP 21324");
}

void loop(bool enabled) {
  handleUsb(enabled);  // geht auch ohne WLAN
  if (started) {
    drain(ddp, handleDdp, enabled);
    drain(e131, handleE131, enabled);
    drain(wledUdp, handleWled, enabled);
  }
  if (!enabled) lastPacketMs = 0;
}

bool active() { return lastPacketMs != 0 && millis() - lastPacketMs < timeoutMs; }

const char *source() { return lastSource; }

void onSerialLine(LineHandler handler) { lineHandler = handler; }

void copyTo(CRGB *leds, uint16_t n) { memcpy(leds, frame, sizeof(CRGB) * min<uint16_t>(n, LED_COUNT)); }

}  // namespace realtime
