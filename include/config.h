#pragma once
// Zentrale Einstellungen der Firmware. Hier wird alles angepasst, was von der
// Hardware abhaengt. WLAN-Zugangsdaten stehen NICHT hier: die gibt man beim
// ersten Start im Einrichtungs-WLAN ein (siehe README).

// ---------------- LED-Streifen ----------------
#define LED_PIN 1            // GPIO1 -> 74AHCT125 Pin 2 (1A)
#define LED_COUNT 144        // WS2812B, 1 m, 144 LEDs
#define LED_COLOR_ORDER GRB  // WS2812B ist fast immer GRB

// Strombegrenzung fuer den Streifen in mA (WLED nennt das "ABL").
// Faustregel: ca. 85 % des Netzteil-Nennstroms, abzueglich ~300 mA fuer den ESP.
//   5 V / 4 A-Netzteil  -> 3000
//   5 V / 10 A-Netzteil -> 8000
// Ueber die Weboberflaeche laesst sich der Wert spaeter anpassen; dies ist der Startwert.
#define LED_MAX_MILLIAMPS_DEFAULT 2400  // USB-C-Netzteil 5 V / 3 A
#define LED_MAX_MILLIAMPS_LIMIT 9000  // Obergrenze, die die Weboberflaeche zulaesst

// ---------------- PC-Sync (SignalRGB, OpenRGB ...) ----------------
// Kommen laenger als diese Zeit keine Daten vom PC, laufen wieder die eigenen Effekte.
#define REALTIME_TIMEOUT_MS 2500
// E1.31/sACN: erstes Universum (144 LEDs passen in ein Universum mit 170 LEDs)
#define REALTIME_E131_UNIVERSE 1
// Geraetename, unter dem SignalRGB & Co. den Controller anzeigen
#define DEVICE_NAME "LED-Kopfhoerer"

// ---------------- ARGB-Eingang (Mainboard, z. B. ASUS Aura) ----------------
// Daten-Pin des Mainboard-ARGB-Headers ueber Spannungsteiler 1 kOhm / 2 kOhm.
#define ARGB_IN_PIN 2
// Ohne gueltiges Signal so lange laufen danach wieder die eigenen Effekte.
#define ARGB_TIMEOUT_MS 2000
// Byte-Reihenfolge, die das Mainboard sendet. WS2812-Standard ist G, R, B.
#define ARGB_IN_ORDER_R 1
#define ARGB_IN_ORDER_G 0
#define ARGB_IN_ORDER_B 2

// ---------------- Display (Waveshare ESP32-C6-LCD-1.3) ----------------
#define LCD_SCK 7
#define LCD_MOSI 6
#define LCD_DC 15
#define LCD_CS 14
#define LCD_RST 21
#define LCD_BL 22
#define LCD_W 240
#define LCD_H 240
#define LCD_ROTATION 1  // Startwert, in der Weboberflaeche einstellbar (0..3 = 0/90/180/270 Grad)
// 240x240-Panel am 240x320-Controller: bei 180 und 270 Grad liegt das Bild 80 Zeilen versetzt
#define LCD_ROW_OFFSET2 80

// ---------------- Taster ----------------
// BOOT-Taster auf dem Board. Kurz druecken: naechster Effekt.
// 5 s halten: gespeicherte WLAN-Daten loeschen und Einrichtungs-WLAN starten.
#define BUTTON_PIN 9

// ---------------- Netzwerk ----------------
#define HOSTNAME "led-kopfhoerer"          // erreichbar als http://led-kopfhoerer.local
#define SETUP_AP_SSID "LED-Kopfhoerer-Setup"  // offenes WLAN fuer die Ersteinrichtung
#define WIFI_CONNECT_TIMEOUT_MS 20000

// OTA-Update ueber WLAN (PlatformIO: upload_protocol = espota).
// Leer lassen = OTA aus. Eigenes Passwort am besten in include/secrets.h
// definieren (die Datei ist in .gitignore), z. B.:
//   #define OTA_PASSWORD "meinPasswort"
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif
