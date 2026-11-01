#include "display.h"

#include <Arduino_GFX_Library.h>

#include "config.h"
#include "effects.h"
#include "argb_in.h"
#include "realtime.h"
#include "matter_dev.h"

extern "C" {
#include <qrcode.h>
}

namespace display {

namespace {
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, LCD_ROTATION, true /* IPS */, LCD_W, LCD_H, 0, 0, 0, LCD_ROW_OFFSET2);
bool ok = false;
bool pairingShown = false;  // Matter-Koppelbildschirm ist zu sehen
uint8_t rotation = LCD_ROTATION;

constexpr uint16_t BG = 0x0000;
constexpr uint16_t FG = 0xFFFF;
constexpr uint16_t MUTED = 0x8410;
constexpr uint16_t ACCENT = 0x3C9F;
constexpr uint16_t GOOD = 0x07E0;
constexpr uint16_t WARN = 0xFD20;

// zuletzt gezeichneter Zustand, damit nur Aenderungen neu gezeichnet werden
String lastHeader, lastNet1, lastNet2, lastFx, lastInfo;
uint32_t lastPreviewMs = 0;

constexpr int PREVIEW_Y = 196;
constexpr int PREVIEW_H = 30;
constexpr int PREVIEW_SEGMENTS = 48;

uint16_t to565(const CRGB &c) { return ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3); }

void textLine(int y, int h, const String &txt, uint8_t size, uint16_t color, String &cache) {
  if (txt == cache) return;
  cache = txt;
  gfx->fillRect(0, y, LCD_W, h, BG);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(8, y + 2);
  gfx->print(txt);
}
// Feste Bildteile; danach zeichnen die Zeilen beim naechsten loop() neu.
void drawStatic() {
  gfx->setRotation(rotation);
  gfx->fillScreen(BG);
  gfx->drawFastHLine(8, 34, LCD_W - 16, ACCENT);
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  gfx->setCursor(8, PREVIEW_Y - 12);
  gfx->print("LED-Vorschau");
  lastHeader = lastNet1 = lastNet2 = lastFx = lastInfo = "";
  lastPreviewMs = 0;
}
// QR-Code zeichnen: Die Bibliothek ruft uns mit dem fertigen Code auf.
constexpr int QR_Y = 30;
constexpr int QR_BOX = 156;
void drawQr(esp_qrcode_handle_t q) {
  int n = esp_qrcode_get_size(q);
  int scale = (QR_BOX - 16) / n;  // 4 Module Ruhezone sind im weissen Rand enthalten
  int size = n * scale;
  int x0 = (LCD_W - size) / 2, y0 = QR_Y + (QR_BOX - size) / 2;
  gfx->fillRect((LCD_W - QR_BOX) / 2, QR_Y, QR_BOX, QR_BOX, 0xFFFF);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(q, x, y)) gfx->fillRect(x0 + x * scale, y0 + y * scale, scale, scale, 0x0000);
}

// Bildschirm zum Koppeln mit Apple Home & Co.: QR-Code und Zahlencode
void drawPairing(const NetInfo &net) {
  gfx->setRotation(rotation);
  gfx->fillScreen(BG);
  gfx->setTextSize(2);
  gfx->setTextColor(FG);
  gfx->setCursor(8, 8);
  gfx->print("Mit Home koppeln");
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = drawQr;
  esp_qrcode_generate(&cfg, matterdev::qrPayload().c_str());
  String c = matterdev::manualCode();
  if (c.length() == 11) c = c.substring(0, 4) + "-" + c.substring(4, 7) + "-" + c.substring(7);
  gfx->setTextColor(ACCENT);
  gfx->setCursor((LCD_W - c.length() * 12) / 2, 194);
  gfx->print(c);
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  gfx->setCursor(8, 222);
  gfx->print(net.ip + "  " + String(HOSTNAME) + ".local");
}
}  // namespace

bool begin() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  ok = gfx->begin();
  if (!ok) return false;
  gfx->setTextWrap(false);
  drawStatic();
  return true;
}

void loop(const LedState &s, const NetInfo &net, const CRGB *leds, uint16_t n) {
  if (!ok) return;

  bool wantPairing = matterdev::started() && !matterdev::paired() && net.connected;
  if ((s.displayRotation & 3) != rotation) {
    rotation = s.displayRotation & 3;
    if (wantPairing) drawPairing(net);
    else drawStatic();
  }
  if (wantPairing != pairingShown) {
    pairingShown = wantPairing;
    if (wantPairing) drawPairing(net);
    else drawStatic();
  }
  if (pairingShown) return;

  textLine(6, 24, "LED-Kopfhoerer", 2, FG, lastHeader);

  if (net.setupAp) {
    textLine(44, 20, "WLAN einrichten:", 2, WARN, lastNet1);
    textLine(66, 40, "Verbinde mit\n " + net.ssid + "\n dann " + net.ip, 1, FG, lastNet2);
  } else if (net.connected) {
    textLine(44, 20, net.ip, 2, GOOD, lastNet1);
    textLine(66, 40, String(HOSTNAME) + ".local\n " + net.ssid + "\n " + net.detail2, 1, MUTED, lastNet2);
  } else {
    textLine(44, 20, "Verbinde WLAN...", 2, WARN, lastNet1);
    textLine(66, 40, net.ssid + "\n " + net.detail + "\n " + net.detail2, 1, MUTED, lastNet2);
  }

  if (s.argbEnabled && argb::active())
    textLine(112, 24, "Aura (" + String(argb::detectedLeds()) + " LEDs)", 2, GOOD, lastFx);
  else if (s.syncEnabled && realtime::active())
    textLine(112, 24, String("PC-Sync ") + realtime::source(), 2, GOOD, lastFx);
  else
    textLine(112, 24, effects::NAMES[s.effect < effects::COUNT ? s.effect : 0], 2, FG, lastFx);
  String info = s.on ? ("Hell " + String((s.brightness * 100 + 127) / 255) + "% Tempo " + String(s.speed))
                     : String("Aus");
  textLine(140, 20, info, 2, s.on ? MUTED : WARN, lastInfo);

  uint32_t now = millis();
  if (now - lastPreviewMs < 100) return;
  lastPreviewMs = now;
  const int segW = (LCD_W - 16) / PREVIEW_SEGMENTS;
  for (int k = 0; k < PREVIEW_SEGMENTS; k++) {
    uint16_t idx = (uint32_t)k * n / PREVIEW_SEGMENTS;
    CRGB c = s.on ? leds[idx] : CRGB::Black;
    // Vorschau ohne globale Helligkeit, damit auch dunkle Einstellungen sichtbar bleiben
    gfx->fillRect(8 + k * segW, PREVIEW_Y, segW, PREVIEW_H, to565(c));
  }
}

void message(const char *line1, const char *line2) {
  if (!ok) return;
  gfx->fillScreen(BG);
  gfx->setTextSize(2);
  gfx->setTextColor(WARN);
  gfx->setCursor(8, 90);
  gfx->print(line1);
  gfx->setTextColor(FG);
  gfx->setCursor(8, 120);
  gfx->print(line2);
  lastHeader = lastNet1 = lastNet2 = lastFx = lastInfo = "";
}

}  // namespace display
