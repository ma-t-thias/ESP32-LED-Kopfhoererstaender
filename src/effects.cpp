#include "effects.h"

#include "config.h"

namespace effects {

const char *const NAMES[] = {
    "Einfarbig",      // 0
    "Regenbogen",     // 1
    "Farbverlauf",    // 2
    "Palette",        // 3
    "Atmen",          // 4
    "Farbwischer",    // 5
    "Theater-Jagd",   // 6
    "Komet",          // 7
    "Scanner",        // 8
    "Funkeln",        // 9
    "Konfetti",       // 10
    "Feuer",          // 11
    "Jonglieren",     // 12
    "Wellen",         // 13
    "Sinus",          // 14
    "Farbwechsel",    // 15
    "Nordlicht",      // 16
    "Plasma",         // 17
    "Meteorregen",    // 18
    "Regentropfen",   // 19
    "Herzschlag",     // 20
    "Kerze",          // 21
    "Lichtwelle",     // 22
    "Ozean",          // 23
};
const uint8_t COUNT = sizeof(NAMES) / sizeof(NAMES[0]);

// Bedeutung des Reglers "intensity" je Effekt (wie in der Weboberflaeche); leer = ohne Wirkung
const char *const INT_NAMES[] = {
    "", "Anzahl Regenboegen", "Wiederholungen", "Dichte", "", "", "Abstand", "Schweiflaenge",
    "Schweiflaenge", "Haeufigkeit", "Menge", "Flammenhoehe", "Anzahl Punkte", "Wellengroesse",
    "Frequenz", "", "Kontrast", "Feinheit", "Anzahl Meteore", "Haeufigkeit", "Ausbreitung",
    "Flackern", "Dichte", "Schaum",
};
static_assert(sizeof(INT_NAMES) == sizeof(NAMES), "INT_NAMES passend zu NAMES pflegen");

const char *const PALETTE_NAMES[] = {
    "Regenbogen", "Party", "Ozean", "Lava", "Wald", "Wolken", "Hitze", "Eigene Farben",
};
const uint8_t PALETTE_COUNT = sizeof(PALETTE_NAMES) / sizeof(PALETTE_NAMES[0]);

bool usesColor(uint8_t effect) {
  switch (effect) {
    case 0: case 2: case 4: case 5: case 7: case 8: case 14: case 20: case 21: return true;
    default: return false;
  }
}

namespace {

uint32_t phase = 0;       // laeuft abhaengig von "speed" hoch; 0 = Stillstand
uint32_t lastMs = 0;
int32_t lastCometPos = -1;
uint8_t heat[LED_COUNT];  // fuer "Feuer"

// Nordlicht: dunkel -> gruen -> tuerkis -> violett
const TProgmemRGBGradientPalette_byte AURORA_GP[] = {
    0, 0, 0, 0,  70, 0, 40, 10,  130, 0, 200, 60,  180, 0, 160, 140,  220, 70, 0, 160,  255, 140, 40, 200};
// Ozean: Tiefblau -> Blaugruen -> Schaum
const TProgmemRGBGradientPalette_byte OCEAN_GP[] = {
    0, 0, 4, 24,  90, 0, 30, 90,  170, 0, 110, 150,  220, 40, 170, 200,  255, 200, 240, 255};

struct Meteor {
  int16_t pos;    // Position * 16
  uint8_t speed;  // Positionsschritte * 16 pro Bild
  uint8_t colorIndex;
  bool active;
};
Meteor meteors[6];

struct Drop {
  int16_t center;
  uint8_t age;  // 0 = frei
  uint8_t colorIndex;
};
Drop drops[5];
uint32_t lastStep = 0;  // gemeinsamer Takt fuer Meteorregen und Regentropfen

CRGBPalette16 currentPalette(const LedState &s) {
  switch (s.palette) {
    case 1: return PartyColors_p;
    case 2: return OceanColors_p;
    case 3: return LavaColors_p;
    case 4: return ForestColors_p;
    case 5: return CloudColors_p;
    case 6: return HeatColors_p;
    case 7: return CRGBPalette16(s.color1, s.color2, s.color1);
    default: return RainbowColors_p;
  }
}

// Dreieckswelle 0..max..0 aus einem fortlaufenden Zaehler
uint16_t bounce(uint32_t counter, uint16_t max) {
  if (max == 0) return 0;
  uint32_t period = 2u * max;
  uint32_t p = counter % period;
  return p <= max ? p : period - p;
}

void fillLine(CRGB *leds, int32_t a, int32_t b, const CRGB &c) {
  if (a > b) std::swap(a, b);
  for (int32_t i = a; i <= b; i++) leds[i] = c;
}

void fire(CRGB *leds, uint16_t n, const LedState &s) {
  // Fire2012 (Mark Kriegsman). Intensitaet = weniger Abkuehlung = hoehere Flammen,
  // Geschwindigkeit = mehr Funken.
  uint8_t cooling = 20 + (255 - s.intensity) / 3;
  uint8_t sparking = 50 + s.speed / 2;
  for (uint16_t i = 0; i < n; i++) heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / n) + 2));
  for (uint16_t k = n - 1; k >= 2; k--) heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  if (random8() < sparking) {
    uint8_t y = random8(7);
    heat[y] = qadd8(heat[y], random8(160, 255));
  }
  for (uint16_t j = 0; j < n; j++) leds[j] = ColorFromPalette(HeatColors_p, scale8(heat[j], 240));
}

}  // namespace

