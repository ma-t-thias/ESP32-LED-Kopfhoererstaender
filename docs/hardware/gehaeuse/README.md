# Gehäuse für das ESP32-Board im Kopfhörerständer

Angepasster Grundkörper (`Base_Box`) für den [RGB Headphone Stand von FaisLeFr](https://www.thingiverse.com/thing:4722406). Vorne in der V-Kerbe zwischen den beiden Armen sitzt ein eigenes Gehäuse für das Board. Das Display zeigt nach vorne, unter die Kopfhörerauflage, und ist 20° nach hinten geneigt.

Aktuelles Board ist das **Waveshare ESP32-C6-Touch-LCD-1.9** (1,9", 170 × 320, Touch). Für das defekte ESP32-C6-LCD-1.3 liegt das alte Gehäuse im Archiv [`c6-lcd-1.3/`](#archiv-esp32-c6-lcd-13).

Für das C6-Touch-LCD-1.9 gibt es zwei Varianten, **hochkant** und **quer**. Welche es wird, entscheidet ein Probedruck.

| Hochkant | Quer |
|---|---|
| <img src="c6-touch-1.9/hochkant/bilder/hochkant_iso.png" alt="Hochkant, isometrisch schräg von vorne" width="400"> | <img src="c6-touch-1.9/quer/bilder/quer_iso.png" alt="Quer, isometrisch schräg von vorne" width="400"> |
| <img src="c6-touch-1.9/hochkant/bilder/hochkant_front.png" alt="Hochkant, von vorne ca. 25° von oben" width="400"> | <img src="c6-touch-1.9/quer/bilder/quer_front.png" alt="Quer, von vorne ca. 25° von oben" width="400"> |

<p align="center">
  <img src="c6-touch-1.9/vergleich/vergleich_nah.png" alt="Nahansicht: links hochkant, rechts quer" width="800">
</p>

## Board-Daten

Waveshare liefert auf der [Seite zum ESP32-C6-LCD-1.9](https://docs.waveshare.com/ESP32-C6-LCD-1.9/Resources-And-Documents) ein STEP und eine Maßzeichnung, die beide „ESP32-S3-Touch-LCD-1.9“ heißen. Die Bauteile im STEP gehören zur S3-Variante, die Mechanik ist aber für C6 und S3 gleich. Für das Gehäuse zählt nur die Mechanik:

| Merkmal | Maß |
|---|---|
| Rahmen (Displaymodul) | 55,0 × 31,4 mm |
| Glas | 52,36 × 28,80 mm |
| Aktive Fläche | 43,12 × 23,10 mm, 1,95 mm von der USB-Seite weg versetzt |
| Platine | 52,6 × 29,0 × 1,2 mm |
| Bauhöhe | 7,8 mm bis Platinenrückseite, 11,8 mm bis zu den M2-Bolzen (4 mm), 17,35 mm bis zu den Pinspitzen |
| M2-Bolzen | 4 Stück, Abstand 40,75 × 25,0 mm |
| Stiftleisten | 2 × 20 Pins, Raster 2,54 mm, Reihenabstand 17,78 mm, eingelötet, Pins nach hinten |
| USB-C | mittig an einer kurzen Seite (31,4 mm), auf der Rückseite |

## Varianten

| | Hochkant | Quer |
|---|---|---|
| Ausrichtung | lange Seite nach oben | lange Seite waagrecht |
| Breite Gehäuse | 36,0 mm | 59,6 mm |
| Höhe über Tisch (mit Deckel) | 64 mm (66 mm) | 42 mm (44 mm) |
| Überstand über die Armspitzen | 2,3 mm | 11,3 mm |
| USB-C | oben durch den Deckel | seitlich durch die Seitenwand (von vorne gesehen rechts) |
| USB-Kabel | gerader Stecker | **90°-Winkelstecker nötig**, ein gerader Stecker stößt nach ca. 22 mm an den Arm |
| Halt des Boards | zwei Seitenleisten hinter den M2-Bolzen | Leiste unten hinter den Bolzen, zwei Finger am Deckel oben |
| Grundkörper | 59,7 cm³ | 71,1 cm³ |

**Hochkant:** schmal, sitzt sauber in der V-Kerbe, USB-Kabel oben und gerade, kaum Überstand. Dafür deutlich höher.

**Quer:** flach, wirkt wie eine Statusleiste. Dafür 11,3 mm Überstand, mehr Material, Winkelstecker nötig, USB-Kabel seitlich sichtbar. Ohne Überstand ginge es nur mit einer dreieckigen Aussparung in `1_Base` und `Diffuser_base` an beiden V-Kanten (bis ca. 9 mm tief, von x ±21 bis ±30 mm). Diese Lösung ist nicht modelliert.

<p align="center">
  <img src="c6-touch-1.9/vergleich/vergleich_front.png" alt="Vergleich von vorne" width="400">
  <img src="c6-touch-1.9/vergleich/vergleich_seite.png" alt="Vergleich von der Seite" width="400">
</p>

## Gemeinsame Merkmale

- **Einbau von oben:** Das Board wird von oben eingeschoben, der Deckel wird aufgesteckt.
- **Touch von vorne:** Die Frontlippe ist nur 1,2 mm dick, das Glas liegt 1,3 mm hinter der Gehäusefront. Vom Fensterrand bis zur aktiven Fläche sind es mindestens 3,2 mm, ein Finger erreicht die ganze Fläche. Fenster mit 0,8-mm-Fase.
- **Halteleisten:** Sie liegen 0,1 mm hinter den Spitzen der M2-Bolzen und halten das Board gegen das Fenster. Zu den Stiftleisten bleiben mindestens 0,5 mm.
- **Kabelraum:** 5,5 mm hinter den Pinspitzen für gelötete Litzen (5V, GND, GPIO1, GPIO2). Ein Trichter über die ganze Taschenbreite führt in den Kanal (8 × 7 mm) ins Innere des Grundkörpers.
- **USB-C-Öffnung:** 12,5 × 7,5 mm mit R1,5, ausgelegt auf den Steckerkörper.
- **Vertiefung für die Diffuser_box:** Die drei Diffusorscheiben liegen in einer 0,5 mm tiefen Vertiefung im Boden (Spiel ca. 0,25 mm, Restboden 1,0 mm).
- **Optionale Antennen-Durchführung:** Das Feature `Antenna_Feedthrough` setzt ein Loch mit 6 mm Durchmesser in die Rückwand über den Deckplatten, für eine externe U.FL-Antenne (beim Touch-Board per Lötbrücke umschaltbar). Es ist in beiden Varianten unterdrückt, die STL-Dateien haben das Loch also nicht. In SolidCraft lässt es sich einschalten, danach STL neu exportieren.

## Dateien

```
c6-touch-1.9/
  hochkant/drucken/   Base_Box_ESP32.stl, ESP32_Lid.stl, RGB Headphone Stand - ESP32.3mf
  hochkant/cad/       SolidCraft-Modell (ESP32.sc) und Baugruppe (.sc/.3mf)
  hochkant/bilder/    Ansichten und Schnitte
  quer/drucken/       Base_Box_ESP32_quer.stl, ESP32_Lid_quer.stl, ESP32_quer.3mf
  quer/cad/           ESP32_quer.sc, Baugruppe_quer.sc/.3mf
  quer/bilder/        Ansichten
  vergleich/          beide Varianten nebeneinander (links hochkant, rechts quer)
c6-lcd-1.3/           Archiv des Gehäuses für das ESP32-C6-LCD-1.3
```

Die CAD-Dateien enthalten kein Board-Modell. Das STEP gibt es auf der Waveshare-Seite (siehe oben). Die Baugruppen sind nur zur Ansicht gedacht.

## Drucken

- Alles ist ohne Stützmaterial druckbar.
- **Grundkörper:** wie das Original mit dem Boden nach unten. Das Displaygehäuse ist oben offen. Die Fensteroberkante und die Decke des Trichters sind Brücken zwischen festen Wänden (bis 55 mm bei quer).
- **Deckel:** In Bambu Studio mit „Auf Fläche legen" auf die Oberseite legen.
- **Material:** Kein metall- oder carbonhaltiges Filament verwenden, sonst wird die WLAN-Antenne abgeschirmt.

## Montage

1. Litzen an 5V, GND, GPIO1 und GPIO2 anlöten.
2. Die Litzen durch den Trichter und den Kanal ins Innere des Grundkörpers führen. Nicht direkt über den Antennenbereich der Platine legen.
3. Das Board mit dem Display nach vorne von oben einschieben, bis es unten aufliegt. Die M2-Bolzen gleiten dabei vor den Halteleisten vorbei.
4. Den Deckel aufstecken. TF-Karte und die BOOT- und RESET-Taster sind nur bei abgenommenem Deckel und herausgezogenem Board erreichbar.

## Benötigte Originalteile und Lizenz

Alle anderen Teile stammen unverändert aus dem [RGB Headphone Stand (Thingiverse 4722406)](https://www.thingiverse.com/thing:4722406) von **FaisLeFr**, einem Remix des Designs von NoycePrints. Lizenz: **Creative Commons – Attribution (CC BY)**. `Base_Box_ESP32` ist eine Abwandlung der dortigen `Base_Box`.

Aus dem Original gebraucht werden: 1_Base, Diffuser_base, Diffuser_box, 2 × Sides (eine Seite gespiegelt), Side_Diffuser LEFT und RIGHT, Front_Cover, Back_Cover, Headpone_post / mount, Cable_hook sowie die M3-Schrauben laut Original-Anleitung. Die `Base_Box` des Originals wird durch `Base_Box_ESP32` ersetzt.

## Offene Punkte

- **Antennenlage am echten Board prüfen.** Die C6-Platine ist anders bestückt als das S3-STEP. Wo die Keramikantenne sitzt, ist noch unbekannt.
- **Probedruck beider Varianten** (Passung von Board, Deckel und USB-Stecker).
- **Entscheidung hochkant oder quer** nach dem Probedruck.
- Der Grundkörper ist aus der Original-STL nachgebaut. Die Mulde für den alten Controller entfällt, die Wände sind leicht facettiert (ca. 0,05 mm).

## Archiv: ESP32-C6-LCD-1.3

Das erste Gehäuse war für das Waveshare ESP32-C6-LCD-1.3 (1,3", 240 × 240) gebaut. Das Board ist defekt, das Display ist gebrochen. Die Dateien liegen unverändert in [`c6-lcd-1.3/`](c6-lcd-1.3/): Hochkant-Einbau mit Fenster 24,5 × 24,5 mm, Halteleisten an den M2-Bolzen, Kabelraum und Deckel mit USB-C-Öffnung.
