#include "display.h"

#include <Arduino_GFX_Library.h>
#include <esp_random.h>

#include "config.h"
#include "fw_version.h"
#include "effects.h"
#include "argb_in.h"
#include "realtime.h"
#include "matter_dev.h"
#include "imu.h"
#include "touch.h"

extern "C" {
#include <qrcode.h>
}

namespace display {

namespace {
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, LCD_ROTATION, true /* IPS */, LCD_W, LCD_H, LCD_OFFSETS);
bool ok = false;
// STATUS = mit WLAN-Zeilen, HOME = aufgeraeumt (Touch-Board und WLAN verbunden),
// SETTINGS = Regler und Netzdaten hinter dem Zahnrad
// CONFIRM = Rueckfrage, ob ein im Browser gestartetes Firmware-Update erlaubt ist
enum Screen : uint8_t { SCR_STATUS, SCR_HOME, SCR_SETTINGS, SCR_PAIRING, SCR_OFF, SCR_CONFIRM };
#ifdef TOUCH_ADDR
constexpr bool HAS_TOUCH = true;
#else
constexpr bool HAS_TOUCH = false;
#endif
bool settingsOpen = false;
uint32_t settingsTouched = 0;               // letzte Beruehrung auf der Einstellseite
constexpr uint32_t SETTINGS_TIMEOUT_MS = 20000;  // danach zurueck zur Startseite
// Firmware-Update aus dem Browser: erst nach "Erlauben" am Display (Anwesenheit am Geraet)
constexpr uint32_t CONFIRM_ASK_MS = 60000;    // so lange steht die Frage
constexpr uint32_t CONFIRM_VALID_MS = 120000;  // so lange gilt "Erlauben" fuer den Upload
bool confirmAskedSet = false;                   // Frage wurde gestellt
uint32_t confirmAsked = 0, confirmAnswered = 0;  // Zeitpunkte von Frage und Antwort
int8_t confirmAnswer = 0;                       // 1 = erlaubt, -1 = abgelehnt
uint32_t confirmToken = 0;                      // gehoert zu genau der Anfrage, die gefragt hat
int okX, okY, okW, okH, noX, noY, noW, noH;      // Lage der beiden Knoepfe
Screen shown = SCR_STATUS;  // welcher Bildschirm gerade zu sehen ist
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

void setLayout(bool home = false) {
  gfx->setRotation(rotation);
  W = gfx->width();
  H = gfx->height();
  if (home) {  // ohne WLAN-Zeilen: Effekt nach oben, Vorschau groesser
    fxY = 48;
    infoY = 76;
    previewLabel = H >= 240;
    previewY = previewLabel ? 124 : 108;
    previewH = H - previewY - (previewLabel ? 14 : 6);
  } else if (H >= 240) {  // 240x240 oder hochkant 170x320
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
// w = Breite der Zeile (0 = ganze Breite), z. B. damit das Zahnrad stehen bleibt.
void textLine(int y, int h, const String &txt, uint8_t size, uint16_t color, String &cache, int w = 0) {
  if (txt == cache) return;
  cache = txt;
  int margin = w ? 10 : 16;
  if (!w) w = W;
  if (size == 2 && txt.indexOf('\n') < 0 && (int)txt.length() * 12 > w - margin) size = 1;
  gfx->fillRect(0, y, w, h, BG);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(8, y + 2);
  gfx->print(txt);
}
// Zahnrad oben rechts (nur mit Touch); Trefferflaeche 48x44 in der Ecke
void drawGear() {
  int cx = W - 20, cy = 17;
  for (int k = 0; k < 8; k++)
    gfx->fillCircle(cx + lround(9 * cos(k * PI / 4)), cy + lround(9 * sin(k * PI / 4)), 3, MUTED);
  gfx->fillCircle(cx, cy, 8, MUTED);
  gfx->fillCircle(cx, cy, 3, BG);
}
bool inCorner(const touch::Point &p) { return p.x >= W - 48 && p.y < 44; }

// Feste Bildteile; danach zeichnen die Zeilen beim naechsten loop() neu.
void drawStatic(bool home) {
  setLayout(home);
  gfx->fillScreen(BG);
  gfx->drawFastHLine(8, 34, W - 16, ACCENT);
  if (HAS_TOUCH) drawGear();
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
// Passt hochkant, quer und quadratisch, mit Copyright.
uint16_t hue565(uint8_t h) { return to565(CHSV(h, 255, 255)); }
int rowY, rowCx, barX, barY, barW;  // Lage von Untertitel und Balken, setzt drawLogo
void drawLogo(const char *sub) {
  setLayout();
  gfx->fillScreen(BG);
  bool wide = W > H;
  bool square = !wide && H < W * 3 / 2;   // altes 240x240-Display
  int R = square ? min(W, H) / 4 : min(W, H) * 3 / 10;  // Radius des Buegels
  int cx = W / 2, cy = wide ? H / 2 - 4 : square ? H / 2 - 58 : H / 2 - 40;
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
  // Update-Anzeige eine Zeile unter der Version; 240x240: etwas enger, damit das Copyright frei bleibt
  rowY = ty + 46 + (square ? 8 : 14);
  rowCx = tx + tw / 2;
  barY = rowY + (square ? 14 : 16);  // Platz fuer die Aktivitaetsanzeige in der Zeile darueber
  barX = wide ? W / 2 + 14 : 16;
  barW = wide ? W - barX - 14 : W - 32;
  // Copyright: hochkant/quadratisch ganz unten, quer unter dem Kopfhoerer
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  int cwTxt = strlen(COPYRIGHT_TEXT) * 6;
  if (wide) gfx->setCursor(max(0, cx - cwTxt / 2), ly + 8);
  else gfx->setCursor(max(0, (W - cwTxt) / 2), H - 11);
  gfx->print(COPYRIGHT_TEXT);
}

// ---------- Update-Anzeige: Balken mit Farbverlauf, drehende Aktivitaetsanzeige, Prozent ----------
uint8_t barShown = 0;  // bis hierhin ist der Balken schon gefuellt (in Prozent)
void drawBarFrame() {
  gfx->fillRect(barX + 2, barY + 2, barW - 4, 4, BG);  // alte Fuellung weg (neuer Versuch)
  gfx->drawRoundRect(barX, barY, barW, 8, 4, MUTED);
  barShown = 0;
}
// Fuellt nur das neue Stueck; Farbe laeuft ueber die Breite von Rot ueber Gelb/Gruen nach Blau.
void drawBar(uint8_t pct) {
  int inner = barW - 4, from = inner * barShown / 100, to = inner * pct / 100;
  for (int x = from; x < to; x++) gfx->drawFastVLine(barX + 2 + x, barY + 2, 4, hue565(x * 160 / inner));
  barShown = pct;
}
// Aktivitaetsanzeige: 8 Striche im Kreis, der hellste wandert im Uhrzeigersinn,
// die folgenden werden dunkler (wie das bekannte Warte-Symbol).
constexpr int SPOKES = 8;
uint16_t gray565(uint8_t v) { return ((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3); }
void drawSpinner(int x, int y, uint8_t step) {
  gfx->fillCircle(x, y, 10, BG);
  for (int k = 0; k < SPOKES; k++) {
    float a = (k * 360.0f / SPOKES - 90) * DEG_TO_RAD, c = cosf(a), s = sinf(a);
    int age = (step - k + SPOKES) % SPOKES;  // 0 = gerade aktiv
    uint16_t col = gray565(max(60, 235 - age * 28));
    // Strich als schmales Viereck (2 Dreiecke) von Radius 4,5 bis 9,5, rund 2 px breit
    auto P = [&](float r, float o, int16_t &px, int16_t &py) {
      px = x + lroundf(c * r - s * o);
      py = y + lroundf(s * r + c * o);
    };
    int16_t ax, ay, bx, by, cx2, cy2, dx, dy;
    P(4.5f, -0.9f, ax, ay); P(9.5f, -0.9f, bx, by); P(9.5f, 0.9f, cx2, cy2); P(4.5f, 0.9f, dx, dy);
    gfx->fillTriangle(ax, ay, bx, by, cx2, cy2, col);
    gfx->fillTriangle(ax, ay, cx2, cy2, dx, dy, col);
  }
}
void drawPercent(uint8_t pct) {
  char t[16];
  snprintf(t, sizeof(t), "Update %3u %%", (unsigned)pct);
  int w = 13 * 6, x0 = rowCx - (w + 24) / 2;  // Aktivitaetsanzeige links, Text rechts daneben
  gfx->fillRect(x0 + 24, rowY - 2, w, 12, BG);
  gfx->setTextSize(1);
  gfx->setTextColor(FG);
  gfx->setCursor(x0 + 24, rowY);
  gfx->print(t);
}
int spinnerX() { return rowCx - (13 * 6 + 24) / 2 + 9; }

#ifdef TOUCH_ADDR
constexpr const char *OFF_HINT = "Aus - antippen";
#else
constexpr const char *OFF_HINT = "Aus - Taste druecken";
#endif
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

// ---------- Rueckfrage Firmware-Update ----------
// Zeitablauf fest eintragen (sonst kaeme die Frage nach dem millis()-Ueberlauf wieder)
void confirmTick() {
  uint32_t now = millis();
  if (!confirmAskedSet) return;
  if (!confirmAnswer && now - confirmAsked >= CONFIRM_ASK_MS) confirmAnswer = -1, confirmAnswered = now;
  if (confirmAnswer == 1 && now - confirmAnswered >= CONFIRM_VALID_MS) confirmAnswer = -1, confirmAnswered = now;
  if (confirmAnswer == -1 && now - confirmAnswered >= CONFIRM_VALID_MS) confirmAskedSet = false, confirmAnswer = 0;
}
bool confirmOpen() {
  confirmTick();
  return confirmAskedSet && !confirmAnswer;
}

void drawButton(int x, int y, int w, int h, uint16_t bg, uint16_t fg, const char *t) {
  gfx->fillRoundRect(x, y, w, h, 10, bg);
  gfx->setTextSize(2);
  gfx->setTextColor(fg);
  gfx->setCursor(x + (w - (int)strlen(t) * 12) / 2, y + (h - 16) / 2);
  gfx->print(t);
}

void drawConfirm() {
  setLayout();
  gfx->fillScreen(BG);
  bool wide = W > H;
  gfx->setTextSize(2);
  gfx->setTextColor(WARN);
  gfx->setCursor(8, 10);
  gfx->print("Update?");
  gfx->drawFastHLine(8, 34, W - 16, ACCENT);
  gfx->setTextSize(1);
  gfx->setTextColor(FG);
  const char *lines[] = {"Im Browser wurde ein", "Firmware-Update", "gestartet. Nur erlauben,", "wenn du das warst."};
  if (!HAS_TOUCH) {
    gfx->setTextColor(MUTED);
    gfx->setCursor(8, 180);
    gfx->print("Sonst 60 s warten = abgelehnt");
    gfx->setTextColor(FG);
  }
  for (int i = 0; i < 4; i++) {
    gfx->setCursor(8, 46 + i * 14);
    gfx->print(lines[i]);
  }
  if (!HAS_TOUCH) {  // altes Board: BOOT-Taste
    gfx->setTextSize(2);
    gfx->setTextColor(GOOD);
    gfx->setCursor(8, 130);
    gfx->print("BOOT-Taste");
    gfx->setCursor(8, 150);
    gfx->print("= erlauben");
    okW = noW = 0;
    return;
  }
  if (wide) {  // nebeneinander unten
    okY = noY = H - 54;
    okH = noH = 46;
    noX = 8, noW = W / 2 - 12;
    okX = W / 2 + 4, okW = W / 2 - 12;
  } else {     // untereinander
    okX = noX = 8;
    okW = noW = W - 16;
    okY = H - 112, okH = 50;
    noY = H - 54, noH = 46;
  }
  drawButton(okX, okY, okW, okH, GOOD, BG, "Erlauben");
  drawButton(noX, noY, noW, noH, 0x31A6, FG, "Ablehnen");
}

// ---------- Einstellseite: drei Regler (Helligkeit, Tempo, Effekt-Staerke) und Netzdaten ----------
// Hochkant untereinander, quer: Regler links, Netzdaten rechts.
constexpr int SL_X = 12, SL_Y0 = 44;
int slW, slRowH, infoX, infoY0;
int lastSlider[3];
const char *lastIntLabel = nullptr;
bool slDown = false;  // Finger lag beim letzten Aufruf auf
int slActive = -1;    // Regler, auf dem der Finger aufgesetzt hat
String lastSetInfo;

void settingsLayout() {
  bool wide = W > H;
  slW = wide ? 170 : W - 2 * SL_X;
  slRowH = wide ? 42 : 54;
  infoX = wide ? SL_X + slW + 16 : 8;
  infoY0 = wide ? SL_Y0 + 4 : SL_Y0 + 3 * slRowH + 4;
}

const char *intLabel(const LedState &s) { return effects::INT_NAMES[s.effect < effects::COUNT ? s.effect : 0]; }

uint8_t &sliderValue(LedState &s, int i) { return i == 0 ? s.brightness : i == 1 ? s.speed : s.intensity; }

// full = mit Beschriftung; sonst nur Prozentwert und Regler neu (weniger Flackern beim Ziehen)
void drawSliderRow(int i, const char *label, uint8_t v, bool enabled, bool full) {
  int y = SL_Y0 + i * slRowH;
  gfx->setTextSize(1);
  if (full) {
    gfx->fillRect(0, y, SL_X + slW + 10, slRowH - 4, BG);
    gfx->setTextColor(enabled ? FG : MUTED);
    gfx->setCursor(SL_X, y + 2);
    gfx->print(label);
  } else {
    gfx->fillRect(SL_X + slW - 36, y, 36, 12, BG);       // Prozentwert
    gfx->fillRect(0, y + 13, SL_X + slW + 10, 18, BG);  // Regler mit Knopf
  }
  if (enabled) {
    String val = String((v * 100 + 127) / 255) + " %";
    gfx->setTextColor(MUTED);
    gfx->setCursor(SL_X + slW - (int)val.length() * 6, y + 2);
    gfx->print(val);
  }
  int fill = (int)v * slW / 255;
  gfx->fillRoundRect(SL_X, y + 18, slW, 8, 4, 0x31A6);
  if (fill > 0) gfx->fillRoundRect(SL_X, y + 18, max(fill, 8), 8, 4, enabled ? ACCENT : MUTED);
  gfx->fillCircle(SL_X + fill, y + 22, 8, enabled ? FG : MUTED);
}

void drawSettings() {
  setLayout();
  settingsLayout();
  gfx->fillScreen(BG);
  gfx->setTextSize(2);
  gfx->setTextColor(FG);
  gfx->setCursor(8, 10);
  gfx->print("Einrichten");
  gfx->drawFastHLine(8, 34, W - 16, ACCENT);
  int cx = W - 20, cy = 17;  // Schliessen: X im Kreis, an der Stelle des Zahnrads
  gfx->fillCircle(cx, cy, 11, 0x31A6);
  for (int d = -1; d <= 1; d++) {
    gfx->drawLine(cx - 4 + d, cy - 4, cx + 4 + d, cy + 4, FG);
    gfx->drawLine(cx - 4 + d, cy + 4, cx + 4 + d, cy - 4, FG);
  }
  lastSlider[0] = lastSlider[1] = lastSlider[2] = -1;
  lastIntLabel = nullptr;
  slDown = false;
  slActive = -1;
  lastSetInfo = "";
}

void drawSettingsInfo(const NetInfo &net) {
  bool wide = W > H;
  String ip = net.connected || net.setupAp ? net.ip : String("nicht verbunden");
  String all = ip + "|" + net.ssid;
  if (all == lastSetInfo) return;
  lastSetInfo = all;
  gfx->fillRect(infoX, infoY0, W - infoX, H - infoY0, BG);
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  gfx->setCursor(infoX, infoY0);
  gfx->print("IP-Adresse");
  gfx->setTextSize(wide || ip.length() * 12 > (unsigned)(W - infoX - 4) ? 1 : 2);
  gfx->setTextColor(net.connected ? GOOD : WARN);
  gfx->setCursor(infoX, infoY0 + 12);
  gfx->print(ip);
  int y = infoY0 + (wide ? 26 : 34);
  gfx->setTextSize(1);
  gfx->setTextColor(MUTED);
  gfx->setCursor(infoX, y);
  gfx->print(String(HOSTNAME) + ".local");
  gfx->setCursor(infoX, y + 14);
  gfx->print(net.ssid);
  if (!wide) {  // quer reicht die Breite nicht fuer die Versionskennung
    gfx->setCursor(infoX, y + 28);
    gfx->print("Firmware");
    gfx->setCursor(infoX, y + 40);
    gfx->print(FW_FULL);
  }
}

void loopSettings(const LedState &s, const NetInfo &net) {
  const char *il = intLabel(s);
  bool intOn = il[0] != 0;
  bool newLabel = il != lastIntLabel;  // anderer Effekt: Beschriftung neu
  lastIntLabel = il;
  const char *labels[3] = {"Helligkeit", "Tempo", intOn ? il : "Staerke (ohne Wirkung)"};
  for (int i = 0; i < 3; i++) {
    uint8_t v = sliderValue(const_cast<LedState &>(s), i);
    bool full = lastSlider[i] < 0 || (i == 2 && newLabel);
    if (v == lastSlider[i] && !full) continue;
    lastSlider[i] = v;
    drawSliderRow(i, labels[i], v, i < 2 || intOn, full);
  }
  drawSettingsInfo(net);
}
}  // namespace

bool begin() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LCD_BL_ON);
  ok = gfx->begin();
  if (!ok) return false;
  gfx->setTextWrap(false);
  drawLogo(FW_FULL);
  logoUntil = millis() + BOOT_LOGO_MS;
  return true;
}

void loop(const LedState &s, const NetInfo &net, const CRGB *leds, uint16_t n) {
  if (!ok || updating) return;

  bool wantPairing = matterdev::started() && !matterdev::paired() && net.connected;
  uint8_t want = s.displayRotation;
  if (want == ROT_AUTO) want = imu::rotation() >= 0 ? imu::rotation() : LCD_ROTATION;
  // Koppeln geht vor; aus = Logo (ausser bei der WLAN-Einrichtung, die braucht die Hinweise)
  if (settingsOpen && millis() - settingsTouched > SETTINGS_TIMEOUT_MS) settingsOpen = false;
  Screen screen = confirmOpen()              ? SCR_CONFIRM
                  : wantPairing                ? SCR_PAIRING
                  : (!s.on && !net.setupAp)  ? SCR_OFF
                  : settingsOpen             ? SCR_SETTINGS
                  : HAS_TOUCH && net.connected ? SCR_HOME
                                             : SCR_STATUS;
  if (screen == SCR_PAIRING || screen == SCR_OFF || screen == SCR_CONFIRM) settingsOpen = false;
  bool redraw = false;
  if (logoUntil) {  // Startbild: nur bei Drehung neu zeichnen, danach normale Anzeige
    if ((want & 3) != rotation) {
      rotation = want & 3;
      drawLogo(FW_FULL);
    }
    if ((int32_t)(millis() - logoUntil) < 0) return;
    logoUntil = 0;
    redraw = true;
  }
  if ((want & 3) != rotation) {
    rotation = want & 3;
    redraw = true;
  }
  if (screen != shown || redraw) {
    shown = screen;
    if (screen == SCR_CONFIRM) drawConfirm();
    else if (screen == SCR_PAIRING) drawPairing(net);
    else if (screen == SCR_OFF) drawLogo(OFF_HINT);
    else if (screen == SCR_SETTINGS) drawSettings();
    else drawStatic(screen == SCR_HOME);
  }
  if (shown == SCR_SETTINGS) return loopSettings(s, net);
  if (shown != SCR_STATUS && shown != SCR_HOME) return;

  textLine(6, 24, W < 200 ? "Kopfhoerer" : "LED-Kopfhoerer", 2, FG, lastHeader, HAS_TOUCH ? W - 36 : 0);

  if (shown == SCR_HOME) {
    // aufgeraeumt: Netzdaten stehen auf der Einstellseite
  } else if (net.setupAp) {
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
  String info = s.on ? ("Hell " + String((s.brightness * 100 + 127) / 255) + "% Tempo " + String((s.speed * 100 + 127) / 255) + "%")
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

uint32_t askUpdate() {
  if (confirmOpen()) return 0;  // es laeuft schon eine Frage: nicht verlaengern
  confirmAskedSet = true;
  confirmAsked = millis();
  confirmAnswer = 0;
  do confirmToken = esp_random(); while (!confirmToken);
  return confirmToken;
}
bool confirming() { return confirmOpen(); }
void answerUpdate(bool yes) {
  if (!confirmOpen()) return;
  confirmAnswer = yes ? 1 : -1;
  confirmAnswered = millis();
}
int8_t updateAnswer(uint32_t token) {
  confirmTick();
  if (!confirmAskedSet || !token || token != confirmToken) return -1;
  return confirmAnswer;
}
void clearUpdateAnswer() {
  confirmAskedSet = false;
  confirmAnswer = 0;
}

int handleTouch(uint8_t gesture, const touch::Point &p, LedState &s) {
  bool tap = gesture == touch::TAP_TOP || gesture == touch::TAP_BOTTOM;
  if (confirmOpen()) {  // Frage offen: nur die beiden Knoepfe zaehlen, sonst nichts durchlassen
    auto in = [&](int x, int y, int w, int h) { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; };
    if (tap && shown == SCR_CONFIRM && !logoUntil) {
      if (in(okX, okY, okW, okH)) answerUpdate(true);
      else if (in(noX, noY, noW, noH)) answerUpdate(false);
    }
    return 1;
  }
  if (!HAS_TOUCH || !ok || updating || logoUntil) return 0;
  if (shown == SCR_STATUS || shown == SCR_HOME) {
    if (!tap || !inCorner(p)) return 0;
    settingsOpen = true;  // Zahnrad: loop() zeichnet die Einstellseite
    settingsTouched = millis();
    return 1;
  }
  // bis loop() nach dem Schliessen neu zeichnet, bleibt alles verbraucht
  if (shown != SCR_SETTINGS) return 0;
  if (!settingsOpen) return 1;
  int &active = slActive;
  if (p.down || gesture) settingsTouched = millis();
  if (p.down && !slDown) {
    active = -1;
    // nur auf dem Regler selbst (Balken +- Knopf), nicht auf Beschriftung oder X
    for (int i = 0; i < 3; i++) {
      int y = SL_Y0 + i * slRowH;
      if (!inCorner(p) && p.y >= y + 8 && p.y < y + 38 && p.x < SL_X + slW + 16) active = i;
    }
    if (active == 2 && !intLabel(s)[0]) active = -1;  // Effekt ohne Staerke-Regler
  }
  slDown = p.down;
  if (!p.down) active = -1;
  if (active >= 0) {
    int v = constrain((p.x - SL_X) * 255 / slW, 0, 255);
    if (active == 0 && v < 1) v = 1;  // ganz dunkel waere "an, aber aus"
    uint8_t &ref = sliderValue(s, active);
    if (ref == v) return 1;
    ref = v;
    return 2;
  }
  if (tap && inCorner(p)) settingsOpen = false;
  return 1;  // auf der Einstellseite wechseln Tippen und Wischen keinen Effekt
}

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
  static uint32_t lastSpin = 0;
  if (!updating) {  // erster Aufruf: Startbild ohne Untertitel, dann Balkenrahmen
    updating = true;
    shown = 255;
    drawLogo("");
    drawBarFrame();
  }
  if (percent > 100) percent = 100;
  uint32_t now = millis();
  if (percent != shown) {
    if (percent < barShown) drawBarFrame();  // neuer Versuch: Balken von vorn
    drawBar(percent);
    drawPercent(percent);
    shown = percent;
  }
  if (now - lastSpin >= 110) {  // ein Schritt je 110 ms, eine Runde knapp 1 s
    lastSpin = now;
    drawSpinner(spinnerX(), rowY + 3, (now / 110) % SPOKES);
  }
}

}  // namespace display
