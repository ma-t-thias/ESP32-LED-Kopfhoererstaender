# CLAUDE.md – Regeln für KI-Sitzungen in diesem Repo

LED-Kopfhörerständer von Matthias Steis: Ein Waveshare **ESP32-C6-Touch-LCD-1.9** steuert einen WS2812B-Streifen per WLAN, Touch, Matter/Apple Home und PC-Sync. Dazu gehören ein selbst konstruierter Ständer (SolidCraft), eine Sockel-Platine und eine PC-Simulation mit Blender. Ausführliche Doku: `README.md`, `docs/`.

## Zusammenarbeit
- **Sprache:** Deutsch, in kurzen, verständlichen Sätzen. Matthias ist Git-Einsteiger.
- **Wie ein Senior-Entwickler mit Team:** Jede nennenswerte Änderung (Firmware, Web-UI, Platine, Schaltplan) vor dem PR von einem zweiten Agenten prüfen lassen und die Befunde beheben. Im PR-Text nennen, was die Prüfung fand.
- **Keep it simple:** nur das Nötige, keine Extra-Features ohne Nachfrage, kleine eigene Treiber statt großer Bibliotheken.
- **Doku im selben PR:** README, `docs/` und Schaltplan mitziehen. Im PR-Text einen eigenen Abschnitt „Doku-Änderungen“ anlegen.
- **PRs immer als Entwurf** anlegen. Bei jedem PR einen kurzen Git-Tipp für den nächsten Klick geben (Entwurf → „Ready for review“ → „Merge pull request“ → in Fork auf main „Pull“ klicken).
- **Weboberfläche** (`web/index.html`) nach den Apple Human Interface Guidelines: Systemschrift, Systemfarben, 44-pt-Trefferflächen, Dark Mode, reduzierte Bewegung.

## Harte Regeln
- Nie committen: `include/secrets.h`, `test_ota.ini` (Vorlage: `test_ota.example.ini`). Ebenfalls gitignored und nie von Hand ändern: `include/fw_version.h` (erzeugt `tools/version.py` bei jedem Build), `update/`, `*.bin`, `*.blend`.
- Keine Tags pushen, keine Branches löschen, kein Force-Push ohne ausdrückliches OK von Matthias.
- Update-Dateien für das Web-Update (`update/LED-Kopfhoerer_<LCD19|LCD13>_<Tag>.bin`) nur aus getaggten, sauberen Ständen: `tools/update_file.py`, Aufgabe „Update-Datei erzeugen“ bzw. `pio run -e esp32-c6-lcd -t update_file`. Diese Regel nicht aufweichen.
- **Auf Matthias' PC** (Remote Control):
  - Der Hauptordner `K:\Fork-GitHub\ESP32-LED-Kopfhoererstaender` bleibt auf `main`.
  - Gebaut wird nur in kurzlebigen Worktrees.
  - Nichts löschen, stashen oder umstellen ohne sein OK.
  - Flashen nur, wenn er „flashen“ schreibt.
  - Ein Push braucht seine Freigabe direkt in der Sitzung.
- Teile bestellt Matthias selbst. Bezugsquellen nur vorschlagen, ungeprüfte Preise als ungeprüft kennzeichnen.

## Aufbau
| Pfad | Inhalt |
|---|---|
| `src/`, `include/config.h` | Firmware (PlatformIO, Arduino-Core 3 über pioarduino). Pins und Schwellen in `config.h`. |
| `web/index.html` | Quelle der Weboberfläche → `python tools/web2h.py` erzeugt `src/webui_index.h` (beide committen) |
| `tools/version.py`, `tools/update_file.py` | Versionskennung aus `git describe`, Update-Datei nur für Tags |
| `tools/sim/`, `tools/blender/` | PC-Simulation (`env:sim`) und Blender-Szene, Anleitung `docs/simulation.md`. STLs in `tools/blender/stl/` sind eingecheckt, die `.blend` entsteht lokal. |
| `docs/hardware/schaltung.html` | Schaltplan, Breadboard, Bauteile. Erzeugt mit `python3 docs/hardware/gen_schaltung.py` aus `schaltung_template.html`, nie von Hand ändern. |
| `docs/hardware/platine/`, `platine-v2/` | Sockel-Platinen. `gen_platine.py` bzw. `gen_platine_v2.py` erzeugen Gerber, Bestückung und Ätzvorlage in `out/` (brauchen `shapely`, `pillow`). |
| `docs/hardware/gehaeuse/` | Druckteile und CAD-Stände |

