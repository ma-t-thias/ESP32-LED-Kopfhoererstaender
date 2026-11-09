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

// ---------- USB (Adalight, TPM2) ----------
// Kommt ueber den USB-Seriell-Anschluss des ESP32-C6 (GPIO12/13). Das ist die USB-C-Buchse
// des Boards oder, mit geschlossenen Loetbruecken SJ1/SJ2, die USB-C-Buchse im Sockel.
// Die Baudrate ist bei USB egal; OpenRGB & Co. duerfen jede einstellen.
enum class UsbSt : uint8_t { IDLE, ADA_D, ADA_A, ADA_HI, ADA_LO, ADA_CHK, ADA_DATA,
                             TPM_TYPE, TPM_HI, TPM_LO, TPM_DATA, TPM_END };
UsbSt usbSt = UsbSt::IDLE;
uint8_t usbBuf[LED_COUNT * 3];
uint8_t usbHi = 0, usbLo = 0, usbTpmType = 0;
uint32_t usbLen = 0, usbPos = 0;   // erwartete und gelesene Datenbytes
uint32_t usbLastByteMs = 0;
bool usbTxQuiet = false;
constexpr uint32_t USB_GAP_MS = 100;        // laengere Pause mitten im Bild = Bild verwerfen
constexpr size_t USB_MAX_BYTES_PER_LOOP = 4096;

void usbFrameDone(const char *src) {
  uint32_t n = min<uint32_t>(usbLen, sizeof(usbBuf)) / 3;  // empfangene LEDs
  if (n == 0) return;
  if (n >= LED_COUNT) {
    writeRgb(0, usbBuf, LED_COUNT * 3);
  } else {
    // Weniger LEDs eingestellt als vorhanden: auf den ganzen Streifen strecken
    for (uint32_t i = 0; i < LED_COUNT; i++) {
      const uint8_t *p = usbBuf + (i * n / LED_COUNT) * 3;
      frame[i] = CRGB(p[0], p[1], p[2]);
    }
  }
  received(src, REALTIME_TIMEOUT_MS);
  if (!usbTxQuiet) {
    // Haelt das Programm am PC den Anschluss offen, ohne zu lesen, duerfen Log-Ausgaben
    // die Schleife nicht bremsen: ab jetzt verwerfen statt warten.
    usbTxQuiet = true;
    Serial.setTxTimeoutMs(0);
  }
}

void usbByte(uint8_t b) {
  switch (usbSt) {
    case UsbSt::IDLE:
      if (b == 'A') usbSt = UsbSt::ADA_D;
      else if (b == 0xC9) usbSt = UsbSt::TPM_TYPE;
      break;
    // Adalight: 'A' 'd' 'a', LED-Anzahl - 1 (High, Low), Pruefsumme High ^ Low ^ 0x55, dann R G B ...
    case UsbSt::ADA_D: usbSt = b == 'd' ? UsbSt::ADA_A : (b == 'A' ? UsbSt::ADA_D : UsbSt::IDLE); break;
    case UsbSt::ADA_A: usbSt = b == 'a' ? UsbSt::ADA_HI : UsbSt::IDLE; break;
    case UsbSt::ADA_HI: usbHi = b; usbSt = UsbSt::ADA_LO; break;
    case UsbSt::ADA_LO: usbLo = b; usbSt = UsbSt::ADA_CHK; break;
    case UsbSt::ADA_CHK:
      if (b != (usbHi ^ usbLo ^ 0x55)) { usbSt = UsbSt::IDLE; break; }
      usbLen = ((uint32_t(usbHi) << 8 | usbLo) + 1) * 3;
      usbPos = 0;
      usbSt = UsbSt::ADA_DATA;
      break;
    case UsbSt::ADA_DATA:
      if (usbPos < sizeof(usbBuf)) usbBuf[usbPos] = b;
      if (++usbPos >= usbLen) {
        usbFrameDone("USB-Adalight");
        usbSt = UsbSt::IDLE;
      }
      break;
    // TPM2: 0xC9, Typ (0xDA = Daten), Laenge High, Low, Daten, 0x36
    case UsbSt::TPM_TYPE: usbTpmType = b; usbSt = UsbSt::TPM_HI; break;
    case UsbSt::TPM_HI: usbHi = b; usbSt = UsbSt::TPM_LO; break;
    case UsbSt::TPM_LO:
      usbLen = uint32_t(usbHi) << 8 | b;
      usbPos = 0;
      usbSt = usbLen ? UsbSt::TPM_DATA : UsbSt::TPM_END;
      break;
    case UsbSt::TPM_DATA:
      if (usbTpmType == 0xDA && usbPos < sizeof(usbBuf)) usbBuf[usbPos] = b;
      if (++usbPos >= usbLen) usbSt = UsbSt::TPM_END;
      break;
    case UsbSt::TPM_END:
      if (b == 0x36 && usbTpmType == 0xDA) usbFrameDone("USB-TPM2");
      usbSt = UsbSt::IDLE;
      break;
  }
}

void usbPoll(bool enabled) {
  size_t budget = USB_MAX_BYTES_PER_LOOP;
  int avail = Serial.available();
  if (avail <= 0) {
    if (usbSt != UsbSt::IDLE && millis() - usbLastByteMs > USB_GAP_MS) usbSt = UsbSt::IDLE;
    return;
  }
  usbLastByteMs = millis();
  uint8_t tmp[256];
  while (avail > 0 && budget > 0) {
    size_t n = Serial.read(tmp, min<size_t>(min<size_t>(avail, sizeof(tmp)), budget));
    if (n == 0) break;
    budget -= n;
    if (enabled)
      for (size_t k = 0; k < n; k++) usbByte(tmp[k]);
    avail = Serial.available();
  }
  if (!enabled) usbSt = UsbSt::IDLE;
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
  usbPoll(enabled);  // USB geht auch ohne WLAN
  if (!started) {
    if (!enabled) lastPacketMs = 0;
    return;
  }
  drain(ddp, handleDdp, enabled);
  drain(e131, handleE131, enabled);
  drain(wledUdp, handleWled, enabled);
  if (!enabled) lastPacketMs = 0;
}

bool active() { return lastPacketMs != 0 && millis() - lastPacketMs < timeoutMs; }

const char *source() { return lastSource; }

void copyTo(CRGB *leds, uint16_t n) { memcpy(leds, frame, sizeof(CRGB) * min<uint16_t>(n, LED_COUNT)); }

}  // namespace realtime
