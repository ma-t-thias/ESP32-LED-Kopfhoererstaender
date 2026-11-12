# ESP32-LED-Kopfhoererstaender

WLAN-Steuerung für die LED-Beleuchtung eines Kopfhörerständers. Ein Waveshare ESP32-C6-Touch-LCD-1.9 steuert einen WS2812B-Streifen (5 V, 144 LEDs, 1 m). Über eine Weboberfläche im Heimnetz wählst du Farben, Paletten und bewegte Effekte, ähnlich wie bei WLED. Das eingebaute Display zeigt IP-Adresse, Effekt, Helligkeit und eine LED-Vorschau. Tippen in die obere Hälfte oder Wischen nach rechts schaltet zum nächsten Effekt, Wischen nach links zum vorherigen. Tippen in die untere Hälfte wechselt die Farbe (bei Farbeffekten wie Einfarbig) oder sonst die Palette.

<p align="center">
  <img src="docs/images/weboberflaeche-handy-licht.jpg" alt="Seite Licht mit Presets, Tempo und Effekten, heller Modus" width="250">
  <img src="docs/images/weboberflaeche-handy-farben.jpg" alt="Seite Farben mit Farbrad und Paletten, dunkler Modus" width="250">
  <img src="docs/images/weboberflaeche-handy-einstellungen.jpg" alt="Seite Einstellungen mit Aura, Stromgrenze und Gerätedaten" width="250">
</p>

<p align="center">
  <img src="docs/images/weboberflaeche-pc-dunkel.jpg" alt="Weboberfläche am PC, Seite Licht mit allen Effekten" width="800">
</p>

Die Weboberfläche hat vier Seiten, zwischen denen die Leiste unten umschaltet: **Licht** (Presets, Effekte, Tempo), **Farben** (Farbrad, Paletten), **Einstellungen** (ASUS Aura, PC-Sync, Matter, Stromgrenze, Display-Drehung mit „Auto“ über den Lagesensor, Firmware-Update, WLAN) und **Info** (Gerätedaten mit Firmware-Version, Über dieses Projekt: Copyright, Lizenz, Herkunft, Quelltext). Kopfzeile mit Ein/Aus, Live-Streifen und Helligkeit bleiben auf jeder Seite oben. Jede Seite hat eine eigene Adresse (`#licht`, `#farben`, `#einst`, `#info`), und der Browser merkt sich die zuletzt offene.

Die Bilder zeigen die Weboberfläche mit Beispieldaten. Eine klickbare Vorschau ohne Gerät ist `web/index.html` mit `window.LED_DEMO = true`.

## Hardware

| Bauteil | Anschluss |
|---|---|
| ESP32-C6-Touch-LCD-1.9 | VBUS-Pin über Schottky-Diode 1N5819, GND, **GPIO3 = LED-Daten**, **GPIO23 = ARGB-Eingang** |
| ARGB-Kabel vom Mainboard | D über 1 kΩ / 2 kΩ an GPIO23, G an GND, 5V frei |
| 74AHCT125 | Pegelwandler 3,3 V → 5 V (Kanal 1), 100 nF an Pin 14 |
| WS2812B | DIN über 330 Ω, +5 V/GND direkt vom Netzteil |
| Netzteil | USB-C-Netzteil 5 V / 3 A über USB-C-Buchse mit 5,1-kΩ-CC-Widerständen, Stromgrenze 2400 mA |

Beim ESP32-C6-Touch-LCD-1.9 sind GPIO1 und GPIO2 mit der Lagesensor-IMU verbunden. Deshalb liegen LED-Daten und ARGB-Eingang hier auf **GPIO3** (Leiste mit 1, 2, 3) und **GPIO23** (Leiste mit 22, 23). Das alte ESP32-C6-LCD-1.3 nutzt weiter GPIO1 und GPIO2, siehe `env:esp32-c6-lcd-13`.

