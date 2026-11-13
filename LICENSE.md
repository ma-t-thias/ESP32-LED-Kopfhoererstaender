# Lizenz

© 2026 Matthias Steis

Dieses Projekt (Firmware, Weboberfläche, Dokumentation, Gehäuse- und Platinendaten) steht unter der Lizenz
**Creative Commons Namensnennung – Nicht kommerziell – Weitergabe unter gleichen Bedingungen 4.0 International (CC BY-NC-SA 4.0)**.

Vollständiger Lizenztext: https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode.de
Kurzfassung: https://creativecommons.org/licenses/by-nc-sa/4.0/deed.de

Kurz gesagt:

- **Erlaubt:** nachbauen, ändern und weitergeben, für private und andere nicht kommerzielle Zwecke.
- **Namensnennung:** „Matthias Steis“, Link zu diesem Projekt und Hinweis auf die Lizenz angeben, Änderungen kenntlich machen.
- **Nicht kommerziell:** Verkauf oder andere kommerzielle Nutzung nur mit schriftlicher Erlaubnis von Matthias Steis.
- **Gleiche Bedingungen:** Geänderte Fassungen nur unter derselben Lizenz weitergeben.

## Fremde Bestandteile

Diese Teile stehen unter ihren eigenen Lizenzen, die hier unverändert gelten:

| Teil | Urheber | Lizenz |
|---|---|---|
| Grundkörper des Ständers „RGB Headphone Stand“, [thingiverse.com/thing:4722406](https://www.thingiverse.com/thing:4722406). Remix von „Headphone stand (setup themed)“, [thingiverse.com/thing:3534573](https://www.thingiverse.com/thing:3534573). Sockel (`Base_Box`) verändert, Display-Gehäuse und Platine neu. | FaisLeFr (Remix), NoycePrints (Original) | CC BY-NC (nicht kommerziell); laut `LICENSE.txt` im Download-Paket: „created by Thingiverse user NoycePrints, and is licensed under cc-nc“ |
| FastLED | FastLED-Projekt | MIT |
| GFX Library for Arduino | moononournation, auf Basis Adafruit GFX | BSD |
| ArduinoJson | Benoît Blanchon | MIT |
| Arduino-Core für ESP32 | Espressif Systems | LGPL-2.1 |
| ESP-IDF, Matter-SDK | Espressif Systems, Connectivity Standards Alliance u. a. | Apache-2.0 |

Wichtig für den Ständer: Weil das Original von NoycePrints nicht kommerziell lizenziert ist, gilt das auch für alle daraus abgeleiteten Druckteile (Seiten, Auflage, Abdeckungen, Sockel einschließlich `Base_Box_ESP32` mit angesetztem Display-Gehäuse). Eine kommerzielle Nutzung dieser Teile braucht zusätzlich die Erlaubnis der Rechteinhaber (NoycePrints, ggf. FaisLeFr), oder die Teile müssen neu konstruiert werden. Platine und Firmware sind davon nicht betroffen.

Die Lizenztexte der Bibliotheken stehen in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Wer eine fertige Firmware-Datei weitergibt, legt diese Datei bei. Der Arduino-Core ist LGPL-2.1: Weil der vollständige Quelltext dieses Projekts öffentlich ist, kann jeder die Firmware mit einer geänderten Fassung des Cores neu bauen.

Hinweis: Creative Commons empfiehlt seine Lizenzen eigentlich nicht für Software. Für den Quelltext gilt CC BY-NC-SA hier bewusst, damit Firmware und Hardware einheitlich nicht kommerziell bleiben.

Die JSON-Schnittstelle ist mit WLED kompatibel, enthält aber keinen WLED-Code. Apple, Apple Home, Matter, ASUS Aura, SignalRGB und OpenRGB sind Marken ihrer jeweiligen Inhaber; dieses Projekt ist mit keinem dieser Unternehmen verbunden.

## Haftung

Bereitgestellt ohne Gewähr. Nachbau und Betrieb auf eigene Verantwortung, besonders bei Netzteil und Verkabelung.
