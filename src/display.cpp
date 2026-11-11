#include "display.h"

#include <Arduino_GFX_Library.h>

#include "config.h"
#include "fw_version.h"
#include "effects.h"
#include "argb_in.h"
#include "realtime.h"
#include "matter_dev.h"
#include "imu.h"

extern "C" {
#include <qrcode.h>
}

namespace display {

namespace {
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, LCD_ROTATION, true /* IPS */, LCD_W, LCD_H, LCD_OFFSETS);
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

constexpr int PREVIEW_SEGMENTS = 48;

// Aufteilung des Bildschirms; haengt von Groesse und Drehung ab (siehe setLayout)
int W = LCD_W, H = LCD_H;
int fxY, infoY, previewY, previewH;
bool previewLabel;

void setLayout() {
  gfx->setRotation(rotation);
  W = gfx->width();
  H = gfx->height();
  if (H >= 240) {  // 240x240 oder hochkant 170x320
    fxY = 112;
    infoY = 140;
    previewY = H > 240 ? 180 : 196;
    previewH = H - previewY - 14;
    previewLabel = true;
  } else {  // quer 320x170: alles etwas enger, ohne Beschriftung der Vorschau
    fxY = 96;
    infoY = 120;
    previewY = 146;
    previewH = H - previewY - 6;
    previewLabel = false;
  }
}

uint16_t to565(const CRGB &c) { return ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3); }

// Text, der in Groesse 2 nicht in die Zeile passt, wird klein geschrieben.
void textLine(int y, int h, const String &txt, uint8_t size, uint16_t color, String &cache) {
  if (txt == cache) return;
  cache = txt;
  if (size == 2 && txt.indexOf('\n') < 0 && (int)txt.length() * 12 > W - 16) size = 1;
  gfx->fillRect(0, y, W, h, BG);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(8, y + 2);
  gfx->print(txt);
}
// Feste Bildteile; danach zeichnen die Zeilen beim naechsten loop() neu.
void drawStatic() {
  setLayout();
  gfx->fillScreen(BG);
  gfx->drawFastHLine(8, 34, W - 16, ACCENT);
  if (previewLabel) {
    gfx->setTextSize(1);
    gfx->setTextColor(MUTED);
    gfx->setCursor(8, previewY - 12);
    gfx->print("LED-Vorschau");
  }
  lastHeader = lastNet1 = lastNet2 = lastFx = lastInfo = "";
  lastPreviewMs = 0;
}
// Startbild: Kopfhoerer mit Buegel im Regenbogen, darunter eine LED-Reihe und
// der Name. Nur mit Grafikbefehlen gezeichnet, braucht also keinen Bildspeicher.
// Passt hochkant, quer und quadratisch. Gibt die Unterkante zurueck.
uint16_t hue565(uint8_t h) { return to565(CHSV(h, 255, 255)); }
int drawLogo(const char *sub) {
  setLayout();
  gfx->fillScreen(BG);
  bool wide = W > H;
  bool square = !wide && H < W * 3 / 2;   // altes 240x240-Display
  int R = square ? min(W, H) / 4 : min(W, H) * 3 / 10;  // Radius des Buegels
  int cx = W / 2, cy = wide ? H / 2 - 4 : square ? H / 2 - 50 : H / 2 - 40;
  if (wide) cx = W / 4 + 8;                // quer: Bild links, Text rechts
  // Buegel: oberer Halbkreis in 24 Farbstuecken
  for (int k = 0; k < 24; k++)
    gfx->fillArc(cx, cy, R, R - 7, 180 + k * 7.5f, min(180 + (k + 1) * 7.5f + 0.5f, 359.9f), hue565(k * 10));
  // Ohrmuscheln mit leuchtendem Innenring
  int cw = R / 2 + 4, ch = R + 6, cyTop = cy - 4;
  for (int side = -1; side <= 1; side += 2) {
    int x = cx + side * (R - 3) - cw / 2;
    gfx->fillRoundRect(x, cyTop, cw, ch, 7, 0xC618);
    gfx->fillRoundRect(x + 3, cyTop + 3, cw - 6, ch - 6, 5, 0x2104);
    gfx->fillRoundRect(x + cw / 2 - 2, cyTop + 6, 4, ch - 12, 2, hue565(side < 0 ? 0 : 230));
  }
  // LED-Reihe unter dem Kopfhoerer
  int ly = cy + ch + 6, dots = 12, step = (2 * R + cw) / dots;
  for (int k = 0; k < dots; k++) gfx->fillCircle(cx - (dots - 1) * step / 2 + k * step, ly, 2, hue565(k * 21));
  // Name
  int tx = wide ? W / 2 + 6 : 0, ty = wide ? H / 2 - 24 : ly + 18, tw = wide ? W - tx : W;
  auto center = [&](const char *t, uint8_t size, uint16_t col, int y) {
    gfx->setTextSize(size);
    gfx->setTextColor(col);
    gfx->setCursor(tx + max(0, (tw - (int)strlen(t) * 6 * size) / 2), y);
    gfx->print(t);
  };
  center("LED", 2, FG, ty);
  center("Kopfhoerer", 2, FG, ty + 20);
  center(sub, 1, MUTED, ty + 46);
  return ty + 58;
}
// Fortschrittsbalken unter dem Startbild
int barY = 0;
void drawBar(uint8_t pct) {
  int x = 16, w = W - 32;
  if (W > H) { x = W / 2 + 14; w = W - x - 14; }
  gfx->drawRoundRect(x, barY, w, 8, 4, MUTED);
  gfx->fillRoundRect(x + 2, barY + 2, (w - 4) * pct / 100, 4, 2, ACCENT);
}
uint32_t logoUntil = 0;  // bis dahin bleibt das Startbild stehen
bool updating = false;  // Update oder Meldung vor Neustart: normale Anzeige ruht

