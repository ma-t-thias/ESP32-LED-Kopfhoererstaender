# Ständer v2 – Druckdateien (Stand v10, 22.11.2026)

Aktuelle Druck-STLs des Ständers v2, in Druckausrichtung. Sie stammen aus der Baugruppe `Staender_v10_Gesamt_B` (Sockel v9, Auflage v4). Die SolidCraft-Quellen (`.sc`) und die ausführliche `DRUCKLISTE.txt` liegen auf Matthias' PC unter `ESP32-Umbau\Passprobe\Sensoren\`. Der Stand ist noch nicht komplett probegedruckt.

**Druckeinstellungen:** PLA 0,2 mm Schicht, 3 Wände, 20 % Füllung · Fluor-PLA 100 % konzentrisch, 0,4-mm-Düse · TPU (Shore 95A) 100 %

## Gemeinsame Teile (Variante A und B)

| Datei | Teil | Material | Stück | Hinweis |
|---|---|---|---|---|
| `Sockel_oval_v9.stl` | Sockel-Schale | PLA schwarz | 1 | Oberseite nach unten aufs Bett. v9: 4 Mikro-Schalllöcher Ø2,0, Mikro-Kragen Ø10/Ø7 |
| `Boden_v5.stl` | Bodenplatte | PLA schwarz | 1 | flach; 4 Dome für Platine v2 (M2,5 selbstschneidend) |
| `Auflage_v4.stl` | Kopfhörer-Auflage | PLA schwarz | 1 | auf der Rückseite liegend. Bajonett mit Endanschlag und Rastung, Außenkanten R 2 |
| `Druckplatte_FSR.stl` | Druckplatte für den FSR | PLA schwarz | 1 | flach, Noppe nach oben. Einsetzen, im Uhrzeigersinn drehen bis es klickt |
| `ESP32_Lid_Mikro_hochkant.stl` | Deckel Display-Gehäuse | PLA schwarz | 1 | hochkant-Variante |
| `Inlay_Saeule_v3.stl` | Säulen-Streifen vorne | Fluor-PLA | 1 | Flansch + 8 Klemmnoppen, von unten in die Nut schieben |
| `Inlay_Turm_hinten.stl` | Lichtschlitz hinter dem Display-Turm | Fluor-PLA | 1 | Flansch aufs Bett, von innen einsetzen |
| `Inlay_Leuchtring_v3.stl` | Leuchtring | Fluor-PLA | 1 | flach |
| `Inlay_Augen_links_v3.stl` | Augen links (3 Fenster + Flansch) | Fluor-PLA | 1 | stehend wie eingebaut, ggf. Baumstützen |
| `Inlay_Augen_rechts_v3.stl` | Augen rechts | Fluor-PLA | 1 | wie links |
| `TPU_Fuss_4x.stl` | Fuß Ø12 × 5 mm | TPU | 4 | Pilzkopf-Zapfen rastet im Boden ein |

## Variante A „Glatt“ (durchgehender Lichtstreifen)

| Datei | Teil | Material | Stück | Hinweis |
|---|---|---|---|---|
| `Saeule_v2_A_Glatt.stl` | Säule | PLA schwarz | 1 | auf der Seite liegend, ohne Stützen |
| `Inlay_Seite_A_links.stl` | Seiten-Inlay links | Fluor-PLA | 1 | Außenfläche aufs Bett |
| `Inlay_Seite_A_rechts.stl` | Seiten-Inlay rechts | Fluor-PLA | 1 | wie links |

## Variante B „Rauten“ (10 Rauten je Seite)

| Datei | Teil | Material | Stück | Hinweis |
|---|---|---|---|---|
| `Saeule_v2_B_Rauten.stl` | Säule | PLA schwarz | 1 | auf der Seite liegend, ohne Stützen |
| `Inlay_Seite_B_links.stl` | Seiten-Inlay links | Fluor-PLA | 1 | Platte aufs Bett, Stopfen nach oben |
| `Inlay_Seite_B_rechts.stl` | Seiten-Inlay rechts | Fluor-PLA | 1 | wie links |

Zusätzlich nötig: Moosgummi-Ring Ø10/Ø7 × 1,5 mm zwischen Mikrofon und Kragen; Schrauben und Elektronik siehe `DRUCKLISTE.txt` auf Matthias' PC.

Die STLs für die Blender-Simulation (Montagelage) liegen getrennt in `tools/blender/stl/`.
