#include "selftest.h"

#include <math.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "fw_version.h"
#include "imu.h"
#include "sensors.h"
#include "state.h"

namespace selftest {

namespace {
#ifdef TOUCH_ADDR
constexpr bool HAS_TOUCH = true;
#else
constexpr bool HAS_TOUCH = false;
#endif

enum Step : uint8_t { IDLE, RED, GREEN, BLUE, WHITE, CHASE, TOUCH, BUTTON, HPMIC, SENSOR, RESULT };
constexpr uint32_t COLOR_MS = 1500;   // je Farbe
constexpr uint32_t CHASE_MS = 4000;   // ein Lichtpunkt laeuft einmal ueber den Streifen
constexpr uint32_t WAIT_MS = 15000;   // so lange wartet der Test auf Tippen/Taste
constexpr uint32_t HPMIC_MS = 10000;  // Drucksensor und Mikrofon live ansehen
constexpr uint32_t RESULT_MS = 30000;  // so lange bleibt das Ergebnis stehen (Tippen beendet frueher)

Step step = IDLE;
uint32_t stepSince = 0;
bool btnSeen = false;  // Taste war gedrueckt, jetzt aufs Loslassen warten
// Ergebnis: "ok", "fehlt" (keine Reaktion), "fehler" (falscher Messwert), "-" (Board ohne)
String rTouch = "-", rButton = "-", rSensor = "-", rHp = "-", rMic = "-";
float gValue = 0;
uint16_t hpMax = 0;  // hoechster Druck-Messwert (mV, nach Nullpunkt) im Schritt HPMIC
uint8_t micMax = 0;  // hoechste Lautstaerke 0..255
bool micSeen = false;  // Mikrofon hat Daten geliefert
bool done = false, passed = false;

const char *stepName() {
  switch (step) {
    case RED: case GREEN: case BLUE: case WHITE: case CHASE: return "leds";
    case TOUCH: return "touch";
    case BUTTON: return "button";
    case HPMIC: return "hpmic";
    case SENSOR: return "sensor";
    case RESULT: return "result";
    default: return "idle";
  }
}

void go(Step s) {
  step = s;
  stepSince = millis();
  btnSeen = false;
  switch (s) {
    // grosse Zeilen hoechstens 12 Zeichen (Hochformat 170 px)
    case RED: display::testScreen("Selbsttest", "Alle LEDs\nrot?", 0); break;
    case GREEN: display::testScreen("Selbsttest", "Alle LEDs\ngruen?", 0); break;
    case BLUE: display::testScreen("Selbsttest", "Alle LEDs\nblau?", 0); break;
    case WHITE: display::testScreen("Selbsttest", "Alle LEDs\nweiss?", 0); break;
    case CHASE: display::testScreen("Selbsttest", "Lichtpunkt\nlaeuft bis\nzum Ende?", 0); break;
    case TOUCH: display::testScreen("Selbsttest", "Bitte das\nDisplay\nantippen", 0); break;
    case BUTTON: display::testScreen("Selbsttest", "Bitte BOOT-\nTaste\ndruecken", 0); break;
    case HPMIC: display::testScreen("Selbsttest", "", 0, "Drucksensor druecken und\nsprechen oder klatschen"); break;
    default: break;
  }
}

void checkSensor() {
  int16_t ax, ay, az;
  if (!imu::present()) {
#ifdef IMU_ADDR
    rSensor = "fehlt";  // Board hat einen Lagesensor, er antwortet aber nicht
#endif
    return;
  }
  if (!imu::readAccel(ax, ay, az)) {
    rSensor = "fehler";
    return;
  }
  gValue = sqrtf(float(ax) * ax + float(ay) * ay + float(az) * az) / 16384.0f;  // +-2 g: 1 g = 16384
  rSensor = (gValue > 0.8f && gValue < 1.2f) ? "ok" : "fehler";  // in Ruhe misst er etwa 1 g
}

void finish() {
  done = true;
  passed = rTouch != "fehlt" && rButton == "ok" && rSensor != "fehlt" && rSensor != "fehler" &&
           rHp != "fehlt" && rMic != "fehlt" && rMic != "fehler";
  String body = "Touch " + rTouch + "\nTaste " + rButton + "\nLage " + rSensor;  // je max. 12 Zeichen
  if (rHp != "-") body += "\nDruck " + rHp + "\nMikro " + rMic;
  String sn = settings::serialNumber();
  String foot = "SN " + (sn.length() ? sn : String("keine"));
  if (rSensor == "ok" || rSensor == "fehler") foot += "\nLagesensor " + String(gValue, 2) + " g";
  if (rHp != "-") foot += "\nmax " + String(hpMax) + " mV, Mikro " + String(micMax);
  foot += "\nLEDs: selbst ansehen";
  display::testScreen(passed ? "Test OK" : "Test FEHLER", body, passed ? 1 : 2, foot);
  // eine Zeile fuer Pruefprogramme am USB
  Serial.printf("SELBSTTEST %s touch=%s taste=%s lage=%s g=%.2f druck=%s/%u mikro=%s/%u sn=%s fw=%s\n",
                passed ? "BESTANDEN" : "FEHLER", rTouch.c_str(), rButton.c_str(), rSensor.c_str(), gValue, rHp.c_str(),
                hpMax, rMic.c_str(), micMax, sn.length() ? sn.c_str() : "-", FW_FULL);
  time_t now = time(nullptr);
  settings::saveSelftest(passed, now > 1700000000 ? uint32_t(now) : 0);  // Datum nur mit Uhrzeit aus dem Netz
  step = RESULT;
  stepSince = millis();
  btnSeen = false;
}
}  // namespace

void start() {
  rTouch = HAS_TOUCH ? "fehlt" : "-";
  rButton = "fehlt";
  rSensor = "-";
  rHp = rMic = "-";
  gValue = 0;
  hpMax = micMax = 0;
  micSeen = false;
  done = passed = false;
  Serial.println("Selbsttest startet");
  go(RED);
}

bool running() { return step != IDLE; }

bool micWanted() { return step == HPMIC; }

uint8_t brightness() { return 80; }  // reicht zum Erkennen, schont Netzteil und Augen

void loop(CRGB *leds, uint16_t n, bool buttonDown, bool touched) {
  if (step == IDLE) return;
  uint32_t t = millis() - stepSince;
  switch (step) {
    case RED: case GREEN: case BLUE: case WHITE: {
      static const CRGB C[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
      fill_solid(leds, n, C[step - RED]);
      if (t >= COLOR_MS) go(Step(step + 1));
      break;
    }
    case CHASE: {
      fill_solid(leds, n, CRGB::Black);
      uint16_t i = min<uint32_t>(n - 1, t * n / CHASE_MS);
      leds[i] = CRGB::White;
      if (t >= CHASE_MS) go(HAS_TOUCH ? TOUCH : BUTTON);
      break;
    }
    case TOUCH:
      fill_solid(leds, n, CRGB::Black);
      if (touched) rTouch = "ok";
      if (touched || t >= WAIT_MS) go(BUTTON);
      break;
    case BUTTON:
      fill_solid(leds, n, CRGB::Black);
      if (buttonDown) btnSeen = true;
      else if (btnSeen) rButton = "ok";  // erst nach dem Loslassen weiter
      if (rButton == "ok" || t >= WAIT_MS) {
        if (sensors::hpAvailable()) {
          go(HPMIC);
          break;
        }
        step = SENSOR;
        checkSensor();
        finish();
      }
      break;
#ifdef HP_SENSE_PIN  // nur Boards mit Drucksensor und Mikrofon
    case HPMIC: {
      // 10 s lang Druck und Lautstaerke messen und wie auf einem Oszilloskop zeigen
      static uint32_t lastScope = 0;
      fill_solid(leds, n, CRGB::Black);
      uint16_t mv = sensors::hpMilliVolts();
      uint8_t lvl = sensors::micLevel();
      if (mv > hpMax) hpMax = mv;
      if (lvl > micMax) micMax = lvl;
      if (sensors::micOk()) micSeen = true;
      if (millis() - lastScope >= 100) {
        lastScope = millis();
        display::testScope(mv, lvl, t * 100 / HPMIC_MS);
      }
      if (t >= HPMIC_MS) {
        rHp = hpMax > HP_ON_MV ? "ok" : "fehlt";  // nicht (fest genug) gedrueckt oder nicht angeschlossen
        rMic = !micSeen ? "fehler" : micMax >= MIC_TEST_LEVEL ? "ok" : "fehlt";  // keine Daten / zu leise
        step = SENSOR;
        checkSensor();
        finish();
      }
      break;
    }
#endif
    case RESULT:
      // gruen bzw. rot leuchten lassen, damit man das Ergebnis auch von weitem sieht
      fill_solid(leds, n, passed ? CRGB(0, 60, 0) : CRGB(60, 0, 0));
      if (buttonDown && t > 400) btnSeen = true;  // nach 0,4 s (Prellen der letzten Taste); BOOT druecken und loslassen beendet ebenfalls (1,3-Zoll ohne Touch)
      if (touched || (btnSeen && !buttonDown) || t >= RESULT_MS) {
        step = IDLE;
        fill_solid(leds, n, CRGB::Black);
        display::testEnd();
      }
      break;
    default: break;
  }
}

String json() {
  String j = "{\"running\":" + String(running() ? "true" : "false");
  j += ",\"step\":\"" + String(stepName()) + "\"";
  j += ",\"done\":" + String(done ? "true" : "false");
  j += ",\"passed\":" + String(passed ? "true" : "false");
  j += ",\"touch\":\"" + rTouch + "\",\"button\":\"" + rButton + "\",\"sensor\":\"" + rSensor + "\"";
  j += ",\"g\":" + String(gValue, 2);
  j += ",\"hp\":\"" + rHp + "\",\"mic\":\"" + rMic + "\"";
  j += ",\"hpMax\":" + String(hpMax) + ",\"micMax\":" + String(micMax) + "}";
  return j;
}

}  // namespace selftest