// QR-Code zeichnen: Die Bibliothek ruft uns mit dem fertigen Code auf.
constexpr int QR_BOX = 156;
int qrX, qrY;  // linke obere Ecke des weissen QR-Felds
void drawQr(esp_qrcode_handle_t q) {
  int n = esp_qrcode_get_size(q);
  int scale = (QR_BOX - 16) / n;  // 4 Module Ruhezone sind im weissen Rand enthalten
  int size = n * scale;
  int x0 = qrX + (QR_BOX - size) / 2, y0 = qrY + (QR_BOX - size) / 2;
  gfx->fillRect(qrX, qrY, QR_BOX, QR_BOX, 0xFFFF);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(q, x, y)) gfx->fillRect(x0 + x * scale, y0 + y * scale, scale, scale, 0x0000);
}

// Bildschirm zum Koppeln mit Apple Home & Co.: QR-Code und Zahlencode
// Hochkant/quadratisch: Titel, QR-Code, Code darunter. Quer: QR-Code links, Text rechts.
void drawPairing(const NetInfo &net) {
  setLayout();
  gfx->fillScreen(BG);
  bool wide = W > H;
  int tx = wide ? QR_BOX + 16 : 8;  // linker Rand des Texts
  qrX = wide ? 6 : (W - QR_BOX) / 2;
  qrY = wide ? (H - QR_BOX) / 2 : 30;
  gfx->setTextSize(wide || W < 200 ? 1 : 2);
  gfx->setTextColor(FG);
  gfx->setCursor(tx, 8);
  gfx->print("Mit Home koppeln");
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = drawQr;
  esp_qrcode_generate(&cfg, matterdev::qrPayload().c_str());
  String c = matterdev::manualCode();
  if (c.length() == 11) c = c.substring(0, 4) + "-" + c.substring(4, 7) + "-" + c.substring(7);
  gfx->setTextSize(wide ? 1 : 2);
  gfx->setTextColor(ACCENT);
  if (wide) gfx->setCursor(tx, 40);
  else gfx->setCursor((W - (int)c.length() * 12) / 2, qrY + QR_BOX + 8);
  gfx->print(c);
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  if (wide) {
    gfx->setCursor(tx, 64);
    gfx->print(net.ip);
    gfx->setCursor(tx, 76);
    gfx->print(String(HOSTNAME) + ".local");
  } else {
    gfx->setCursor(8, qrY + QR_BOX + 36);
    gfx->print(net.ip + "  " + String(HOSTNAME) + ".local");
  }
}
}  // namespace

