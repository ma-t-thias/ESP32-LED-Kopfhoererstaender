# PC-Simulation der LED-Effekte (`env:sim`)

Rechnet die Effekte auf dem PC, ohne ESP und ohne LEDs. Dafür wird dieselbe `src/effects.cpp` wie in der Firmware übersetzt (mit FastLED im PC-Modus). Was hier zu sehen ist, macht der Ständer genauso.

Das Programm gibt etwa 30-mal pro Sekunde eine Zeile aus: je LED sechs Hex-Zeichen `RRGGBB`, im selben Format wie `/api/live` am Ständer. Angezeigt wird das mit der Blender-Szene (siehe [`../blender/README.md`](../blender/README.md)).

## Bauen

1. Einmalig einen C++-Compiler installieren, falls keiner da ist. Unter Windows zum Beispiel [w64devkit](https://github.com/skeeto/w64devkit) (entpacken, den Ordner `bin` in den PATH aufnehmen) oder MSYS2 mit `mingw-w64-ucrt-x86_64-gcc`. Prüfen im Terminal: `g++ --version`.
2. In VS Code unten in der Statusleiste die Umgebung **env:sim** wählen und **Build** drücken (oder im Terminal `pio run -e sim`).
3. Das Programm liegt danach unter `.pio/build/sim/program.exe` (Linux/macOS: `program`).

Die Firmware-Umgebungen sind davon nicht betroffen, `env:sim` baut nur `effects.cpp` und die Dateien in diesem Ordner.

## Benutzen

Direkt im Terminal (zum Ausprobieren):

```
.pio/build/sim/program effect 5 speed 200
```

Befehle über die Eingabe, einer pro Zeile:

| Befehl | Wirkung |
|---|---|
| `effect 24` | Effekt-Nummer wie in der Weboberfläche (0 = Einfarbig, 24 = Musik) |
| `palette 3` | Palette |
| `speed 128`, `intensity 90` | Tempo, Effekt-Stärke (0–255) |
| `color1 ff7800`, `color2 0050ff` | Farben für Einfarbig, Atmen, Komet … |
| `list` | Namen aller Effekte und Paletten (Zeilen mit `#`) |
| `quit` | beenden |

## Dateien

| Datei | Inhalt |
|---|---|
| `ledsim.cpp` | Hauptprogramm: Bildtakt 60/s wie in der Firmware, Befehle, Ausgabe |
| `sensors_sim.cpp` | Ersatz für die Sensoren: kein Kopfhörer-Sensor, das Mikrofon liefert einen künstlichen Takt (120 BPM) für den Effekt „Musik“ |
| `shim/Arduino.h` | das Nötigste aus Arduino für den PC |

Grenzen: Nur die Effekte selbst, ohne Helligkeit (wie `/api/live`). PC-Sync, ARGB-Eingang, Nachtmodus, Stromgrenze und Ein-/Ausblenden aus `main.cpp` laufen nicht mit.