Schaltplan, Breadboard-Belegung und Bauteilliste: [`docs/hardware/schaltung.html`](docs/hardware/schaltung.html). Die Zeichnung zeigt das ESP32-C6-Touch-LCD-1.9 (VBUS = J4 Pin 1, GPIO3 = J3 Pin 7, GPIO23 = J4 Pin 9). Im Browser öffnen. Die Seite wird mit `python3 docs/hardware/gen_schaltung.py` aus der Vorlage neu erzeugt.

## Bauen und flashen

Voraussetzung: VS Code mit PlatformIO.

1. Repo mit Fork klonen, z. B. nach `K:\Fork-GitHub\ESP32-LED-Kopfhoererstaender`.
2. Den Ordner in VS Code öffnen. PlatformIO lädt beim ersten Bauen die Plattform `pioarduino` (Arduino-Core 3.x für den ESP32-C6) und die Bibliotheken FastLED und GFX Library for Arduino.
3. Board per USB-C anschließen, **Upload** in der PlatformIO-Leiste klicken.
4. Serieller Monitor mit 115200 Baud zeigt Verbindung und IP-Adresse.

Es gibt zwei Umgebungen, auswählbar unten in der VS-Code-Statusleiste:

| Umgebung | Inhalt | Update per WLAN |
|---|---|---|
| `esp32-c6-lcd` (Standard) | alles, mit Matter für Apple Home & Co. | möglich, siehe unten |
| `esp32-c6-lcd-13` | für das alte ESP32-C6-LCD-1.3 (4 MB), ohne Matter | möglich |

Die Partitionstabelle (`partitions_8mb.csv` bzw. `partitions.csv` beim 1.3er) wird nur per USB geschrieben. Nach dem Wechsel auf eine neue Tabelle deshalb einmal per USB flashen. Die Einstellungen und WLAN-Daten liegen in beiden Tabellen an derselben Stelle und bleiben erhalten. `otadata` muss bei `0xE000` liegen, denn dort setzt der USB-Upload den Merker „starte app0“ zurück. Bis November 2026 lag er bei `0x9000`. Mit diesem alten Stand startete nach einem Web-Update auch nach einem USB-Upload weiter die alte Firmware. Einmal per USB flashen behebt das; Einstellungen, WLAN und Matter-Kopplung bleiben erhalten.

Das USB-C-Netzteil für die LEDs darf beim Flashen angeschlossen sein. Die Diode verhindert, dass es in den USB-Anschluss zurückspeist.

### Versionskennung

Beim Bauen schreibt `tools/version.py` den Stand aus Git in die Firmware: letzter Tag plus Commit-Hash, der Hash steht immer in Klammern dabei. Die Kennung steht im Startbild, auf der Display-Seite **Einrichten**, in der Weboberfläche unter **Info → Gerät → Firmware**, in `/api/info` (Feld `fw`) und im seriellen Monitor.

| Kennung | Bedeutung |
|---|---|
| `V1.0.0 (1a2b3c4)` | genau auf dem Tag `V1.0.0` gebaut, Commit `1a2b3c4` |
| `Prototyp_001-3 (1a2b3c4)` | 3 Commits nach dem Tag, Commit `1a2b3c4` |
| `… (1a2b3c4+)` | mit Änderungen gebaut, die noch nicht committet sind |
| `1a2b3c4` | noch kein Tag vorhanden, nur der Hash |

Einen neuen Stand markieren: in Fork Rechtsklick auf den Commit → **New Tag…**, Namen eingeben (z. B. `v1.0.0`) und den Tag pushen. Oder auf GitHub unter **Releases → Draft a new release** einen Tag anlegen und danach in Fork **Fetch** klicken, damit der Tag auch lokal da ist. Danach einmal neu bauen.

## Erste Einrichtung (WLAN)

Die WLAN-Zugangsdaten stehen nicht im Code.