void reset() {
  lastCometPos = -1;
  memset(heat, 0, sizeof(heat));
  memset(meteors, 0, sizeof(meteors));
  memset(drops, 0, sizeof(drops));
}

void render(CRGB *leds, uint16_t n, const LedState &s, uint32_t now) {
  uint32_t dt = lastMs ? now - lastMs : 0;
  lastMs = now;
  phase += dt * s.speed;  // bei speed 128: ca. 128000 pro Sekunde

  const CRGBPalette16 pal = currentPalette(s);

  switch (s.effect) {
    case 0:  // Einfarbig
      fill_solid(leds, n, s.color1);
      break;

    case 1: {  // Regenbogen, Intensitaet = wie viele Regenboegen auf dem Streifen
      uint8_t delta = 1 + s.intensity / 32;
      fill_rainbow(leds, n, phase >> 11, delta);
      break;
    }

    case 2: {  // Farbverlauf Farbe 1 -> Farbe 2 -> Farbe 1, wandert langsam
      uint16_t shift = phase >> 11;
      uint8_t reps = 1 + s.intensity / 64;
      for (uint16_t i = 0; i < n; i++) {
        uint8_t t = triwave8(uint8_t((i * 256u * reps) / n + shift));
        leds[i] = blend(s.color1, s.color2, t);
      }
      break;
    }

    case 3: {  // Palette fliessend
      uint8_t scale = 1 + s.intensity / 32;
      uint8_t base = phase >> 11;
      for (uint16_t i = 0; i < n; i++) leds[i] = ColorFromPalette(pal, base + i * scale, 255, LINEARBLEND);
      break;
    }

    case 4: {  // Atmen
      uint8_t level = 20 + scale8(sin8(phase >> 11), 235);
      CRGB c = s.color1;
      c.nscale8_video(level);
      fill_solid(leds, n, c);
      break;
    }

    case 5: {  // Farbwischer: Farbe 1 schiebt sich ueber Farbe 2 und zurueck
      uint32_t cycle = (phase >> 10) % (2u * n);
      for (uint16_t i = 0; i < n; i++) {
        if (cycle < n) leds[i] = (i <= cycle) ? s.color1 : s.color2;
        else leds[i] = (i <= cycle - n) ? s.color2 : s.color1;
      }
      break;
    }

    case 6: {  // Theater-Jagd mit Palettenfarben
      uint8_t spacing = 3 + s.intensity / 64;
      uint16_t off = (phase >> 13) % spacing;
      for (uint16_t i = 0; i < n; i++)
        leds[i] = ((i + off) % spacing == 0) ? ColorFromPalette(pal, i * 2 + (phase >> 13)) : CRGB::Black;
      break;
    }

    case 7:    // Komet: Farbe 1 laeuft hin und her, Intensitaet = Schweiflaenge
    case 8: {  // Scanner: zwei Kometen (Farbe 1 und 2) gegenlaeufig
      uint8_t fade = 8 + (255 - s.intensity) / 3;
      fadeToBlackBy(leds, n, fade);
      int32_t pos = bounce(phase >> 10, n - 1);
      int32_t from = lastCometPos < 0 ? pos : lastCometPos;
      if (abs(pos - from) > 20) from = pos;  // nach Sprung keine Linie ziehen
      fillLine(leds, from, pos, s.color1);
      if (s.effect == 8) fillLine(leds, n - 1 - from, n - 1 - pos, s.color2);
      lastCometPos = pos;
      break;
    }

    case 9: {  // Funkeln
      fadeToBlackBy(leds, n, 4 + s.speed / 16);
      uint8_t chance = 10 + s.intensity / 2;
      if (random8() < chance) leds[random16(n)] = ColorFromPalette(pal, random8());
      if (random8() < chance / 2) leds[random16(n)] = ColorFromPalette(pal, random8());
      break;
    }

    case 10: {  // Konfetti
      fadeToBlackBy(leds, n, 6 + s.speed / 20);
      uint8_t count = 1 + s.intensity / 64;
      for (uint8_t k = 0; k < count; k++)
        leds[random16(n)] += ColorFromPalette(pal, (phase >> 12) + random8(64));
      break;
    }

    case 11:  // Feuer
      fire(leds, n, s);
      break;

    case 12: {  // Jonglieren: acht Punkte mit unterschiedlichem Takt
      fadeToBlackBy(leds, n, 20);
      uint8_t bpmBase = 1 + s.speed / 24;
      uint8_t dots = 3 + s.intensity / 50;
      for (uint8_t d = 0; d < dots; d++)
        leds[beatsin16(bpmBase + d, 0, n - 1)] |= ColorFromPalette(pal, d * (256 / dots));
      break;
    }

    case 13: {  // Wellen (Perlin-Rauschen)
      uint8_t scale = 8 + s.intensity / 4;
      uint16_t z = phase >> 8;
      for (uint16_t i = 0; i < n; i++) leds[i] = ColorFromPalette(pal, inoise8(i * scale, z), 255, LINEARBLEND);
      break;
    }

    case 14: {  // Sinus zwischen Farbe 1 und Farbe 2
      uint8_t freq = 1 + s.intensity / 32;
      uint8_t shift = phase >> 11;
      for (uint16_t i = 0; i < n; i++) leds[i] = blend(s.color1, s.color2, sin8(i * freq + shift));
      break;
    }

    case 15:  // Farbwechsel: ganzer Streifen gleitet durch die Palette
      fill_solid(leds, n, ColorFromPalette(pal, phase >> 12, 255, LINEARBLEND));
      break;

    case 16: {  // Nordlicht: zwei Rauschebenen, Intensitaet = Kontrast (mehr dunkle Stellen)
      static const CRGBPalette16 aurora = AURORA_GP;
      uint32_t z = phase >> 9;
      uint8_t cut = s.intensity / 2;
      for (uint16_t i = 0; i < n; i++) {
        uint8_t hue = inoise8(i * 10, z);
        uint8_t lvl = inoise8(i * 5 + 5000, z / 2 + 3000);
        lvl = qsub8(lvl, cut);
        lvl = scale8(lvl, 255) + scale8(lvl, cut);  // Kontrast wieder anheben
        leds[i] = ColorFromPalette(aurora, hue, lvl, LINEARBLEND);
      }
      break;
    }

    case 17: {  // Plasma: ueberlagerte Sinuswellen durch die Palette, Intensitaet = Feinheit
      uint8_t f = 1 + s.intensity / 40;
      uint16_t t = phase >> 10;
      for (uint16_t i = 0; i < n; i++) {
        uint8_t v = sin8(i * 3 * f + t) / 2 + sin8(i * 2 * f - t * 2 + 80) / 3 + cos8(i * f + t / 3) / 6;
        leds[i] = ColorFromPalette(pal, v + (t >> 2), 255, LINEARBLEND);
      }
      break;
    }

    case 18: {  // Meteorregen: Meteore mit flackerndem Schweif, Intensitaet = Anzahl
      for (uint16_t i = 0; i < n; i++)
        if (random8() < 90) leds[i].fadeToBlackBy(40);
      uint8_t maxCount = 1 + s.intensity / 52;
      uint32_t steps = (phase >> 11) - lastStep;
      lastStep = phase >> 11;
      for (uint8_t k = 0; k < 6; k++) {
        Meteor &m = meteors[k];
        if (!m.active) {
          if (k < maxCount && random8() < 6) m = {int16_t((n - 1) * 16), uint8_t(12 + random8(20)), random8(), true};
          continue;
        }
        m.pos -= int16_t(min<uint32_t>(steps, 8) * m.speed / 4);
        if (m.pos < -64) { m.active = false; continue; }
        for (int8_t j = 0; j < 3; j++) {
          int16_t p = m.pos / 16 + j;
          if (p >= 0 && p < n) leds[p] = ColorFromPalette(pal, m.colorIndex, 255 - j * 70);
        }
      }
      break;
    }

    case 19: {  // Regentropfen: Ringe breiten sich von zufaelligen Stellen aus, Intensitaet = Haeufigkeit
      fadeToBlackBy(leds, n, 30);
      bool tick = (phase >> 12) != lastStep;
      lastStep = phase >> 12;
      for (uint8_t k = 0; k < 5; k++) {
        Drop &d = drops[k];
        if (!d.age) {
          if (random8() < 2 + s.intensity / 16) d = {int16_t(random16(n)), 1, random8()};
          continue;
        }
        uint8_t lvl = 255 - min(255, d.age * 12);
        CRGB c = ColorFromPalette(pal, d.colorIndex, lvl);
        int16_t a = d.center - d.age / 2, b = d.center + d.age / 2;
        if (a >= 0) leds[a] += c;
        if (b < n) leds[b] += c;
        if (tick && ++d.age > 21) d.age = 0;
      }
      break;
    }

    case 20: {  // Herzschlag: Doppelschlag aus der Mitte in Farbe 1, Intensitaet = Ausbreitung
      uint16_t beat = (phase >> 7) % 1024;  // bei Tempo 128 etwa ein Schlag pro Sekunde
      uint16_t mid = n / 2;
      uint16_t spread = 64 + s.intensity * 2;
      for (uint16_t i = 0; i < n; i++) {
        uint16_t d = abs(int(i) - int(mid)) * 512u / n;  // 0..256
        int16_t t = int16_t(beat) - int16_t(d * 256u / spread);
        uint8_t lvl = 0;
        if (t >= 0 && t < 160) lvl = sin8(t * 256 / 160 - 64);
        else if (t >= 220 && t < 340) lvl = scale8(sin8((t - 220) * 256 / 120 - 64), 170);
        lvl = qadd8(scale8(lvl, 230), 25);
        CRGB c = s.color1;
        c.nscale8_video(lvl);
        leds[i] = c;
      }
      break;
    }

    case 21: {  // Kerze: warmes Flackern um Farbe 1, Intensitaet = Flackerstaerke
      uint32_t z = phase >> 7;
      for (uint16_t i = 0; i < n; i++) {
        uint8_t nz = inoise8((i / 6) * 60, z);
        uint8_t dip = scale8(255 - nz, 40 + s.intensity / 2);
        CRGB c = s.color1;
        c.nscale8_video(255 - dip);
        c.g = scale8(c.g, 255 - dip / 3);  // beim Abdunkeln etwas roeter
        leds[i] = c;
      }
      break;
    }

    case 22: {  // Lichtwelle: Palette laeuft symmetrisch von der Mitte nach aussen, Intensitaet = Dichte
      uint8_t scale = 2 + s.intensity / 24;
      uint8_t t = phase >> 11;
      uint16_t mid = n / 2;
      for (uint16_t i = 0; i < n; i++) {
        uint8_t d = abs(int(i) - int(mid)) * scale;
        leds[i] = ColorFromPalette(pal, d - t, 255, LINEARBLEND);
      }
      break;
    }

    case 23: {  // Ozean: drei Wellenschichten mit Schaumkronen, Intensitaet = Schaum
      static const CRGBPalette16 ocean = OCEAN_GP;
      uint16_t t = phase >> 10;
      uint8_t foam = 255 - s.intensity / 3;
      for (uint16_t i = 0; i < n; i++) {
        uint16_t v = sin8(i * 4 + t) + sin8(i * 7 - t * 2 + 40) + sin8(i * 2 + t / 2 + 120);
        uint8_t lvl = v / 3;
        CRGB c = ColorFromPalette(ocean, scale8(lvl, 235), 255, LINEARBLEND);
        if (lvl > foam) c += CRGB(qsub8(lvl, foam) * 3, qsub8(lvl, foam) * 3, qsub8(lvl, foam) * 3);
        leds[i] = c;
      }
      break;
    }

    default:
      fill_solid(leds, n, CRGB::Black);
      break;
  }
}

}  // namespace effects
