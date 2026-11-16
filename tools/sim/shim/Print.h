// Print-Ersatz fuer die PC-Simulation (Textausgabe von Arduino_GFX)
#pragma once
#include "Arduino.h"
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *b, size_t n) { size_t k = 0; while (n--) k += write(*b++); return k; }
  size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
  size_t print(const char *s) { return write(s); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write(uint8_t(c)); }
  size_t print(int v) { return print(String(std::to_string(v))); }
  size_t println(const char *s = "") { return print(s) + write("\n"); }
};
