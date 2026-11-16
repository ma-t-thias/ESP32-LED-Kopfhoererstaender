// Display in der PC-Simulation: zeichnet das Startbild mit derselben src/logo.cpp wie die
// Firmware in einen Bildspeicher und speichert es als PNG (fuer die Blender-Szene).
#include <cstdio>
#include <vector>

#include "config.h"
#include "logo.h"

namespace {

// Arduino_GFX, das in den Arbeitsspeicher zeichnet. Arduino_GFX rechnet die Drehung selbst
// in Breite/Hoehe um; die Pixel kommen hier schon in Bildkoordinaten an.
class MemGFX : public Arduino_GFX {
 public:
  MemGFX(int16_t w, int16_t h) : Arduino_GFX(w, h), pix(size_t(w) * h, 0) {}
  bool begin(int32_t = GFX_NOT_DEFINED) override { return true; }
  void writePixelPreclipped(int16_t x, int16_t y, uint16_t c) override { pix[size_t(y) * width() + x] = c; }
  void setRotation(uint8_t r) override {
    Arduino_GFX::setRotation(r);
    pix.assign(size_t(width()) * height(), 0);
  }
  std::vector<uint16_t> pix;
};

uint32_t crcTable[256];
uint32_t crc(const uint8_t *d, size_t n, uint32_t c = 0xFFFFFFFFu) {
  if (!crcTable[1])
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t v = i;
      for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
      crcTable[i] = v;
    }
  for (size_t i = 0; i < n; i++) c = crcTable[(c ^ d[i]) & 255] ^ (c >> 8);
  return c;
}
void be32(std::vector<uint8_t> &o, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) o.push_back(uint8_t(v >> s));
}
void chunk(std::vector<uint8_t> &png, const char *type, const std::vector<uint8_t> &data) {
  be32(png, uint32_t(data.size()));
  std::vector<uint8_t> td(type, type + 4);
  td.insert(td.end(), data.begin(), data.end());
  png.insert(png.end(), td.begin(), td.end());
  be32(png, crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
}

// Einfaches PNG ohne Kompression (zlib "stored"), RGB 8 Bit
bool writePng(const char *file, int w, int h, const std::vector<uint16_t> &pix) {
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; y++) {
    raw.push_back(0);  // Filter "keiner"
    for (int x = 0; x < w; x++) {
      uint16_t c = pix[size_t(y) * w + x];
      uint8_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      raw.push_back(uint8_t((r << 3) | (r >> 2)));
      raw.push_back(uint8_t((g << 2) | (g >> 4)));
      raw.push_back(uint8_t((b << 3) | (b >> 2)));
    }
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  for (size_t i = 0; i < raw.size(); i += 65535) {
    size_t n = std::min<size_t>(65535, raw.size() - i);
    z.push_back(i + n == raw.size());
    z.push_back(uint8_t(n));
    z.push_back(uint8_t(n >> 8));
    z.push_back(uint8_t(~n));
    z.push_back(uint8_t(~n >> 8));
    z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
  }
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  be32(z, (b << 16) | a);
  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'}, ihdr;
  be32(ihdr, w);
  be32(ihdr, h);
  ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});  // 8 Bit, RGB
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", {});
  FILE *f = fopen(file, "wb");
  if (!f) return false;
  bool ok = fwrite(png.data(), 1, png.size(), f) == png.size();
  return fclose(f) == 0 && ok;
}

}  // namespace

// Startbild als PNG speichern. rot = Drehung wie in der Firmware (0/2 hochkant, 1/3 quer).
bool writeLogoPng(const char *file, uint8_t rot, const char *sub) {
  MemGFX g(LCD_W, LCD_H);
  g.begin();
  g.setRotation(rot);
  logo::draw(&g, sub);
  return writePng(file, g.width(), g.height(), g.pix);
}
