# Platine v2 für den Ständer v2 (mit Kopfhörer-Sensor und Mikrofon)

Status: **Entwurf, noch nicht bestellt und nicht getestet.** Die Firmware kennt Sensor und Mikrofon (Einstellungen → Kopfhörer, Effekt „Musik“), am echten Aufbau ist beides noch ungetestet.

Die Platine v2 sitzt im neuen ovalen Sockel des Ständers v2 (siehe [`../gehaeuse/staender-v2/`](../gehaeuse/staender-v2/)). Dort ist deutlich mehr Platz als im alten Sockel, deshalb ist sie ein schlichtes Rechteck mit 4 Befestigungslöchern. Gegenüber der [Platine v1](../platine/) kommt Folgendes dazu:

- **Kopfhörer-Sensor:** Anschluss für eine FSR-Drucksensor-Folie (FSR402 o. ä.) unter der Druckplatte in der Kopfhörer-Auflage. Die Folie liegt zwischen 3,3 V und GPIO0, R5 (22 kΩ) zieht nach GND, C3 (100 nF) glättet. Ohne Kopfhörer liegt GPIO0 bei ca. 0 V, mit Kopfhörer je nach Gewicht bei ca. 1–3 V.
- **Mikrofon:** Steckplatz für ein rundes I2S-Mikrofonmodul INMP441 (Ø 14 mm, 2 × 3 Pins) für die Musik-Reaktion. SCK = GPIO19, WS = GPIO20, SD = GPIO21, L/R auf GND (linker Kanal).
- **Schutz am Eingang:** Polyfuse F1 (4 A Haltestrom) und TVS-Diode D2 (5 V) direkt hinter dem USB-C-Eingang.
- **Ein Anschluss zum ESP:** Alle Leitungen zum ESP laufen über eine 10-polige Stiftleiste J7 (ein Kabel statt mehrerer).
- **USB-Daten nicht mehr über die Platine:** Für PC-Sync führt das Datenkabel jetzt direkt von der USB-C-Buchsenplatine (Pins 5–6) zum ESP (J3 Pin 12 = GPIO13 = D+, J3 Pin 11 = GPIO12 = D−). Es bleibt der Schalter: gesteckt PC-Sync, abgezogen nur Strom.

![Bestückungsplan](out/platine_v2_bestueckung.png)

Ansicht von der Bauteilseite. Gelb ist die Massefläche (GND), die farbigen Bahnen sind die übrigen Netze (Kupfer unten). Die hellblau gestrichelten Linien liegen auf der Bauteilseite: 3V3 zum Mikrofon und eine kurze Masse-Brücke von L/R zu GND am Mikrofon.

## Maße und Einbau

- Größe **64 × 52 mm**, Ecken mit 2 mm gerundet. Koordinaten wie im SolidCraft-Modell: Ursprung = Mitte des Sockels, +y zeigt zum Display.
- Umriss x −32…32, y −22…30.
- 4 Befestigungslöcher Ø 2,7 mm für M2,5 bei (−29/−19), (29/−19), (−29/21), (29/21). Die Dome im Sockel-Boden müssen an diese Stellen.
- Bis zur Sockeldecke sind über der Platine ca. 17 mm frei. Die höchsten Bauteile sind F1 (RGEF400, Körper ca. 13 mm, mit Beinchen-Knick ca. 15 mm) und D2 (stehend, ca. 12 mm). Wird es zu eng, F1 vor dem Einlöten flach umlegen (die Beinchen sind lang genug). C1 liegt.
- Das Mikrofonmodul liegt flach auf zwei kurzen 1×3-Stiftleisten im markierten Kreis vorne rechts. Die Mitte (= Schallloch) liegt bei x 17,04 / y 22,69. Der Mikrofonchip zeigt nach unten zur Platine, die Beschriftung mit dem Schallloch nach oben. Genau darüber braucht der Sockel eine Schallöffnung. Zum Schraubenkopf bei (29/21) bleiben ca. 2,5 mm.
- **J7 nicht als gerade Stiftleiste mit Dupont-Stecker** (zu hoch): 90°-Stiftleiste nach vorne oder Litze direkt einlöten, am ESP-Ende Dupont-Buchsen.
- J4 (ARGB, 90°-Stiftleiste) zeigt an der rechten Kante nach außen.