1. Beim ersten Start öffnet der Controller das WLAN **`LED-Kopfhoerer-Setup`** (ohne Passwort). Das Display zeigt das an.
2. Mit dem Handy verbinden. Die Einrichtungsseite öffnet sich meist von selbst, sonst `http://192.168.4.1` aufrufen.
3. Heim-WLAN wählen, Passwort eingeben, **Speichern und verbinden**. Der Controller startet neu.
4. Danach erreichst du die Steuerung unter **http://led-kopfhoerer.local** oder der IP-Adresse auf dem Display.

Findet der Controller das gespeicherte WLAN nach dem Einschalten 20 Sekunden lang nicht, öffnet er zusätzlich wieder das Einrichtungs-WLAN.

## Bedienung

- **Weboberfläche:** Live-Vorschau des Streifens, Ein/Aus, Helligkeit, Presets, Farbrad für zwei Farben, Paletten, Effekte mit animierter Vorschau, Tempo, Intensität, Aura und PC-Sync, Stromgrenze, Anzeige drehen, WLAN neu einrichten. Hell- und Dunkelmodus folgen dem Gerät. Auf dem iPhone lässt sich die Seite über *Teilen → Zum Home-Bildschirm* wie eine App ablegen.
- **Presets:** Tippen lädt ein Preset, „Speichern“ legt die aktuelle Einstellung als neues an (bis zu 12). Gedrückt halten bzw. Rechtsklick löscht eins. Presets liegen im Flash des Controllers und gelten für alle Geräte im Heimnetz.
- **BOOT-Taster kurz drücken, in die obere Displayhälfte tippen oder nach rechts wischen:** nächster Effekt. **Nach links wischen:** vorheriger Effekt. **In die untere Hälfte tippen:** nächste Farbe bei Farbeffekten (Einfarbig, Atmen, Komet, Kerze ...), sonst nächste Palette. Oben, unten, links und rechts gelten im angezeigten Bild, also passend zur Drehung.
- **Zahnrad oben rechts** (nur 1,9-Zoll-Board mit Touch) öffnet die Seite **Einrichten**: Schieberegler für Helligkeit, Tempo und die Effekt-Stärke (Beschriftung je Effekt, z. B. „Anzahl Punkte“ bei Jonglieren; ausgegraut, wenn der Effekt sie nicht nutzt), dazu IP-Adresse, Hostname, WLAN und Firmware-Version. Das X an derselben Stelle schließt sie, nach 20 s ohne Berührung geht es von selbst zurück. Mit verbundenem WLAN ist die Startseite dafür aufgeräumt: nur Effekt, Helligkeit/Tempo und LED-Vorschau. Ohne WLAN oder bei der Einrichtung stehen die Netzhinweise weiter auf der Startseite.
- **Ausgeschaltet** (über Weboberfläche, Matter oder Apple Home) zeigt das Display das Logo statt der Statusseite. Die erste Berührung oder ein kurzer Druck auf den BOOT-Taster schaltet nur wieder ein, Effekt und Farbe bleiben wie zuvor, und die Statusseite erscheint wieder. Ausnahme: Während der WLAN-Einrichtung oder des Koppelns bleiben deren Hinweise zu sehen.
- **BOOT-Taster 5 s halten:** WLAN-Daten löschen, Neustart ins Einrichtungs-WLAN.
- **Anzeige drehen** (Einstellungen): 0° und 180° ergeben Hochformat, 90° und 270° Querformat, passend zur Gehäusevariante. Die Anzeige ordnet sich selbst neu an.

Alle Einstellungen werden etwa 3 Sekunden nach der letzten Änderung gespeichert und nach einem Neustart wiederhergestellt.

### Effekte

