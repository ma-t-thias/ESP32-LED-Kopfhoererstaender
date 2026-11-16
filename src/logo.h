#pragma once
#include <Arduino_GFX.h>  // nur der Zeichenteil, ohne Display-Treiber

// Startbild (Kopfhoerer mit Regenbogen-Buegel, Name, Untertitel, Copyright). Eigene Datei,
// damit auch die PC-Simulation (tools/sim, env:sim) genau dieses Bild erzeugen kann.
namespace logo {

// Lage von Update-Anzeige und Warte-Symbol, passend zum gezeichneten Bild
struct Layout {
  int rowY, rowCx;          // Zeile fuer "Update 42 %"
  int barX, barY, barW;     // Fortschrittsbalken
  int spinX, spinY, spinR;  // Warte-Symbol in der Kopfhoerer-Grafik
};

// Zeichnet das ganze Bild in der aktuellen Drehung von g. sub = kleine Zeile unter dem Namen.
Layout draw(Arduino_GFX *g, const char *sub);

// Regenbogenfarbe 0..255 im Display-Format RGB565
uint16_t hue565(uint8_t h);

}  // namespace logo
