#include "argb_in.h"

#include <driver/rmt_rx.h>

#include "config.h"

namespace argb {

namespace {
constexpr uint32_t RESOLUTION_HZ = 10000000;  // 1 Tick = 100 ns
constexpr size_t MAX_IN_LEDS = 300;
constexpr size_t CHUNK_SYMBOLS = 256;          // Zwischenpuffer; Daten kommen stueckweise
// WS2812: "0" = ca. 0,4 us high, "1" = ca. 0,8 us high. Grenze bei 0,55 us.
constexpr uint16_t ONE_THRESHOLD_TICKS = 6;
constexpr uint16_t MIN_HIGH_TICKS = 2;         // kuerzer = Stoerung
constexpr uint16_t MAX_HIGH_TICKS = 12;        // laenger = kein WS2812-Bit

rmt_channel_handle_t channel = nullptr;
rmt_receive_config_t rxConfig = {};
rmt_symbol_word_t chunk[CHUNK_SYMBOLS];

// Werden im Interrupt beschrieben. Nach "is_last" ruht der Empfang, bis loop() ihn
// neu startet; deshalb greifen Interrupt und loop() nie gleichzeitig auf die Daten zu.
uint8_t rawBytes[MAX_IN_LEDS * 3];
volatile size_t bitCount = 0;
volatile bool frameBad = false;
volatile bool frameDone = false;

CRGB inFrame[MAX_IN_LEDS];
uint16_t inCount = 0;
size_t lastLength = 0;
uint8_t stableFrames = 0;
uint32_t lastFrameMs = 0;
bool started = false;

bool onReceive(rmt_channel_handle_t, const rmt_rx_done_event_data_t *e, void *) {
  for (size_t k = 0; k < e->num_symbols; k++) {
    const rmt_symbol_word_t &s = e->received_symbols[k];
    uint16_t high = s.duration0;
    if (!s.level0 || high < MIN_HIGH_TICKS || high > MAX_HIGH_TICKS) {
      frameBad = true;
      continue;
    }
    size_t bc = bitCount;
    if (bc >= sizeof(rawBytes) * 8) {
      frameBad = true;
      continue;
    }
    uint8_t mask = 0x80 >> (bc & 7);
    if ((bc & 7) == 0) rawBytes[bc >> 3] = 0;
    if (high >= ONE_THRESHOLD_TICKS) rawBytes[bc >> 3] |= mask;
    bitCount = bc + 1;
  }
  if (e->flags.is_last) frameDone = true;
  return false;
}

void startReceive() {
  bitCount = 0;
  frameBad = false;
  frameDone = false;
  if (rmt_receive(channel, chunk, sizeof(chunk), &rxConfig) != ESP_OK) started = false;
}
}  // namespace

bool begin() {
  pinMode(ARGB_IN_PIN, INPUT_PULLDOWN);  // kein Kabel = sauber low

  rmt_rx_channel_config_t cfg = {};
  cfg.gpio_num = (gpio_num_t)ARGB_IN_PIN;
  cfg.clk_src = RMT_CLK_SRC_DEFAULT;
  cfg.resolution_hz = RESOLUTION_HZ;
  cfg.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;
  if (rmt_new_rx_channel(&cfg, &channel) != ESP_OK) {
    Serial.println("ARGB-Eingang: kein RMT-Kanal frei");
    return false;
  }
  rmt_rx_event_callbacks_t cbs = {};
  cbs.on_recv_done = onReceive;
  rmt_rx_register_event_callbacks(channel, &cbs, nullptr);
  rmt_enable(channel);

  rxConfig.signal_range_min_ns = 100;    // kuerzere Spitzen gelten als Stoerung
  rxConfig.signal_range_max_ns = 30000;  // 30 us low = Bildende (WS2812-Reset)
  rxConfig.flags.en_partial_rx = 1;      // ein Bild ist laenger als der Hardwarepuffer

  started = true;
  startReceive();
  Serial.printf("ARGB-Eingang aktiv an GPIO%d\n", ARGB_IN_PIN);
  return started;
}

void loop(bool enabled) {
  if (!started || !frameDone) return;

  size_t bits = bitCount;
  bool ok = !frameBad && bits >= 24 && bits % 24 == 0;
  size_t count = bits / 24;
  // Wurde mitten in einem Bild neu gestartet, ist die Laenge falsch. Erst wenn zwei
  // Bilder nacheinander gleich lang sind, gilt das Signal als gueltig.
  if (ok) {
    if (count == lastLength) {
      if (stableFrames < 255) stableFrames++;
    } else {
      lastLength = count;
      stableFrames = 0;
    }
  }
  if (ok && stableFrames >= 1 && enabled) {
    for (size_t i = 0; i < count; i++) {
      const uint8_t *p = rawBytes + i * 3;
      inFrame[i] = CRGB(p[ARGB_IN_ORDER_R], p[ARGB_IN_ORDER_G], p[ARGB_IN_ORDER_B]);
    }
    inCount = count;
    lastFrameMs = millis();
  }
  if (!enabled) lastFrameMs = 0;
  startReceive();
}

bool active() { return lastFrameMs != 0 && millis() - lastFrameMs < ARGB_TIMEOUT_MS; }

uint16_t detectedLeds() { return active() ? inCount : 0; }

void copyTo(CRGB *leds, uint16_t n) {
  if (inCount == 0) return;
  for (uint16_t i = 0; i < n; i++) leds[i] = inFrame[(uint32_t)i * inCount / n];
}

}  // namespace argb
