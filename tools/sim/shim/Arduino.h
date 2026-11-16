// Minimaler Arduino-Ersatz fuer die PC-Simulation (env:sim). Nur das, was effects.cpp,
// logo.cpp, state.h, FastLED und der Zeichenteil von Arduino_GFX auf dem PC brauchen;
// auf dem ESP wird diese Datei nie benutzt.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

using std::max;
using std::min;

class String : public std::string {
 public:
  using std::string::string;
  String() {}
  String(const std::string &s) : std::string(s) {}
  int indexOf(char c) const {
    size_type p = find(c);
    return p == npos ? -1 : int(p);
  }
};

// Flash-Zugriffe: auf dem PC ganz normaler Speicher
class __FlashStringHelper;
#define F(s) ((const __FlashStringHelper *)(s))
#define PROGMEM
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define pgm_read_dword(a) (*(const uint32_t *)(a))
#ifndef PI
#define PI 3.14159265358979323846
#endif
#define DEG_TO_RAD 0.017453292519943295

// Pins gibt es auf dem PC nicht
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
inline void pinMode(int, int) {}
inline int digitalRead(int) { return LOW; }
inline void digitalWrite(int, int) {}
inline void delay(unsigned long) {}