| Effekt | Farben | Intensität bedeutet |
|---|---|---|
| Einfarbig | Farbe 1 | – |
| Regenbogen | – | Anzahl Regenbögen |
| Farbverlauf | Farbe 1 → 2 | Anzahl Wiederholungen |
| Palette | Palette | Dichte |
| Atmen | Farbe 1 | – |
| Farbwischer | Farbe 1 und 2 | – |
| Theater-Jagd | Palette | Abstand |
| Komet | Farbe 1 | Schweiflänge |
| Scanner | Farbe 1 und 2 | Schweiflänge |
| Funkeln | Palette | Häufigkeit |
| Konfetti | Palette | Menge |
| Feuer | Glut-Farben | Flammenhöhe |
| Jonglieren | Palette | Anzahl Punkte |
| Wellen | Palette | Wellengröße |
| Sinus | Farbe 1 und 2 | Frequenz |
| Farbwechsel | Palette | – |
| Nordlicht | eigene Grün-Violett-Töne | Kontrast |
| Plasma | Palette | Feinheit |
| Meteorregen | Palette | Anzahl Meteore |
| Regentropfen | Palette | Häufigkeit |
| Herzschlag | Farbe 1 | Ausbreitung |
| Kerze | Farbe 1 (warm wählen) | Flackern |
| Lichtwelle | Palette | Dichte |
| Ozean | eigene Blautöne | Schaum |

Tempo 0 hält bewegte Effekte an. Die Palette „Eigene Farben“ nutzt Farbe 1 und 2.

## ASUS Aura über das Mainboard-Kabel

Der Ständer kann direkt von Armoury Crate/Aura gesteuert werden, ohne Zusatzsoftware. Dafür liest der ESP das Signal eines ARGB-Headers auf dem Mainboard mit. Für Aura ist der Ständer dann ein ganz normaler ARGB-Streifen.

**Anschluss** (siehe `docs/hardware/schaltung.html`):
- Header-Pin **D** → R3 1 kΩ → **GPIO23**, von GPIO23 → R4 2 kΩ → GND (beim alten 1.3er GPIO2). Der Spannungsteiler macht aus 5 V etwa 3,3 V.
- Header-Pin **G** → GND des Aufbaus.
- Header-Pin **5V** bleibt frei. Die LEDs bekommen ihren Strom weiter vom eigenen Netzteil.

**In Armoury Crate:** Im Aura-Bereich die LED-Anzahl für den ARGB-Anschluss einstellen oder erkennen lassen. Die Firmware erkennt die Anzahl, die das Mainboard sendet (bis 300), und streckt sie auf 144 LEDs. Erlaubt Aura 144 LEDs, entspricht jede Aura-LED genau einer LED am Ständer. Viele Boards begrenzen auf weniger, dann übernimmt eine Aura-LED mehrere Ständer-LEDs.

**Verhalten:** Sendet das Mainboard, zeigt der Ständer die Aura-Farben. Display und Weboberfläche zeigen dann „Aura (… LEDs)“. Kommt 2 Sekunden lang kein gültiges Signal, etwa weil der PC aus ist, laufen wieder die eigenen Effekte. Der Eingang ist ab Werk **aus**. Erst wenn das Kabel angeschlossen ist, in der Weboberfläche den Schalter **Mainboard-ARGB übernehmen** einschalten.

Stürzt der ESP ab, während der ARGB-Empfang läuft, bleibt der Eingang beim nächsten Start gesperrt. WLAN und eigene Effekte laufen dann normal weiter, und die Weboberfläche zeigt einen Hinweis. Nach einem Aus- und Einschalten wird es erneut versucht.

Vorrang: Mainboard-Kabel vor PC-Sync per WLAN vor eigenen Effekten.

## Home Assistant und WLED-App

Der Controller verhält sich nach außen wie ein WLED-Gerät. Deshalb funktionieren die **WLED-Integration von Home Assistant** und die **WLED-App** (iOS/Android) ohne Zusatzsoftware.

