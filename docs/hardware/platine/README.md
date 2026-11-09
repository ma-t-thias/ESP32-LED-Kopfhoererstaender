# Platine für den Sockel (bedrahtet, zum Bestellen oder Selbstätzen)

Die Platine ersetzt den Breadboard-Aufbau und die WAGO-Klemmen. Sie sitzt im Sockel (Base_Box) in der Mitte zwischen den drei Diffusorscheiben, mit einer Zunge Richtung ESP-Gehäuse. Die Lichtöffnungen bleiben frei.

![Bestückungsplan](out/platine_bestueckung.png)

Ansicht von der Bauteilseite. Gelb ist die Massefläche (GND), die farbigen Bahnen sind die übrigen Netze. Das Kupfer liegt unten.

## Dateien

| Datei | Zweck |
|---|---|
| `out/platine_aetzvorlage_A4.pdf` | Ätzvorlage 1:1, zwei Kopien auf A4. Mit 100 % drucken, nicht „an Seite anpassen“. |
| `out/platine_aetzvorlage_600dpi.png` | Dieselbe Vorlage als Bild |
| `out/platine_bestueckung.png` | Bestückungsplan |
| `out/platine_gerber.zip` | **Fertigungsdaten zum Hochladen beim Leiterplatten-Shop** (Gerber + Bohrdateien, siehe unten) |
| `out/platine_gerber_vorschau.png` | So sieht die bestellte Platine aus (links oben, rechts unten) |
| `gen_platine.py` | Erzeugt alles neu: `pip install shapely pillow`, dann `python3 docs/hardware/platine/gen_platine.py` |

Das Skript prüft beim Erzeugen die Abstände zwischen den Netzen (mindestens 0,45 mm), zum Rand und zu den Befestigungslöchern. Außerdem prüft es, dass alle GND-Pads an einer zusammenhängenden Massefläche hängen.

## Online bestellen

`out/platine_gerber.zip` beim Shop hochladen (z. B. AISLER, JLCPCB). Inhalt:

| Datei | Lage |
|---|---|
| `platine-F_Cu.gtl` | Kupfer oben: nur Lötaugen (damit die Bohrungen durchkontaktiert sind) |
| `platine-B_Cu.gbl` | Kupfer unten: Leiterbahnen und Massefläche |
| `platine-F_Mask.gts`, `platine-B_Mask.gbs` | Lötstopplack oben/unten |
| `platine-F_SilkS.gto` | Bestückungsdruck oben (Bauteilnamen, Umrisse, Polung) |
| `platine-Edge_Cuts.gm1` | Umriss |
| `platine-PTH.drl`, `platine-NPTH.drl` | Bohrungen durchkontaktiert / ohne Kupfer (3 × M2,5) |

Bestelloptionen: **2 Lagen, FR-4, 1,6 mm, 35 µm Kupfer, HASL bleifrei, Lötstopp grün, Bestückungsdruck weiß**. Größe 46,5 × 46,0 mm, kleinste Bohrung 0,8 mm, kleinster Abstand 0,45 mm, Bahnbreite ab 0,6 mm. Das schafft jeder Standardprozess.

![Gerber-Vorschau](out/platine_gerber_vorschau.png)

## Selbst ätzen

- **Tonertransfer oder Belichten:** Die Vorlage zeigt das Kupfer so, wie man von der Bauteilseite durch die Platine schaut. So drucken, wie sie ist, also **nicht spiegeln**. Beim Aufbügeln bzw. Auflegen mit der Druckseite aufs Kupfer stimmt die Lage dann.
- **Prüfmaß:** Die Platine ist 46,5 × 46,0 mm groß (Außenmaß inkl. Zunge, die oben links auf 18 mm verbreitert ist). Nach dem Drucken mit dem Messschieber kontrollieren.
- **Bahnen:** 5 V 1,5 mm breit, Signale 0,8 mm (USB-Daten 0,6 mm), Abstand zur Massefläche 0,5 mm. Die Bohrlöcher haben in der Ätzvorlage einen kleinen freien Punkt als Körnung (in den Gerber-Daten nicht).
- **Bohren:** 0,8 mm (IC), 0,9 mm (R, C, Stiftleisten), 1,0 mm (D1, DIN), 1,3 mm (Litze 1 mm² für USB-C und Streifen), 2,7 mm (Befestigung M2,5).

## Bauteile

| Teil | Wert | Montage |
|---|---|---|
| U1 | 74AHCT125 (DIP-14) | Kerbe zeigt nach rechts (zu den Streifen-Anschlüssen). Pin 1 oben rechts, eckiges Pad. Eine IC-Fassung passt in der Höhe. |
| D1 | 1N5819 | **stehend** links oben in der verbreiterten Zunge, Körper über dem runden Pad (Anode), Ring (Kathode) ins eckige Pad oben. Abstand zur ESP-Stiftleiste ca. 3,5 mm. |
| R1 | 330 Ω | stehend, rechts neben U1 (ca. 1 mm Luft zu einer IC-Fassung) |
| R3 | 1 kΩ | stehend |
| R4 | 2 kΩ | stehend |
| C2 | 100 nF, Folienkondensator CY630 104J (11,5 × 10,3 × 5,4 mm) | Raster 10,16 mm, **stehend** direkt unter U1. Das Pad rechts (an U1 Pin 14) ist +5 V, die Polung ist bei Folienkondensatoren aber egal. |
| C1 | 1000 µF, Elko Ø 8,2 × 16,7 mm, Raster 3,5 mm | **liegend** in der Zunge, Körper Richtung ESP-Anschluss. Beine 90° abwinkeln: Plus ins eckige Pad links, Minus rechts. |

