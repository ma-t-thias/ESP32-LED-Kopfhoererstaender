"""LED-Farben live in Blender anzeigen: vom echten Ständer oder aus der PC-Simulation.

Färbt die Objekte LED_000, LED_001, ... der Szene (gebaut mit build_staender_blend.py)
etwa 25-mal pro Sekunde ein. Reihenfolge = Reihenfolge am LED-Datenausgang.

Bedienung: In der 3D-Ansicht rechts die Seitenleiste öffnen (Taste N), Reiter "LED".
Dort Quelle wählen (Simulation oder Ständer), Start/Stopp, und bei der Simulation
Effekt, Palette, Tempo, Stärke und Farben per Klick. Anleitung: docs/simulation.md

Ohne Bedienfeld (z. B. aus dem VS Code-Terminal im Projektordner):
  blender Staender_v8.blend --python tools/blender/live_leds.py -- sim effect 5 speed 200
  blender Staender_v8.blend --python tools/blender/live_leds.py -- 192.168.1.159
Quelle "sim" = PC-Simulation .pio/build/sim/program(.exe) (vorher env:sim bauen),
sonst IP/Name des Ständers (liest /api/live, ohne Passwort).
In Blenders Python-Konsole geht auch: led_sim("effect 24"), led_sim("palette 3").
"""

import os
import subprocess
import sys
import threading
import urllib.request

import bpy

QUELLE = ""              # "" = nur Bedienfeld, Start dort; "sim" oder IP = sofort starten
INTERVAL = 0.04          # Sekunden zwischen zwei Aktualisierungen in Blender
TIMEOUT = 1.0
SIM_ARGS = []            # Startwerte für die Simulation, z. B. ["effect", "5"]

SIM_PFAD = ""            # optional: fester Pfad zu program(.exe), sonst wird gesucht

# Argumente nach "--" nur beim ersten Lauf nehmen: sys.argv bleibt die ganze Blender-Sitzung
# gleich, sonst würde ein späteres "Run Script" mit QUELLE = "" die Simulation wieder starten.
if "--" in sys.argv and not bpy.app.driver_namespace.get("led_live_argv"):
    bpy.app.driver_namespace["led_live_argv"] = True
    _args = sys.argv[sys.argv.index("--") + 1:]
    if _args:
        QUELLE, SIM_ARGS = _args[0], _args[1:]


def _startordner():
    """Mögliche Orte im Projekt. Bei "Run Script" im Texteditor ist __file__ nicht der echte
    Pfad (sondern <.blend>/<Textname>), deshalb auch den Pfad des Textes und der .blend prüfen."""
    orte = []
    try:
        orte.append(os.path.dirname(os.path.abspath(__file__)))
    except NameError:
        pass
    for t in bpy.data.texts:
        if t.filepath and os.path.basename(bpy.path.abspath(t.filepath)) == "live_leds.py":
            orte.append(os.path.dirname(bpy.path.abspath(t.filepath)))
    if bpy.data.filepath:
        orte.append(os.path.dirname(bpy.data.filepath))
    orte.append(os.getcwd())
    return orte


def _sim_programm():
    if SIM_PFAD:
        return SIM_PFAD if os.path.isfile(SIM_PFAD) else None
    for ort in _startordner():
        d = ort
        while True:  # nach oben bis zum Projektordner (dort liegt platformio.ini)
            if os.path.isfile(os.path.join(d, "platformio.ini")):
                for name in ("program.exe", "program"):
                    p = os.path.join(d, ".pio", "build", "sim", name)
                    if os.path.isfile(p):
                        return p
                break
            oben = os.path.dirname(d)
            if oben == d:
                break
            d = oben
        for name in ("program.exe", "program"):  # Kopie direkt neben Skript oder .blend
            p = os.path.join(ort, name)
            if os.path.isfile(p):
                return p
    return None


# Windows: kein schwarzes Konsolenfenster für die Simulation aufklappen
_KEIN_FENSTER = 0x08000000 if sys.platform == "win32" else 0


def _leds():
    objs = [o for o in bpy.data.objects if o.name.startswith("LED_") and o.name[4:].isdigit()]
    return sorted(objs, key=lambda o: int(o.name[4:]))