**Home Assistant:** Meist taucht der Ständer unter *Einstellungen → Geräte & Dienste* von selbst als „Entdeckt: WLED“ auf. Sonst *Integration hinzufügen → WLED* und `led-kopfhoerer.local` oder die IP-Adresse eintragen.

| In Home Assistant | Am Ständer |
|---|---|
| Licht ein/aus, Helligkeit, Farbe | Ein/Aus, Helligkeit, Farbe 1 |
| Effekt | alle 24 Effekte mit ihren Namen |
| Auswahl „Farbpalette“ | Palette |
| Zahlen „Geschwindigkeit“ und „Intensität“ | Tempo und Intensität |
| Auswahl „Preset“ | Presets aus der Weboberfläche |
| Schalter „Sync Receive“ | PC-Sync per WLAN |

Damit lassen sich Automationen bauen, z. B. „Ständer auf Kerze, wenn abends das Wohnzimmerlicht gedimmt wird“. Nicht nachgebildet sind mehrere Segmente, Playlists, Nachtlicht-Timer und Firmware-Updates über Home Assistant.

## Apple Home, Google Home und Alexa (Matter)

Der Ständer kann sich per Matter über WLAN in Apple Home einbinden, ebenso in Google Home, Alexa oder SmartThings. Das ist ein **Prototyp** und ab Werk aus.

0. Die Standard-Umgebung `esp32-c6-lcd` enthält Matter. Nur das alte 1.3er-Board (`esp32-c6-lcd-13`) hat keinen Matter-Abschnitt in der Weboberfläche.
1. Weboberfläche → *Einstellungen* → *Apple Home · Matter* einschalten. Der Ständer startet neu.
2. Das Display zeigt jetzt einen QR-Code und den Koppel-Code `3497-011-2332`. Beides steht auch in der Weboberfläche.
3. In der Home-App *Gerät hinzufügen* wählen und den QR-Code scannen, oder unter *Weitere Optionen* den Code eingeben. iPhone und Ständer müssen im selben WLAN sein.
4. Die Home-App warnt vor einem nicht zertifizierten Gerät. Mit *Trotzdem hinzufügen* geht es weiter.

In der Home-App lassen sich Ein/Aus, Helligkeit, Farbe und Weißton steuern, also auch per Siri und in Automationen. Wählt man dort eine Farbe, wechselt der Ständer auf den Effekt *Einfarbig*. Effekte und Presets kennt Matter nicht, die bleiben in der Weboberfläche. Änderungen aus der Weboberfläche zeigt die Home-App sofort an. *Kopplung entfernen* in der Weboberfläche löscht die Matter-Daten, danach lässt sich neu koppeln.

**Grenzen des Prototyps:** Er nutzt die Test-Zugangsdaten aus dem Arduino-Core: Hersteller-ID `0xFFF1` und auf jedem Gerät derselbe Koppel-Code. Für den Verkauf braucht es:
- eine Mitgliedschaft in der Connectivity Standards Alliance (CSA) mit eigener Hersteller-ID,
- pro Gerät ein eigenes Zertifikat (DAC) und einen eigenen Koppel-Code, die ab Werk im Flash stehen (dafür ist die Partition `fctry` vorgesehen),
- eine Zertifizierung im Testlabor.

Dafür würde die Firmware auf ESP-IDF mit esp-matter umziehen. Auf echter Hardware noch zu prüfen: ob das Matter-mDNS und das mDNS für `led-kopfhoerer.local`/WLED sauber nebeneinander laufen.

## PC-Sync mit SignalRGB oder OpenRGB

Der Controller nimmt Farben vom PC entgegen und leuchtet dann synchron mit Mainboard, Lüftern und Co. (z. B. ASUS ARGB). Sendet der PC nichts mehr, laufen nach 2,5 Sekunden wieder die eigenen Effekte. Der Schalter **PC-Sync** in der Weboberfläche schaltet den Empfang ab.