## Dateien

| Datei | Zweck |
|---|---|
| `out/platine_v2_gerber.zip` | **Fertigungsdaten zum Hochladen beim Leiterplatten-Shop** (Gerber + Bohrdateien) |
| `out/platine_v2_bestueckung.png` | Bestückungsplan |
| `out/platine_v2_aetzvorlage_A4.pdf` | Ätzvorlage 1:1, zwei Kopien auf A4. Mit 100 % drucken. |
| `out/platine_v2_aetzvorlage_600dpi.png` | Dieselbe Vorlage als Bild |
| `gen_platine_v2.py` | Erzeugt alles neu: `pip install shapely pillow`, dann `python3 docs/hardware/platine-v2/gen_platine_v2.py` |

Das Skript prüft beim Erzeugen die Abstände zwischen den Netzen und zur Massefläche (mindestens 0,45 mm), zum Rand und zu den Befestigungslöchern, dass jedes Netz durchgehend verbunden ist und dass die Massefläche zusammenhängt. Aktuell: keine Probleme, eine zusammenhängende Massefläche.

## Online bestellen

`out/platine_v2_gerber.zip` beim Shop hochladen (z. B. AISLER). Optionen wie bei v1: **2 Lagen, FR-4, 1,6 mm, 35 µm Kupfer, HASL bleifrei, Lötstopp grün, Bestückungsdruck weiß**. Kleinste Bohrung 0,8 mm, kleinster Abstand 0,45 mm, Bahnbreite ab 0,8 mm.

| Datei | Lage |
|---|---|
| `platine-v2-F_Cu.gtl` | Kupfer oben: Lötaugen, die 3V3-Bahn zum Mikrofon und die Brücke L/R → GND |
| `platine-v2-B_Cu.gbl` | Kupfer unten: Leiterbahnen und Massefläche |
| `platine-v2-F_Mask.gts`, `platine-v2-B_Mask.gbs` | Lötstopplack oben/unten |
| `platine-v2-F_SilkS.gto` | Bestückungsdruck (Bauteilnamen, Pinbelegung, Polung, Copyright) |
| `platine-v2-Edge_Cuts.gm1` | Umriss |
| `platine-v2-PTH.drl`, `platine-v2-NPTH.drl` | Bohrungen durchkontaktiert / ohne Kupfer (4 × M2,5) |

## Selbst ätzen

Die Ätzvorlage zeigt die Unterseite von der Bauteilseite aus gesehen, also **nicht spiegeln**. Prüfmaß 64,0 × 52,0 mm. Die beiden Bahnen auf der Bauteilseite als isolierte Drähte oben einlöten: 3V3 von J7 Pin 3 zu J6 VDD und L/R zu GND am Mikrofon (sonst hängt L/R in der Luft).

## Bauteile

