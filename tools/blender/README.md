# Ständer in Blender (3D-Vorschau mit LEDs)

**Einsteiger:** Anleitung mit Bildern in [`docs/simulation.md`](../../docs/simulation.md). Am einfachsten startet alles aus VS Code: unten in der Statusleiste `env:sim` wählen, dann PlatformIO → Project Tasks → Custom → „Simulation in Blender starten“ bzw. „Blender-Szene neu bauen“ (`tools/sim/sim_targets.py`).

Der Ständer als 3D-Szene in Blender. Jede der 144 LEDs ist ein eigenes Leuchtobjekt an ihrer echten Stelle. Die Farben kommen live vom Ständer oder aus der PC-Simulation ([`../sim/`](../sim/README.md)). Getestet mit Blender 5.3.

## Szene bauen

Die Teile werden in SolidCraft in Montageposition als STL exportiert, mit festen Dateinamen (siehe `PARTS` in `build_staender_blend.py`), in einen Ordner `stl/`. Die STL-Dateien (Stand Sockel v9 / Auflage v7, 23 Teile, ca. 20 MB) liegen im Repo unter `tools/blender/stl/`, damit die Szene auf jedem PC ohne SolidCraft gebaut werden kann. Die `.blend` wird lokal erzeugt und nicht eingecheckt.

```
blender --background --python tools/blender/build_staender_blend.py -- --stl-dir <Ordner mit STLs> --out Staender_v8.blend [--render Vorschau.png] [--demo]
```

Das Display zeigt das Startbild (Kopfhörer mit Regenbogen-Bügel). Das Bild erzeugt die PC-Simulation mit demselben Zeichencode wie die Firmware (`src/logo.cpp`):

```
.pio/build/sim/program logo Display_Grundbild.png 0 "V1.0.0"
```

`0` ist die Drehung (hochkant, so sitzt das Display im Ständer), der Text die kleine Zeile unter dem Namen. Das Skript nimmt `Display_Grundbild.png` neben sich oder die Datei aus `--display-png`.

Das Skript baut die Szene jedes Mal komplett neu. Bei einem neuen CAD-Stand also nur die STLs neu exportieren und das Skript erneut laufen lassen. Die Namen der Teile und LEDs bleiben gleich.

| Collection | Inhalt |
|---|---|
| Tragteile | Sockel, Boden, Säule, Auflage, Druckplatte, Display-Gehäuse, TPU-Füße |
| Inlays | Leuchtring, Augen, Säulen-Inlays (Fluor-PLA, halbtransparent) |
| Elektronik | Platine v2, ESP32-Modul, LCD mit Startbild, USB-C |
| LEDs | `LED_000` … `LED_143` in der Reihenfolge am Datenausgang |

Maßstab: 1 Blender-Einheit = 1 mm, Koordinaten wie im CAD (Ursprung Sockelmitte, +Y zum Display).

### Verteilung der LEDs (vorläufig)

Bis der Streifen wirklich verlegt ist, gilt diese Annahme (`RING_COUNT`, `COLUMN_COUNT` im Skript):

| LEDs | Ort |
|---|---|
| 0–68 | Leuchtring im Sockel, Start hinten in der Mitte, gegen den Uhrzeigersinn (von oben) |
| 69–92 | Säule, von unten nach oben |
| 93–143 | nicht verbaut (ausgeblendet) |

## LEDs live einfärben

Jede LED bekommt ihre Farbe über die Objektfarbe (`obj.color`). Das gemeinsame Material `LED_Emission` gibt sie als Licht aus.

`live_leds.py` setzt diese Farben etwa 25-mal pro Sekunde und legt ein **Bedienfeld** an: in der 3D-Ansicht Taste **N**, Reiter **„LED“**. Dort Quelle (Simulation oder echter Ständer), Start/Stopp und bei der Simulation Effekt, Palette, Tempo, Effekt-Stärke und Farben. Die Namen holt das Skript von der Simulation, sie passen also immer zur Firmware.

Ohne Bedienfeld, im Terminal im Projektordner:

```
blender tools/blender/Staender_v8.blend --python tools/blender/live_leds.py -- sim effect 5
blender tools/blender/Staender_v8.blend --python tools/blender/live_leds.py -- 192.168.1.159
```

- **Echter Ständer:** IP oder `led-kopfhoerer.local`. Gelesen wird `/api/live`, ohne Gerätepasswort.
- **`sim`:** startet die PC-Simulation `.pio/build/sim/program(.exe)` (vorher `env:sim` bauen). In Blenders Python-Konsole geht auch `led_sim("effect 24")`.
- Findet das Skript die Simulation nicht, `SIM_PFAD` oben im Skript setzen.
- Das Skript stellt die 3D-Ansicht auf den Kamerablick mit Eevee-Vorschau („Rendered“) und blendet die Licht-Symbole aus. Nur in dieser Vorschau leuchten die LEDs auch auf die Inlays.