bool begin() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LCD_BL_ON);
  ok = gfx->begin();
  if (!ok) return false;
  gfx->setTextWrap(false);
  drawLogo(FW_VERSION);
  logoUntil = millis() + BOOT_LOGO_MS;
  return true;
}

void loop(const LedState &s, const NetInfo &net, const CRGB *leds, uint16_t n) {
  if (!ok || updating) return;

  bool wantPairing = matterdev::started() && !matterdev::paired() && net.connected;
  uint8_t want = s.displayRotation;
  if (want == ROT_AUTO) want = imu::rotation() >= 0 ? imu::rotation() : LCD_ROTATION;
  if (logoUntil) {  // Startbild: nur bei Drehung neu zeichnen, danach normale Anzeige
    if ((want & 3) != rotation) {
      rotation = want & 3;
      drawLogo(FW_VERSION);
    }
    if ((int32_t)(millis() - logoUntil) < 0) return;
    logoUntil = 0;
    pairingShown = wantPairing;
    if (wantPairing) drawPairing(net);
    else drawStatic();
  }
  if ((want & 3) != rotation) {
    rotation = want & 3;
    if (wantPairing) drawPairing(net);
    else drawStatic();
  }
  if (wantPairing != pairingShown) {
    pairingShown = wantPairing;
    if (wantPairing) drawPairing(net);
    else drawStatic();
  }
  if (pairingShown) return;

  textLine(6, 24, W < 200 ? "Kopfhoerer" : "LED-Kopfhoerer", 2, FG, lastHeader);

  if (net.setupAp) {
    textLine(44, 20, "WLAN einrichten:", 2, WARN, lastNet1);
    textLine(66, fxY - 68, "Verbinde mit\n " + net.ssid + "\n dann " + net.ip, 1, FG, lastNet2);
  } else if (net.connected) {
    textLine(44, 20, net.ip, 2, GOOD, lastNet1);
    textLine(66, fxY - 68, String(HOSTNAME) + ".local\n " + net.ssid + "\n " + net.detail2, 1, MUTED, lastNet2);
  } else {
    textLine(44, 20, "Verbinde WLAN...", 2, WARN, lastNet1);
    textLine(66, fxY - 68, net.ssid + "\n " + net.detail + "\n " + net.detail2, 1, MUTED, lastNet2);
  }

  if (s.argbEnabled && argb::active())
    textLine(fxY, 24, "Aura (" + String(argb::detectedLeds()) + " LEDs)", 2, GOOD, lastFx);
  else if (s.syncEnabled && realtime::active())
    textLine(fxY, 24, String("PC-Sync ") + realtime::source(), 2, GOOD, lastFx);
  else
    textLine(fxY, 24, effects::NAMES[s.effect < effects::COUNT ? s.effect : 0], 2, FG, lastFx);
  String info = s.on ? ("Hell " + String((s.brightness * 100 + 127) / 255) + "% Tempo " + String(s.speed))
                     : String("Aus");
  textLine(infoY, 20, info, 2, s.on ? MUTED : WARN, lastInfo);

  uint32_t now = millis();
  if (now - lastPreviewMs < 100) return;
  lastPreviewMs = now;
  const int segW = (W - 16) / PREVIEW_SEGMENTS;
  for (int k = 0; k < PREVIEW_SEGMENTS; k++) {
    uint16_t idx = (uint32_t)k * n / PREVIEW_SEGMENTS;
    CRGB c = s.on ? leds[idx] : CRGB::Black;
    // Vorschau ohne globale Helligkeit, damit auch dunkle Einstellungen sichtbar bleiben
    gfx->fillRect(8 + k * segW, previewY, segW, previewH, to565(c));
  }
}

uint8_t currentRotation() { return rotation; }

void message(const char *line1, const char *line2) {
  if (!ok) return;
  updating = true;  // Meldung stehen lassen: loop() zeichnet bis zum Neustart nichts mehr
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

void updateProgress(uint8_t percent) {
  if (!ok) return;
  static uint8_t shown = 255;
  if (!updating) {  // erster Aufruf: Startbild mit Hinweis
    updating = true;
    shown = 255;
    barY = drawLogo("Update, bitte warten");
  }
  if (percent == shown) return;
  shown = percent;
  drawBar(percent);
}

}  // namespace display