| Teil | Wert | Montage |
|---|---|---|
| U1 | 74AHCT125 (DIP-14) | Kerbe nach rechts, Pin 1 oben rechts (eckiges Pad). IC-Fassung passt. |
| C1 | 1000 µF / 10 V, Elko Ø 8,2 × 16,7 mm, Raster 3,5 | **liegend** links, Körper nach oben (Richtung LED-Anschluss). Plus ins eckige Pad. |
| C2 | 100 nF Keramik, Raster 5,08 | neben U1 Pin 14 |
| C3 | 100 nF Keramik, Raster 2,54 | neben R5 (Sensor-Glättung) |
| R1 | 330 Ω | stehend über U1 |
| R3 | 1 kΩ | stehend, nur für den ARGB-Eingang |
| R4 | 2 kΩ | stehend, nur für den ARGB-Eingang |
| R5 | 22 kΩ | stehend, Spannungsteiler für den FSR-Sensor |
| D1 | 1N5819 | stehend, Körper über dem runden Pad (Anode), Ring ins eckige Pad (Kathode) |
| D2 | SA5.0A (TVS, DO-15) | stehend, Körper über dem runden Pad (Anode, GND), Ring ins eckige Pad (Kathode, +5 V) |
| F1 | Polyfuse Littelfuse RGEF400 (4 A halten, 6,8 A auslösen, 16 V), radial, Raster 5,1, ca. 9 × 13 mm | beliebig herum. Der Haltestrom sinkt mit der Temperatur (bei 40 °C im Sockel 3,5 A laut Datenblatt), das reicht für 2,4 A LEDs + ESP. **Nicht die RXEF375:** gleicher Strom, aber für 72 V gebaut und mit ca. 27 × 32 mm und 10,9 mm Raster viel zu groß. Die RGEF300 wäre warm zu knapp. |
| J1 | Litze ≥ AWG 22 direkt einlöten | USB-Eingang: − = GND, + = VBUS der USB-C-Buchsenplatine (Pins 1–2 der 8er-Leiste). Kein dünnes Jumperkabel, es fließen bis ca. 2,7 A. |
| LED | Pads 1×3 | Pigtail mit JST-SM zum Streifen: +, G, D |
| J4 | Stiftleiste 1×4, 90° | ARGB vom Mainboard (5V nc, D, Lücke, G). Dritten Stift vor dem Einlöten ziehen. |
| J5 | Stiftleiste 1×2 oder Litze | FSR-Folie (Polung egal) |
| J6 | 2 × Stiftleiste 1×3 (liegen dem Modul bei) | Mikrofonmodul INMP441, flach liegend, Chip nach unten, Schallloch nach oben |
| J7 | 90°-Stiftleiste 1×10 oder Litze | Kabel zum ESP (siehe unten) |

## Anschluss ESP (J7)

| J7 Pin | Signal | ESP32-C6-Touch-LCD-1.9 |
|---|---|---|
| 1 | 5V (VESP, hinter D1) | J4 Pin 1 (VBUS) |
| 2 | GND | J4 Pin 3 (GND) |
| 3 | 3V3 | J4 Pin 5 (3V3) |
| 4 | GPIO0 (FSR) | J3 Pin 4 |
| 5 | GPIO3 (LED-Daten) | J3 Pin 7 |
| 6 | GPIO23 (ARGB-Eingang) | J4 Pin 9 |
| 7 | GPIO19 (I2S SCK) | J3 Pin 17 |
| 8 | GPIO20 (I2S WS) | J3 Pin 19 |
| 9 | GPIO21 (I2S SD) | J3 Pin 20 |
| 10 | GND | J3 Pin 18 (GND) |

## Mikrofon (J6)

Rundes INMP441-Modul (Ø 14 mm), 2 Reihen zu 3 Pins im 2,54-Raster, Reihenabstand 7,62 mm. Auf der Beschriftungsseite: oben SCK, WS, L/R, unten SD, VDD, GND. Auf der Platine ist das Modul um 180° gedreht eingesetzt, von oben gesehen:

| Reihe | links | Mitte | rechts |
|---|---|---|---|
| vorne (y 26,5) | GND (eckig) | VDD = 3V3 | SD → GPIO21 |
| hinten (y 18,88) | L/R → GND | WS → GPIO20 | SCK → GPIO19 |

Beim Einlöten auf die Beschriftung achten: GND am eckigen Pad, Beschriftung nach oben lesbar.

## Offen

- Dicke des INMP441-Moduls nachmessen, wenn es da ist (für die Höhe der Schallöffnung).
- Dome, Schallöffnung über dem Mikrofon und Kabeldurchführungen im Sockel v2 an die neue Platine anpassen.
- Firmware am echten Aufbau testen: Schwellen des FSR (`HP_ON_MV`/`HP_OFF_MV` in `config.h`) und Empfindlichkeit des Mikrofons.