| Protokoll | Port | Typische Quelle |
|---|---|---|
| DDP | UDP 4048 | SignalRGB, xLights, Hyperion |
| E1.31 / sACN | UDP 5568, Universum 1 | OpenRGB, xLights, Jinx |
| WLED-UDP (DRGB, DNRGB, WARLS) | UDP 21324 | SignalRGB (WLED-Integration), LedFx |

Damit Sync-Programme den Controller als WLED-Gerät erkennen, meldet er sich per mDNS als `_wled._tcp` und beantwortet die WLED-Schnittstelle (siehe Home Assistant).

**SignalRGB:** Unter *Geräte* die WLED-Integration öffnen und den Controller hinzufügen. Wenn er nicht automatisch auftaucht, die IP-Adresse vom Display von Hand eintragen. Danach als Streifen mit 144 LEDs im Layout platzieren.

**OpenRGB:** Unter *Settings → E1.31 Devices* ein Gerät anlegen: IP-Adresse des Controllers, Universum 1, Startkanal 1, 144 LEDs, Typ „Linear“. Danach OpenRGB neu starten.

Die Helligkeit aus der Weboberfläche und die Stromgrenze gelten auch im PC-Sync. Für die vollen Farben vom PC stellst du die Helligkeit auf 100 %.

### PC-Sync über USB-C (ohne WLAN)

Statt per WLAN kann der PC die Farben auch über ein USB-C-Kabel schicken, das zugleich den Ständer versorgt. Der Controller versteht dafür das **Adalight**-Protokoll über die USB-Schnittstelle des ESP32-C6.

- **Hardware:** Auf der Sockel-Platine sitzen zwei 1×2-Stiftleisten (2,54 mm): eine für VBUS/GND, eine für D−/D+. Dazu führen Dupont-Kabel bzw. Litzen von der USB-C-Buchsenplatine (Pins 1–2 und 5–6 der 8er-Leiste; dort wegen der Höhe direkt angelötet). Das Datenkabel ist der Schalter: gesteckt gehen D+/D− der Sockel-Buchse an den ESP (J3 Pin 12 = GPIO13 = D+, J3 Pin 11 = GPIO12 = D−), abgezogen läuft nur der Strom. Zum ESP führt eine dritte 1×2-Stiftleiste an der Zungenspitze (ESP D+/D−). Details zu Platine, Anschlüssen und Bestellung: [docs/hardware/platine/README.md](docs/hardware/platine/README.md). Mit Netzteil ist es egal, ob es steckt. Zum Flashen über die Buchse am ESP-Board das Datenkabel abziehen, damit nicht zwei USB-Hosts an denselben Leitungen hängen.
- **OpenRGB:** Unter *Settings → Serial Devices* (je nach Version *LED Strip*) ein Gerät anlegen: COM-Port des Controllers, Protokoll **Adalight**, 144 LEDs, Baudrate beliebig (z. B. 115200). Danach OpenRGB neu starten. Prismatik und Hyperion funktionieren mit Adalight genauso.
- **Strom:** Ein PC-Anschluss liefert weniger als ein Netzteil (USB-C meist 1,5 A, mit PD auch 3 A). Die **Stromgrenze** in der Weboberfläche passend einstellen, z. B. 1200 mA an einem 1,5-A-Anschluss.

Der Schalter **PC-Sync** gilt auch für USB, auf dem Display steht dann „PC-Sync Adalight USB“.

## Stromgrenze

Die Firmware begrenzt die Helligkeit so, dass der berechnete Strom die eingestellte Grenze nicht überschreitet. Startwert ist 2400 mA (`include/config.h`), passend zum USB-C-Netzteil 5 V / 3 A. Farbige Effekte laufen damit in voller Helligkeit, nur Vollweiß auf allen LEDs wird gedimmt. War schon ein anderer Wert gespeichert, stellst du ihn in der Weboberfläche auf 2400 mA. Mit einem stärkeren Netzteil (z. B. USB-C PD 12 V und Step-down auf 5 V / 6 A) kannst du auf etwa 85 % von dessen Strom erhöhen.

