// Minimaler Arduino-Ersatz fuer die PC-Simulation (env:sim). Nur das, was effects.cpp,
// state.h und FastLED auf dem PC brauchen; auf dem ESP wird diese Datei nie benutzt.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

using std::max;
using std::min;
typedef std::string String;

// FastLED kennt Pin-Klassen; auf dem PC gibt es keine Pins.
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
inline void pinMode(int, int) {}
inline int digitalRead(int) { return LOW; }
inline void digitalWrite(int, int) {}