# Umrechnung 0..255 (sRGB) -> linear, damit die Farben wie am Streifen aussehen
def _srgb_zu_linear(v):
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def _linear_zu_srgb(v):
    return v * 12.92 if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055


_LIN = [_srgb_zu_linear(v / 255) for v in range(256)]


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
                                   "oder SIM_PFAD setzen")
            self.proc = subprocess.Popen([prog, *SIM_ARGS], stdin=subprocess.PIPE,
                                         stdout=subprocess.PIPE, text=True, bufsize=1,
                                         creationflags=_KEIN_FENSTER)
            threading.Thread(target=self._read_sim, daemon=True).start()
        else:
            threading.Thread(target=self._read_http, daemon=True).start()
        bpy.app.timers.register(self._apply, first_interval=INTERVAL, persistent=True)

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
        # bis zum Ende lesen, auch nach stop(): sonst kann die Simulation beim Schreiben hängen
        for line in self.proc.stdout:
            if not self.run:
                continue
            if line.startswith("#"):        # Antwort auf "list"
                print(line.rstrip())
            else:
                self.data = line.strip()
        self.proc.stdout.close()
        if self.run:
            print(f"LED-Live: Simulation beendet (Code {self.proc.wait()})")

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
                    rgb = (_LIN[int(h[0:2], 16)], _LIN[int(h[2:4], 16)], _LIN[int(h[4:6], 16)])
                except ValueError:
                    break
                leds[i].color = (*rgb, 1.0)
                for kind in leds[i].children:    # Punktlicht der LED direkt mitfärben
                    if kind.type == "LIGHT":
                        kind.data.color = rgb
        return INTERVAL

    def stop(self):
        self.run = False
        if self.proc:
            try:
                self.proc.stdin.close()     # Simulation endet, wenn ihre Eingabe zugeht
                self.proc.wait(timeout=1)   # der Lese-Thread liest weiter, sie hängt also nicht
            except Exception:
                self.proc.kill()


def led_sim(cmd):
    """Befehl an die laufende Simulation, z. B. led_sim("effect 5")."""
    live = bpy.app.driver_namespace.get("led_live")
    if live:
        live.send(cmd)


def _ansicht():
    """3D-Ansicht so einstellen, dass man die LED-Farben sieht: Kamerablick, Eevee-Vorschau, ohne die
    vielen Licht-Symbole. Im Hintergrundbetrieb (ohne Fenster) passiert nichts."""
    wm = bpy.context.window_manager
    for win in (wm.windows if wm else []):
        for area in win.screen.areas:
            if area.type == "VIEW_3D":
                for sp in area.spaces:
                    if sp.type == "VIEW_3D":
                        # "Rendered" (Eevee): nur dort wirken die Punktlichter der LEDs
                        sp.shading.type = "RENDERED"
                        sp.overlay.show_extras = False   # Lichter, Kamera als Symbole
                        if bpy.context.scene.camera:
                            sp.region_3d.view_perspective = "CAMERA"


def start(quelle, args=None):
    old = bpy.app.driver_namespace.pop("led_live", None)
    if old:
        old.stop()
    if not quelle:
        print("LED-Live: gestoppt")
        _neu_zeichnen()
        return
    _ansicht()
    if args is not None:
        global SIM_ARGS
        SIM_ARGS = args
    bpy.app.driver_namespace["led_live"] = _Live(quelle)
    bpy.app.driver_namespace["led_sim"] = led_sim
    try:  # in Blenders Python-Konsole direkt aufrufbar machen
        import builtins
        builtins.led_sim = led_sim
    except Exception:
        pass
    print(f"LED-Live: Quelle {quelle}, {len(_leds())} LED-Objekte")
    _neu_zeichnen()


def laeuft():
    live = bpy.app.driver_namespace.get("led_live")
    return bool(live and live.run and (live.proc is None or live.proc.poll() is None))


def _neu_zeichnen():
    wm = bpy.context.window_manager
    for win in (wm.windows if wm else []):
        for area in win.screen.areas:
            if area.type == "VIEW_3D":
                area.tag_redraw()