## Firmware-Update über die Weboberfläche

1. Nur für getaggte Stände: Den Commit in Fork taggen (Rechtsklick auf den Commit → „New Tag…“, z. B. `V1.0.0`) und den Tag pushen. Dann in VS Code unter *PlatformIO → Project Tasks → env → Custom* **Update-Datei erzeugen** ausführen. Danach liegt im Projektordner unter `update/` die Datei `LED-Kopfhoerer_LCD19_V1.0.0.bin` (altes 1,3-Zoll-Board: `_LCD13_`). Das ist die normale `firmware.bin` mit sprechendem Namen (`tools/update_file.py`). Hat der Commit keinen Tag oder gibt es nicht eingecheckte Änderungen, bricht die Aufgabe mit einer Meldung ab und erzeugt keine Datei. So ist jede Update-Datei einem Tag zugeordnet. Prototypen flasht man weiter wie gewohnt mit Build/Upload per USB oder WLAN.
2. In der Weboberfläche **Einstellungen → Firmware-Update → Datei wählen**, die Datei auswählen und **Update installieren**. Der Ständer fragt dann am Display **„Update?“**: dort **Erlauben** tippen (altes 1,3-Zoll-Board: BOOT-Taste kurz drücken). Ohne Antwort innerhalb von 60 s oder mit **Ablehnen** wird nichts hochgeladen. Erst nach der Bestätigung beginnt der Upload; so kann niemand im WLAN ohne Zugriff auf den Ständer eine Firmware aufspielen.
3. Ein Balken zeigt den Upload in Prozent, das Display den Fortschritt mit Sanduhr. Danach prüft der Ständer die Datei, startet neu, und die Seite lädt sich neu. Die neue Version steht unter **Info → Gerät → Firmware**.

Geprüft wird vorher: die Datei passt in den freien Firmware-Speicher (sonst Meldung mit beiden Größen, noch bevor das Display fragt), der Name enthält die Board-Kennung (`_LCD19_` bzw. `_LCD13_`, deshalb die Datei nicht umbenennen), es ist eine Firmware für den ESP32-C6, und die Prüfsumme stimmt. Bei einem Fehler bleibt die alte Firmware aktiv. Die Partitionstabelle ändert ein Update nicht; dafür bleibt USB nötig.


## Update über WLAN aus VS Code (nur Testbetrieb)

Die normale Firmware nimmt kein Update direkt aus VS Code an (dafür gibt es die Weboberfläche). Für den Testbetrieb gibt es zwei zusätzliche Umgebungen, die nur lokal existieren:

1. `test_ota.example.ini` nach `test_ota.ini` kopieren (wird nicht eingecheckt) und dort ein eigenes Passwort eintragen.
2. VS Code neu laden. Unten in der Statusleiste gibt es jetzt `env:test-usb` und `env:test-ota`.
3. Einmal `env:test-usb` wählen und per USB hochladen. Diese Firmware nimmt Updates per WLAN an.
4. Ab dann `env:test-ota` wählen und **Upload** klicken. Die Firmware geht per WLAN auf den Ständer, das Display zeigt „Update…“.

Findet der PC `led-kopfhoerer.local` nicht, in `test_ota.ini` bei `host` die IP-Adresse vom Display eintragen. Fragt die Windows-Firewall beim ersten Mal nach Python, für private Netzwerke zulassen. Ohne `test_ota.ini` gibt es die Test-Umgebungen nicht, und die Firmware bleibt ohne Update per WLAN.

