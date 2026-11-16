# Ständer in Blender (3D-Vorschau mit LEDs)

Der Ständer als 3D-Szene in Blender. Jede der 144 LEDs ist ein eigenes Leuchtobjekt an ihrer echten Stelle. Die Farben kommen live vom Ständer oder aus der PC-Simulation ([`../sim/`](../sim/README.md)). Getestet mit Blender 5.3.

## Szene bauen

Die Teile werden in SolidCraft in Montageposition als STL exportiert, mit festen Dateinamen (siehe `PARTS` in `build_staender_blend.py`), in einen Ordner `stl/`. Die STL-Dateien und die `.blend` sind nicht im Repo, nur das Skript.

```
blender --background --python tools/blender/build_staender_blend.py -- --stl-dir <Ordner mit STLs> --out Staender_v8.blend [--render Vorschau.png] [--demo]
```

Das Skript baut die Szene jedes Mal komplett neu. Bei einem neuen CAD-Stand also nur die STLs neu exportieren und das Skript erneut laufen lassen. Die Namen der Teile und LEDs bleiben gleich.

| Collection | Inhalt |
|---|---|
| Tragteile | Sockel, Boden, Säule, Auflage, Druckplatte, Display-Gehäuse, TPU-Füße |
| Inlays | Leuchtring, Augen, Säulen-Inlays (Fluor-PLA, halbtransparent) |
| Elektronik | Platine v2, ESP32-Modul, LCD, USB-C |
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

`live_leds.py` setzt diese Farben etwa 25-mal pro Sekunde:

```
blender Staender_v8.blend --python tools/blender/live_leds.py -- 192.168.1.159
blender Staender_v8.blend --python tools/blender/live_leds.py -- sim effect 5
```

- **Echter Ständer:** IP oder `led-kopfhoerer.local`. Gelesen wird `/api/live`, ohne Gerätepasswort.
- **`sim`:** startet die PC-Simulation `.pio/build/sim/program(.exe)` (vorher `env:sim` bauen). Effekt & Co. danach in Blenders Python-Konsole ändern, z. B. `led_sim("effect 24")` oder `led_sim("palette 3")`.
- Stoppen: das Skript mit `QUELLE = ""` erneut ausführen. Findet das Skript die Simulation nicht, `SIM_PFAD` oben im Skript setzen.
- Das Skript stellt die 3D-Ansicht auf Materialvorschau und blendet die Licht-Symbole aus.

Aus VS Code geht das am bequemsten mit der Erweiterung **Blender Development** (Befehle „Blender: Start“ und „Blender: Run Script“).