# ----------------------------------------------------------------------------- Bedienfeld
# Namen der Effekte und Paletten fragt das Skript einmal bei der Simulation ab ("list"),
# damit die Auswahl immer zur Firmware passt. Blender braucht dauerhafte Listen für Auswahlfelder.
_EFFEKTE = []
_PALETTEN = []


def _namen_holen():
    prog = _sim_programm()
    if not prog:
        return
    try:
        out = subprocess.run([prog], input="list\nquit\n", capture_output=True, text=True,
                             timeout=5, creationflags=_KEIN_FENSTER).stdout
    except Exception as e:
        print("LED-Live: Namen nicht abrufbar:", e)
        return
    _EFFEKTE.clear()
    _PALETTEN.clear()
    for line in out.splitlines():
        teile = line.split(" ", 2)
        if len(teile) == 3 and teile[0] in ("#effect", "#palette"):
            ziel = _EFFEKTE if teile[0] == "#effect" else _PALETTEN
            ziel.append((teile[1], f"{teile[1]}  {teile[2]}", ""))


def _effekte(self, context):
    return _EFFEKTE or [("1", "1  (Simulation nicht gefunden)", "")]


def _paletten(self, context):
    return _PALETTEN or [("0", "0", "")]


def _senden(befehl):
    def update(self, context):
        if self.quelle == "SIM" and laeuft():
            led_sim(befehl(self))
    return update


def _hex(c):
    return "".join(f"{max(0, min(255, round(_linear_zu_srgb(max(0.0, v)) * 255))):02x}" for v in c[:3])


def _alle_werte(p):
    return ["effect", p.effekt, "palette", p.palette, "speed", str(p.tempo),
            "intensity", str(p.staerke), "color1", _hex(p.farbe1), "color2", _hex(p.farbe2)]


class LEDSIM_Props(bpy.types.PropertyGroup):
    quelle: bpy.props.EnumProperty(
        name="Quelle", items=[("SIM", "Simulation (PC)", "Effekte aus dem Firmware-Code, ohne Ständer"),
                              ("STAENDER", "Echter Ständer", "Farben live vom Ständer über WLAN")])
    host: bpy.props.StringProperty(name="Adresse", default="led-kopfhoerer.local",
                                   description="IP oder Name des Ständers, z. B. 192.168.1.159")
    effekt: bpy.props.EnumProperty(name="Effekt", items=_effekte,
                                   update=_senden(lambda p: f"effect {p.effekt}"))
    palette: bpy.props.EnumProperty(name="Palette", items=_paletten,
                                    update=_senden(lambda p: f"palette {p.palette}"))
    tempo: bpy.props.IntProperty(name="Tempo", min=0, max=255, default=128,
                                 update=_senden(lambda p: f"speed {p.tempo}"))
    staerke: bpy.props.IntProperty(name="Effekt-Stärke", min=0, max=255, default=128,
                                   update=_senden(lambda p: f"intensity {p.staerke}"))
    farbe1: bpy.props.FloatVectorProperty(name="Farbe 1", subtype="COLOR", min=0, max=1,
                                          default=(1.0, 0.19, 0.0),
                                          update=_senden(lambda p: f"color1 {_hex(p.farbe1)}"))
    farbe2: bpy.props.FloatVectorProperty(name="Farbe 2", subtype="COLOR", min=0, max=1,
                                          default=(0.0, 0.08, 1.0),
                                          update=_senden(lambda p: f"color2 {_hex(p.farbe2)}"))


class LEDSIM_OT_start(bpy.types.Operator):
    bl_idname = "ledsim.start"
    bl_label = "Start"
    bl_description = "LED-Anzeige starten"

    def execute(self, context):
        p = context.scene.ledsim
        try:
            if p.quelle == "SIM":
                if not _EFFEKTE:
                    _namen_holen()
                start("sim", _alle_werte(p))
            else:
                if not p.host.strip():
                    self.report({"ERROR"}, "Adresse des Ständers fehlt")
                    return {"CANCELLED"}
                start(p.host.strip())
        except RuntimeError as e:
            self.report({"ERROR"}, str(e))
            return {"CANCELLED"}
        return {"FINISHED"}