## API

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/api/state` | aktueller Zustand als JSON |
| POST | `/api/state` | Felder `on`, `bri`, `fx`, `pal`, `spd`, `int`, `c1`, `c2` (`#rrggbb`), `ma`, `sync`, `argb`, `rot` (0–3, 4 = Auto), `matter` (0/1, startet neu) |
| GET | `/api/info` | Firmware-Version (`fw`), Platz für Updates in Byte (`fwMax`), Effekt- und Palettennamen, Netzwerkdaten |
| POST | `/api/matter/unpair` | Matter-Kopplung löschen, danach Neustart |
| POST | `/api/update/ask?size=` | prüft die Dateigröße (413, wenn zu groß), fragt dann am Display „Update?“, liefert `token` (409, wenn schon eine Frage läuft) |
| GET | `/api/update/answer?t=` | Antwort für dieses Token: 1 erlaubt, -1 abgelehnt/abgelaufen, 0 offen |
| POST | `/api/update?t=&size=` | Firmware-Update: Formular-Upload einer `.bin` (Name mit `_LCD19_`/`_LCD13_`), nur nach Erlauben, danach Neustart |
| GET | `/api/live` | aktuelle LED-Farben als Hex-Text, 6 Zeichen je LED |
| GET/POST | `/api/presets` | Presets als JSON (POST-Feld `data`) |
| GET | `/json`, `/json/si`, `/json/info`, `/json/state`, `/json/eff`, `/json/pal`, `/presets.json` | WLED-kompatible Abfrage |
| POST | `/json/state` | WLED-kompatibel setzen: `on`, `bri`, `ps`, `udpn.recv`, `seg` mit `col`, `fx`, `sx`, `ix`, `pal` |

Beispiel: `curl -X POST -d "fx=11&bri=200" http://led-kopfhoerer.local/api/state`

## Aufbau des Codes

| Datei | Inhalt |
|---|---|
| `include/config.h` | Pins, LED-Anzahl, Stromgrenze, Hostname |
| `tools/version.py` | schreibt beim Bauen die Versionskennung nach `include/fw_version.h` (nicht eingecheckt) |
| `tools/update_file.py` | Aufgabe „Update-Datei erzeugen“: legt für getaggte Stände die Update-Datei in `update/` an (nicht eingecheckt) |
| `src/main.cpp` | Ablauf, Bildrate, Ein-/Ausblenden, BOOT-Taster |
| `src/effects.cpp` | alle Effekte und Paletten |
| `src/net.cpp` | WLAN, Einrichtungs-WLAN, Webserver, mDNS, OTA, Firmware-Update per Weboberfläche |
| `src/realtime.cpp` | PC-Sync: Empfang von DDP, E1.31 und WLED-UDP, Adalight über USB |
| `src/wled_api.cpp` | WLED-kompatible JSON-Schnittstelle für Home Assistant und WLED-App |
| `src/matter_dev.cpp` | Matter-Lampe für Apple Home & Co., Koppel-Code und QR-Inhalt |
| `src/argb_in.cpp` | ARGB-Eingang: liest das Mainboard-Signal per RMT an GPIO23 mit |
| `web/index.html` | Weboberfläche (Quelle) |
| `src/webui_index.h` | daraus erzeugt mit `python tools/web2h.py`, nicht von Hand ändern |
| `src/webui.h` | Einrichtungsseite |
| `src/display.cpp` | Anzeige auf dem LCD, passt sich an Hoch- und Querformat an |
| `src/touch.cpp` | Touch: Tippen (oben/unten) und Wischen erkennen |
| `src/imu.cpp` | Lagesensor: Displaydrehung „Auto“ nach der Schwerkraft |
| `src/settings.cpp` | Speichern der Einstellungen und WLAN-Daten im Flash |

## Lizenz

© 2026 Matthias Steis. Lizenz: [CC BY-NC-SA 4.0](LICENSE.md). Nachbau und Änderungen sind mit Namensnennung für nicht kommerzielle Zwecke erlaubt, Verkauf nur mit Erlaubnis. Fremde Bestandteile und ihre Lizenzen stehen in [LICENSE.md](LICENSE.md).
