#include "display.h"

#include <Arduino_GFX_Library.h>

#include "config.h"
#include "effects.h"
#include "realtime.h"

namespace display {

namespace {
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, LCD_ROTATION, true /* IPS */, LCD_W, LCD_H, 0, 0, 0, 0);
bool ok = false;

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
}  // namespace

bool begin() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  ok = gfx->begin();
  if (!ok) return false;
  gfx->setRotation(LCD_ROTATION);
  gfx->fillScreen(BG);
  gfx->setTextWrap(false);
  gfx->drawFastHLine(8, 34, LCD_W - 16, ACCENT);
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  gfx->setCursor(8, PREVIEW_Y - 12);
  gfx->print("LED-Vorschau");
  return true;
}

void loop(const LedState &s, const NetInfo &net, const CRGB *leds, uint16_t n) {
  if (!ok) return;

  textLine(6, 24, "LED-Kopfhoerer", 2, FG, lastHeader);

  if (net.setupAp) {
    textLine(44, 20, "WLAN einrichten:", 2, WARN, lastNet1);
    textLine(66, 40, "Verbinde mit\n " + net.ssid + "\n dann " + net.ip, 1, FG, lastNet2);
  } else if (net.connected) {
    textLine(44, 20, net.ip, 2, GOOD, lastNet1);
    textLine(66, 40, String(HOSTNAME) + ".local\n " + net.ssid, 1, MUTED, lastNet2);
  } else {
    textLine(44, 20, "Verbinde WLAN...", 2, WARN, lastNet1);
    textLine(66, 40, net.ssid, 1, MUTED, lastNet2);
  }

  if (s.syncEnabled && realtime::active())
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