class LEDSIM_OT_stop(bpy.types.Operator):
    bl_idname = "ledsim.stop"
    bl_label = "Stopp"
    bl_description = "LED-Anzeige anhalten"

    def execute(self, context):
        start("")
        return {"FINISHED"}


class LEDSIM_PT_panel(bpy.types.Panel):
    bl_label = "LED-Simulation"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "LED"

    def draw(self, context):
        p = context.scene.ledsim
        col = self.layout.column()
        col.prop(p, "quelle")
        if p.quelle == "STAENDER":
            col.prop(p, "host")
        row = col.row(align=True)
        row.operator("ledsim.start", icon="PLAY")
        row.operator("ledsim.stop", icon="PAUSE")
        col.label(text="Läuft" if laeuft() else "Angehalten", icon="CHECKMARK" if laeuft() else "DOT")
        col.separator()
        if p.quelle == "SIM":
            col.prop(p, "effekt")
            col.prop(p, "palette")
            col.prop(p, "tempo", slider=True)
            col.prop(p, "staerke", slider=True)
            col.prop(p, "farbe1")
            col.prop(p, "farbe2")
        else:
            col.label(text="Effekt am Ständer über die")
            col.label(text="Weboberfläche oder das Display wählen.")


@bpy.app.handlers.persistent
def _beim_laden(*_):
    start("")  # andere Datei geöffnet: Simulation beenden statt im Hintergrund weiterlaufen


_KLASSEN = (LEDSIM_Props, LEDSIM_OT_start, LEDSIM_OT_stop, LEDSIM_PT_panel)


def registrieren():
    if hasattr(bpy.types.Scene, "ledsim"):  # erst den Verweis lösen, dann die Klassen
        del bpy.types.Scene.ledsim
    for k in reversed(_KLASSEN):  # bei erneutem "Run Script" erst die alte Fassung entfernen
        alt = getattr(bpy.types, k.__name__, None)
        if alt is not None:
            try:
                bpy.utils.unregister_class(alt)
            except RuntimeError:
                pass
    for k in _KLASSEN:
        bpy.utils.register_class(k)
    bpy.types.Scene.ledsim = bpy.props.PointerProperty(type=LEDSIM_Props)
    for h in list(bpy.app.handlers.load_pre):
        if getattr(h, "__name__", "") == "_beim_laden":
            bpy.app.handlers.load_pre.remove(h)
    bpy.app.handlers.load_pre.append(_beim_laden)
    _namen_holen()


def _args_ins_bedienfeld(p, args):
    """Startwerte wie "effect 5 speed 200" ins Bedienfeld übernehmen, damit Anzeige und Simulation passen."""
    felder = {"effect": "effekt", "palette": "palette", "speed": "tempo", "intensity": "staerke",
              "color1": "farbe1", "color2": "farbe2"}
    for key, val in zip(args[0::2], args[1::2]):
        feld = felder.get(key)
        try:
            if feld in ("effekt", "palette"):
                setattr(p, feld, val)
            elif feld in ("tempo", "staerke"):
                setattr(p, feld, max(0, min(255, int(val))))
            elif feld:
                v = int(val, 16)
                setattr(p, feld, [_LIN[(v >> s) & 255] for s in (16, 8, 0)])
        except (TypeError, ValueError):
            print(f"LED-Live: Startwert {key} {val} ignoriert")


registrieren()
_p = bpy.context.scene.ledsim
if not bpy.app.driver_namespace.get("led_live_init"):
    bpy.app.driver_namespace["led_live_init"] = True
    if _EFFEKTE:
        _p.effekt = "1"  # Regenbogen: bewegt sich, man sieht sofort, dass es läuft
if QUELLE:
    if QUELLE == "sim":
        _p.quelle = "SIM"
        _args_ins_bedienfeld(_p, SIM_ARGS)
        start("sim", _alle_werte(_p))
    else:
        _p.quelle = "STAENDER"
        _p.host = QUELLE
        start(QUELLE)
