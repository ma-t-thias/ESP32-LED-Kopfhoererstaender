// FastLED vor Arduino_GFX einbinden: GFX definiert Makros (z. B. RGB565), die sonst mit
// Namen in FastLED kollidieren (zeigt sich beim PC-Build in env:sim).
#include <FastLED.h>

#include "logo.h"

#include "config.h"

namespace logo {

namespace {
constexpr uint16_t BG = 0x0000;
constexpr uint16_t FG = 0xFFFF;
constexpr uint16_t MUTED = 0x8410;
}  // namespace

uint16_t hue565(uint8_t h) {
  CRGB c = CHSV(h, 255, 255);
  return ((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3);
}

// Nur mit Grafikbefehlen gezeichnet, braucht also keinen Bildspeicher.
// Passt hochkant, quer und quadratisch, mit Copyright.
Layout draw(Arduino_GFX *g, const char *sub) {
  Layout l;
  const int W = g->width(), H = g->height();
  g->fillScreen(BG);
  bool wide = W > H;
  bool square = !wide && H < W * 3 / 2;   // altes 240x240-Display
  int R = square ? min(W, H) / 4 : min(W, H) * 3 / 10;  // Radius des Buegels
  int cx = W / 2, cy = wide ? H / 2 - 4 : square ? H / 2 - 58 : H / 2 - 40;
  if (wide) cx = W / 4 + 8;                // quer: Bild links, Text rechts
  l.spinX = cx;  // zwischen den Ohrmuscheln, unter dem Buegel
  l.spinY = cy + R / 4;
  l.spinR = R * 3 / 10;
  // Buegel: oberer Halbkreis in 24 Farbstuecken
  for (int k = 0; k < 24; k++)
    g->fillArc(cx, cy, R, R - 7, 180 + k * 7.5f, min(180 + (k + 1) * 7.5f + 0.5f, 359.9f), hue565(k * 10));
  // Ohrmuscheln mit leuchtendem Innenring
  int cw = R / 2 + 4, ch = R + 6, cyTop = cy - 4;
  for (int side = -1; side <= 1; side += 2) {
    int x = cx + side * (R - 3) - cw / 2;
    g->fillRoundRect(x, cyTop, cw, ch, 7, 0xC618);
    g->fillRoundRect(x + 3, cyTop + 3, cw - 6, ch - 6, 5, 0x2104);
    g->fillRoundRect(x + cw / 2 - 2, cyTop + 6, 4, ch - 12, 2, hue565(side < 0 ? 0 : 230));
  }
  // LED-Reihe unter dem Kopfhoerer
  int ly = cy + ch + 6, dots = 12, step = (2 * R + cw) / dots;
  for (int k = 0; k < dots; k++) g->fillCircle(cx - (dots - 1) * step / 2 + k * step, ly, 2, hue565(k * 21));
  // Name
  int tx = wide ? W / 2 + 6 : 0, ty = wide ? H / 2 - 24 : ly + 18, tw = wide ? W - tx : W;
  auto center = [&](const char *t, uint8_t size, uint16_t col, int y) {
    g->setTextSize(size);
    g->setTextColor(col);
    g->setCursor(tx + max(0, (tw - (int)strlen(t) * 6 * size) / 2), y);
    g->print(t);
  };
  center("LED", 2, FG, ty);
  center("Kopfhoerer", 2, FG, ty + 20);
  center(sub, 1, MUTED, ty + 46);
  // Update-Anzeige eine Zeile unter der Version; 240x240: etwas enger, damit das Copyright frei bleibt
  l.rowY = ty + 46 + (square ? 8 : 14);
  l.rowCx = tx + tw / 2;
  l.barY = l.rowY + (square ? 14 : 16);  // Prozentzahl in der Zeile darueber
  l.barX = wide ? W / 2 + 14 : 16;
  l.barW = wide ? W - l.barX - 14 : W - 32;
  // Copyright: hochkant/quadratisch ganz unten, quer unter dem Kopfhoerer
  g->setTextSize(1);
  g->setTextColor(MUTED);
  int cwTxt = strlen(COPYRIGHT_TEXT) * 6;
  if (wide) g->setCursor(max(0, cx - cwTxt / 2), ly + 8);
  else g->setCursor(max(0, (W - cwTxt) / 2), H - 11);
  g->print(COPYRIGHT_TEXT);
  return l;
}

}  // namespace logo
