#pragma once
// Zentrale Einstellungen der Firmware. Hier wird alles angepasst, was von der
// Hardware abhaengt. WLAN-Zugangsdaten stehen NICHT hier: die gibt man beim
// ersten Start im Einrichtungs-WLAN ein (siehe README).

// Board: Standard ist das Waveshare ESP32-C6-Touch-LCD-1.9.
// Das alte ESP32-C6-LCD-1.3 baut die Umgebung env:esp32-c6-lcd-13 (setzt BOARD_LCD_13).

// ---------------- LED-Streifen ----------------
#ifdef BOARD_LCD_13
#define LED_PIN 1            // GPIO1 -> 74AHCT125 Pin 2 (1A)
#else
#define LED_PIN 3            // GPIO3 -> 74AHCT125 Pin 2 (1A); GPIO1/2 sind hier die IMU-Interrupts
#endif
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
#ifdef BOARD_LCD_13
#define ARGB_IN_PIN 2
#else
#define ARGB_IN_PIN 23
#endif
// Ohne gueltiges Signal so lange laufen danach wieder die eigenen Effekte.
#define ARGB_TIMEOUT_MS 2000
// Byte-Reihenfolge, die das Mainboard sendet. WS2812-Standard ist G, R, B.
#define ARGB_IN_ORDER_R 1
#define ARGB_IN_ORDER_G 0
#define ARGB_IN_ORDER_B 2

// ---------------- Display ----------------
#ifdef BOARD_LCD_13
// Waveshare ESP32-C6-LCD-1.3: 240x240, Hintergrundlicht HIGH = an
#define LCD_SCK 7
#define LCD_MOSI 6
#define LCD_DC 15
#define LCD_CS 14
#define LCD_RST 21
#define LCD_BL 22
#define LCD_BL_ON HIGH
#define LCD_W 240
#define LCD_H 240
#define LCD_ROTATION 1  // Startwert, in der Weboberflaeche einstellbar (0..3 = 0/90/180/270 Grad)
// 240x240-Panel am 240x320-Controller: bei 180 und 270 Grad liegt das Bild 80 Zeilen versetzt
#define LCD_OFFSETS 0, 0, 0, 80
#else
// Waveshare ESP32-C6-Touch-LCD-1.9: 170x320 hochkant, Hintergrundlicht LOW = an
// (P-Kanal-FET). Pins und Offsets aus dem Waveshare-Beispiel 07_Hello_World_GFX.
#define LCD_SCK 5
#define LCD_MOSI 4
#define LCD_DC 6
#define LCD_CS 7
#define LCD_RST 14
#define LCD_BL 15
#define LCD_BL_ON LOW
#define LCD_W 170
#define LCD_H 320
#define LCD_ROTATION 0  // 0/2 = hochkant, 1/3 = quer; in der Weboberflaeche einstellbar
// 170 Spalten mittig im 240er Controller
#define LCD_OFFSETS 35, 0, 35, 0
// Touch CST816D am I2C-Bus (teilt ihn mit der IMU). Tippen = naechster Effekt.
#define TOUCH_SDA 18
#define TOUCH_SCL 8
#define TOUCH_ADDR 0x15
// Lagesensor QMI8658 am selben Bus. Anzeige "Auto" dreht das Bild nach der Schwerkraft.
#define IMU_ADDR 0x6B
// Welche Drehung (0..3) passt, wenn die Schwerkraft entlang +X/-X/+Y/-Y des Sensors zeigt.
// Am Board ermittelt (6.11.2026: erste Annahme war 90 Grad zu weit gegen den Uhrzeigersinn).
// Bei falscher Lage die Werte tauschen (Serial zeigt ax/ay).
#define IMU_ROT_XPOS 3
#define IMU_ROT_XNEG 1
#define IMU_ROT_YPOS 2
#define IMU_ROT_YNEG 0
#endif

// Anzeige-Drehung 4 = automatisch ueber den Lagesensor (nur mit IMU_ADDR)
#define ROT_AUTO 4
#ifdef IMU_ADDR
#define ROT_DEFAULT ROT_AUTO
#else
#define ROT_DEFAULT LCD_ROTATION
#endif

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

// Startbild (Bootlogo): so lange nach dem Einschalten sichtbar, LEDs und WLAN starten dabei schon
#define BOOT_LOGO_MS 6000
