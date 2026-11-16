// PC-Simulation der LED-Effekte (env:sim). Rechnet mit derselben effects.cpp wie die
// Firmware und gibt jedes Bild als eine Zeile Hex aus (RRGGBB je LED, wie /api/live).
//
// Steuerung ueber die Standardeingabe, ein Befehl pro Zeile:
//   effect 5      Effekt-Index (wie in der Weboberflaeche, 0 = erster)
//   palette 2     Palette
//   speed 128     Tempo 0..255
//   intensity 90  Effekt-Staerke 0..255
//   color1 ff7800 / color2 0050ff   Farben fuer Einfarbig, Atmen, Komet ...
//   list          Effekt- und Palettennamen ausgeben (Zeilen beginnen mit "#")
//   quit          beenden
// Startwerte gehen auch als Argumente:  ledsim effect 5 speed 200
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "effects.h"

static LedState state;
static std::mutex mtx;
static std::atomic<bool> running{true};
static std::atomic<bool> needReset{true};

static CRGB parseColor(const std::string &hex) {
  uint32_t v = strtoul(hex.c_str(), nullptr, 16);
  return CRGB((v >> 16) & 255, (v >> 8) & 255, v & 255);
}

static void listNames() {
  for (uint8_t i = 0; i < effects::COUNT; i++) printf("#effect %u %s\n", i, effects::NAMES[i]);
  for (uint8_t i = 0; i < effects::PALETTE_COUNT; i++) printf("#palette %u %s\n", i, effects::PALETTE_NAMES[i]);
  fflush(stdout);
}

// Ein Befehl; gibt false zurueck, wenn er unbekannt ist.
static bool command(const std::string &key, const std::string &val) {
  std::lock_guard<std::mutex> lock(mtx);
  int n = atoi(val.c_str());
  uint8_t b = uint8_t(n < 0 ? 0 : n > 255 ? 255 : n);
  if (key == "effect") {
    if (n >= 0 && n < effects::COUNT) { state.effect = uint8_t(n); needReset = true; }
  } else if (key == "palette") {
    if (n >= 0 && n < effects::PALETTE_COUNT) state.palette = uint8_t(n);
  } else if (key == "speed") state.speed = b;
  else if (key == "intensity") state.intensity = b;
  else if (key == "color1") state.color1 = parseColor(val);
  else if (key == "color2") state.color2 = parseColor(val);
  else if (key == "quit") running = false;
  else return false;
  return true;
}

static void readInput() {
  std::string line;
  while (running && std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string key, val;
    in >> key >> val;
    if (key == "list") listNames();
    else if (!key.empty() && !command(key, val)) fprintf(stderr, "unbekannt: %s\n", line.c_str());
  }
  running = false;  // Eingabe geschlossen (z. B. Blender beendet) -> Programm endet
}

int main(int argc, char **argv) {
  for (int i = 1; i + 1 < argc; i += 2) command(argv[i], argv[i + 1]);
  std::thread(readInput).detach();

  static CRGB leds[LED_COUNT];
  static char line[LED_COUNT * 6 + 2];
  static const char HEXCH[] = "0123456789abcdef";
  const auto start = std::chrono::steady_clock::now();
  auto next = start;
  uint32_t frame = 0;

  while (running) {
    uint32_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    {
      std::lock_guard<std::mutex> lock(mtx);
      if (needReset.exchange(false)) {
        effects::reset();
        fill_solid(leds, LED_COUNT, CRGB::Black);
      }
      effects::render(leds, LED_COUNT, state, now);  // wie in der Firmware: ca. 60 Bilder/s
    }
    // Ausgabe nur jedes zweite Bild (30/s) reicht fuer die Anzeige
    if (frame++ % 2 == 0) {
      char *p = line;
      for (uint16_t i = 0; i < LED_COUNT; i++) {
        // ohne Helligkeit, wie /api/live am Staender (dort wirkt sie erst am LED-Ausgang)
        const CRGB &c = leds[i];
        const uint8_t v[3] = {c.r, c.g, c.b};
        for (uint8_t k = 0; k < 3; k++) { *p++ = HEXCH[v[k] >> 4]; *p++ = HEXCH[v[k] & 15]; }
      }
      *p++ = '\n';
      fwrite(line, 1, size_t(p - line), stdout);
      fflush(stdout);
    }
    next += std::chrono::microseconds(16667);
    std::this_thread::sleep_until(next);
  }
  return 0;
}