## Bauen
- Firmware 1.9er: `pio run -e esp32-c6-lcd`. Altes Board: `pio run -e esp32-c6-lcd-13`.
- Simulation: in VS Code unten `env:sim` wählen, dann Project Tasks → Custom → „Simulation in Blender starten“ bzw. „Blender-Szene neu bauen“ (Kommandozeile: `pio run -e sim -t blender` / `-t blender_szene`).
- Nach einer Firmware-Änderung beide Boards bauen, nach Änderungen an Effekten, Logo oder Sensor-API auch `sim`.
- `env:esp32-c6-lcd-13` ersetzt `build_flags` (ohne `-DWITH_MATTER`, mit `-DBOARD_LCD_13`, dort LED GPIO1, ARGB GPIO2). Neue Build-Flags in beide Envs eintragen. In der Cloud ist die PlatformIO-Registry gesperrt: Bibliotheken dort als GitHub-URL in einer Testkopie einbinden, `platformio.ini` im Repo nicht ändern.

## Hardware-Kurzfakten (1.9er)
- **Pins:**
  - LED-Daten: GPIO3 (J3/7), über 74AHCT125 und 330 Ω
  - ARGB-Eingang: GPIO23 (J4/9), Teiler 1k/2k
  - Kopfhörer-Sensor FSR: GPIO0 (J3/4), 22 kΩ nach GND
  - Mikrofon INMP441: SCK 19, WS 20, SD 21 (19/20 sind sonst die SD-Karte, also keine Karte stecken)
  - Nicht benutzen: GPIO1/2 (IMU-Interrupts), 8/18 (I²C Touch + IMU), 9 (BOOT), 12/13 (USB, nur PC-Sync), 14/15 (LCD), 16/17 (UART0)
- **Versorgung:**
  - USB-C 5 V / 3 A, Strombegrenzung 2400 mA
  - ESP-VBUS nur über Schottky D1
  - F1 = Littelfuse RGEF400 (nicht RXEF375)
- **Partitionen:** `partitions_8mb.csv` (1.9er) und `partitions.csv` (1.3er). In beiden müssen otadata bei 0xE000, fctry bei 0x9000 und nvs bei 0x3D0000 bleiben, sonst gehen WLAN-Daten, Einstellungen oder Seriennummer verloren. Die Partitionstabelle wird nur per USB geschrieben.

## Ständer-Konstruktion (SolidCraft, nur auf Matthias' PC)
- Diffusoren sind separate Inlays aus Fluor-PLA. Tragende Teile nie aus Diffusor-Material.
- Gerundete Formen. Nichts kleben: clipsen oder schieben mit Arretierung.
- Alles einfarbig druckbar. TPU-Füße sind erlaubt. Der ESP muss von oben montierbar bleiben.
- Jede Änderung danach in SolidCraft anzeigen.
- Durchbruch-Schnitte 0,2 mm in die Wand legen, dann per Raycast prüfen.

## Lizenz
Eigenes Projekt: CC BY-NC-SA 4.0, „(C) 2026 Matthias Steis“ (siehe `LICENSE.md`). Der alte Ständer (Thingiverse 4722406 von FaisLeFr, Remix von NoycePrints 3534573) ist CC BY-NC: Abgeleitete Druckteile, auch die bisherigen Gehäuse unter `docs/hardware/gehaeuse/c6-*`, dürfen ohne Erlaubnis nicht kommerziell genutzt werden. Deshalb ist Ständer v2 eine eigene Neukonstruktion.
