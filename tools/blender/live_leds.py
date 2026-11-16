"""LED-Farben live in Blender anzeigen: vom echten Ständer oder aus der PC-Simulation.

Färbt die Objekte LED_000, LED_001, ... der Szene (gebaut mit build_staender_blend.py)
etwa 20- bis 30-mal pro Sekunde ein. Reihenfolge = Reihenfolge am LED-Datenausgang.

Quelle (QUELLE unten oder als Argument nach "--"):
  "led-kopfhoerer.local" oder eine IP   echter Ständer, liest /api/live (ohne Passwort)
  "sim"                                 PC-Simulation .pio/build/sim/program(.exe),
                                        vorher in PlatformIO env:sim bauen
  ""                                    stoppen

Benutzung in Blender: Reiter "Scripting", diese Datei öffnen, QUELLE anpassen, "Run Script".
Von außen, z. B. im VS Code-Terminal im Projektordner:
  blender Staender_v8.blend --python tools/blender/live_leds.py -- 192.168.1.159
  blender Staender_v8.blend --python tools/blender/live_leds.py -- sim effect 5 speed 200

Bei der Simulation lassen sich Effekt & Co. danach in der Python-Konsole von Blender ändern:
  led_sim("effect 24")      # Effekt-Nummer wie in der Weboberfläche (0 = erster)
  led_sim("palette 3")      # außerdem: speed, intensity, bri, color1 ff7800, color2 0050ff
"""

import os
import subprocess
import sys
import threading
import urllib.request

import bpy

QUELLE = "led-kopfhoerer.local"
INTERVAL = 0.04          # Sekunden zwischen zwei Aktualisierungen in Blender
TIMEOUT = 1.0
SIM_ARGS = []            # Startwerte für die Simulation, z. B. ["effect", "5"]

if "--" in sys.argv:
    _args = sys.argv[sys.argv.index("--") + 1:]
    if _args:
        QUELLE, SIM_ARGS = _args[0], _args[1:]

# Projektordner = zwei Ebenen über dieser Datei (tools/blender/live_leds.py). In Blenders
# Texteditor ist __file__ der Pfad der geöffneten Datei; sonst das aktuelle Verzeichnis.
try:
    PROJEKT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
except NameError:
    PROJEKT = os.getcwd()


def _sim_programm():
    for name in ("program.exe", "program"):
        p = os.path.join(PROJEKT, ".pio", "build", "sim", name)
        if os.path.isfile(p):
            return p
    return None


def _leds():
    objs = [o for o in bpy.data.objects if o.name.startswith("LED_") and o.name[4:].isdigit()]
    return sorted(objs, key=lambda o: int(o.name[4:]))


# Umrechnung 0..255 (sRGB) -> linear, damit die Farben wie am Streifen aussehen
_LIN = [(v / 255) ** 2.2 for v in range(256)]


class _Live:
    def __init__(self, quelle):
        self.quelle = quelle
        self.run = True
        self.data = None
        self.error = None
        self.proc = None
        if quelle == "sim":
            prog = _sim_programm()
            if not prog:
                raise RuntimeError("Simulation nicht gefunden: erst in PlatformIO env:sim bauen "
                                   f"(gesucht in {os.path.join(PROJEKT, '.pio', 'build', 'sim')})")
            self.proc = subprocess.Popen([prog, *SIM_ARGS], stdin=subprocess.PIPE,
                                         stdout=subprocess.PIPE, text=True, bufsize=1)
            threading.Thread(target=self._read_sim, daemon=True).start()
        else:
            threading.Thread(target=self._read_http, daemon=True).start()
        bpy.app.timers.register(self._apply, first_interval=INTERVAL)

    def _read_http(self):
        url = f"http://{self.quelle}/api/live"
        wait = threading.Event()
        while self.run:
            try:
                with urllib.request.urlopen(url, timeout=TIMEOUT) as r:
                    self.data = r.read().decode("ascii", "ignore").strip()
                    self.error = None
            except Exception as e:  # Ständer aus, falsche IP, WLAN weg: weiter versuchen
                if str(e) != self.error:
                    print("LED-Live:", e)
                self.error = str(e)
            wait.wait(0.05)

    def _read_sim(self):
        for line in self.proc.stdout:
            if not self.run:
                break
            if line.startswith("#"):        # Antwort auf "list"
                print(line.rstrip())
            else:
                self.data = line.strip()

    def send(self, cmd):
        if self.proc and self.proc.poll() is None:
            self.proc.stdin.write(cmd.strip() + "\n")
            self.proc.stdin.flush()

    def _apply(self):
        if not self.run:
            return None
        data, self.data = self.data, None
        if data:
            leds = _leds()
            for i in range(min(len(leds), len(data) // 6)):
                h = data[i * 6:i * 6 + 6]
                try:
                    leds[i].color = (_LIN[int(h[0:2], 16)], _LIN[int(h[2:4], 16)],
                                     _LIN[int(h[4:6], 16)], 1.0)
                except ValueError:
                    break
        return INTERVAL

    def stop(self):
        self.run = False
        if self.proc:
            try:
                self.proc.stdin.close()     # Simulation endet, wenn ihre Eingabe zugeht
                self.proc.wait(timeout=2)
            except Exception:
                self.proc.kill()


def led_sim(cmd):
    """Befehl an die laufende Simulation, z. B. led_sim("effect 5")."""
    live = bpy.app.driver_namespace.get("led_live")
    if live:
        live.send(cmd)


def start(quelle):
    old = bpy.app.driver_namespace.pop("led_live", None)
    if old:
        old.stop()
    if not quelle:
        print("LED-Live: gestoppt")
        return
    bpy.app.driver_namespace["led_live"] = _Live(quelle)
    bpy.app.driver_namespace["led_sim"] = led_sim
    try:  # in Blenders Python-Konsole direkt aufrufbar machen
        import builtins
        builtins.led_sim = led_sim
    except Exception:
        pass
    print(f"LED-Live: Quelle {quelle}, {len(_leds())} LED-Objekte")


start(QUELLE)