Anschlüsse (Lötpads, im 2,54-mm-Raster, Stiftleisten bzw. T-Pfosten passen auch):

| Pads | Wohin |
|---|---|
| USB-C +5V / GND (links, große Pads) | Litze 1 mm² zu VBUS und GND der USB-C-Buchse |
| LED +5V / DIN / GND (rechts) | Streifen. +5V und GND mit Litze 1 mm². |
| ESP VBUS / IO3 / GND / IO23 (Zungenspitze) | J4 Pin 1, J3 Pin 7, J4 Pin 3, J4 Pin 9 des ESP32-C6-Touch-LCD-1.9 |
| ARGB G / – / D / 5V (Zunge) | Raster wie der 3-polige ARGB-Stecker vom Mainboard (5V, D, Lücke, G). Stiftleiste 1×4 einlöten und den dritten Stift (Lücke) ziehen, dann passt der Stecker direkt. Das 5V-Pad hält nur den Stift und ist nicht angeschlossen. |
| USB D− / D+ (Zunge, rechts neben C1, unten) | optional: D− und D+ der USB-C-Buchse, dünne Litze (0,14 mm²), beide Adern verdrillt |
| IO12 / IO13 (darüber) | optional: J3 Pin 11 (GPIO12) und J3 Pin 12 (GPIO13) des ESP32-C6-Touch-LCD-1.9 |

## PC-Steuerung über die USB-C-Buchse (optional, Lötbrücken SJ1/SJ2)

Mit zwei zusätzlichen Adern lässt sich der Ständer über **ein** USB-Kabel vom PC versorgen und steuern, z. B. mit OpenRGB (Adalight/TPM2, siehe Haupt-README). Die Datenleitungen der Sockel-Buchse gehen dann an den USB-Anschluss des ESP32-C6 (GPIO12 = D−, GPIO13 = D+).

- **SJ1 (D−) und SJ2 (D+)** sind Lötbrücken auf der Kupferseite zwischen den Pad-Paaren. **Offen** (Werkszustand): Die Datenleitungen sind getrennt, die Buchse liefert nur Strom. **Mit Zinn geschlossen:** USB-Daten vom PC. Ohne Brücken kann man die Adern auch einfach weglassen.
- Wenn die Brücken geschlossen sind, **nicht gleichzeitig** ein Kabel an der USB-C-Buchse des ESP-Boards und an der Sockel-Buchse stecken. Beide hängen dann an denselben Datenleitungen. Zum Flashen also entweder die Board-Buchse oder die Sockel-Buchse nutzen.
- Die USB-C-Buchsenplatine muss D+ und D− herausführen (bei den üblichen 16-Pin-Breakouts der Fall). CC1/CC2 mit 5,1 kΩ bleiben wie bisher.
- **Steckbar statt gelötet:** Die Pads USB D−/D+ liegen im 2,54-mm-Raster. Dort passt eine 1×2-Stiftleiste, dazu ein 2-adriges Dupont-Kabel von den Stiften D−/D+ der Buchsenplatine (deren 8-polige Leiste: GND, VBUS, SBU2, CC1, D−, D+, SBU1, CC2). Gesteckt = USB-Daten (bei geschlossenen SJ1/SJ2), abgezogen = nur Strom. VBUS und GND bleiben 1-mm²-Litze: Ein Pin und eine Flachbandader (AWG 28) sind für 2,4 A zu schwach.
- Die USB-Adern von oben einstecken. Die Pads liegen rechts neben dem liegenden C1, der Abstand zum Elko reicht für dünne Litze.
- Am PC-Port steht weniger Strom zur Verfügung als am Netzteil. Die Stromgrenze in der Weboberfläche passend einstellen (siehe Haupt-README, Abschnitt Stromgrenze).

## Unterschiede zum Breadboard-Plan

- **Unbenutzte Kanäle des 74AHCT125:** Hier liegen OE und A der Kanäle 2 bis 4 auf GND, auf dem Breadboard liegen die OE-Pins auf +5 V. Beides ist zulässig. Auf GND aktiviert die Ausgänge, die dann fest auf Low liegen und nirgends angeschlossen sind. So spart die einseitige Platine Brücken.
- **C1 (1000 µF):** sitzt direkt an der USB-C-Einspeisung. Stehend wäre er zu hoch, deshalb liegt er in der verlängerten Zunge. D1 ist dafür nach oben in die links verbreiterte Zungenspitze gewandert, mit Abstand zum ESP-Anschluss.
- Die WAGO-Klemmen entfallen. Die Platine verteilt +5 V und GND.

## Einbau

- Drei Befestigungslöcher M2,5 bei (−5/−6), (27,5/−1) und (0/30) mm, passend zu den Domen im Sockel-Vorschlag (`ESP32-Umbau\Passprobe\Vorschlag_Platine\`). Die Unterseite der Platine liegt 4 mm über dem Sockelboden.
- Bauhöhe über der Platine höchstens 13 mm. Mit stehenden Widerständen und der stehenden Diode bleibt alles unter ca. 11 mm. Am höchsten ist C2 mit 10,3 mm.
- Statt CY630 passt auch ein Keramik-Vielschichtkondensator 100 nF, 50 V, X7R mit Raster 5,08 mm (z. B. KEMET C320C104K5R5TA). Dessen Beine auf 10,16 mm aufbiegen.
