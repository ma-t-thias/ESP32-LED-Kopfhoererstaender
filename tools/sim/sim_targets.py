# env:sim: Aufgaben in VS Code unter PlatformIO -> env:sim -> Custom
#   "Simulation in Blender starten"  baut die Simulation, erzeugt das Display-Bild und
#                                    öffnet Blender mit Ständer und Bedienfeld (Reiter "LED")
#   "Blender-Szene neu bauen"        baut tools/blender/Staender_v8.blend aus tools/blender/stl/
# Blender wird gesucht: Umgebungsvariable BLENDER, dann PATH, dann
# "C:\Program Files\Blender Foundation\Blender x.y\blender.exe" (neueste Version).
# Store-/Steam-/ZIP-Installationen: Umgebungsvariable BLENDER auf blender.exe setzen.
import glob
import os
import re
import shutil
import subprocess

Import("env")

PROJ = env.subst("$PROJECT_DIR")
BL_DIR = os.path.join(PROJ, "tools", "blender")
BLEND = os.path.join(BL_DIR, "Staender_v8.blend")
STL = os.path.join(BL_DIR, "stl")
PNG = os.path.join(BL_DIR, "Display_Grundbild.png")


def blender_exe():
    p = os.environ.get("BLENDER") or shutil.which("blender")
    if p:
        return p
    found = glob.glob(r"C:\Program Files\Blender Foundation\Blender *\blender.exe")

    def version(path):
        m = re.search(r"Blender (\d+)\.(\d+)", path)
        return (int(m.group(1)), int(m.group(2))) if m else (0, 0)

    return max(found, key=version) if found else None


def untertitel():
    try:
        return subprocess.run(["git", "describe", "--tags", "--always"], cwd=PROJ, capture_output=True,
                              text=True, timeout=10).stdout.strip()
    except Exception:
        return ""


def display_bild(prog):
    rc = subprocess.run([prog, "logo", PNG, "0", untertitel()]).returncode
    if rc != 0:
        print("Display-Bild konnte nicht erzeugt werden")
    return rc == 0


def szene_bauen(source, target, env):
    exe = blender_exe()
    if not exe:
        print("Blender nicht gefunden. Installieren oder Umgebungsvariable BLENDER setzen.")
        return 1
    if not os.path.isdir(STL):
        print(f"STL-Ordner fehlt: {STL}\nDort die Teile aus SolidCraft ablegen (siehe tools/blender/README.md).")
        return 1
    display_bild(env.subst("$PROG_PATH"))
    print(f"Baue Szene mit {exe} ...")
    vorher = os.path.getmtime(BLEND) if os.path.isfile(BLEND) else 0
    # --python-exit-code: sonst meldet Blender auch bei einem Fehler im Skript "fertig"
    rc = subprocess.run([exe, "--background", "--python-exit-code", "1",
                         "--python", os.path.join(BL_DIR, "build_staender_blend.py"),
                         "--", "--stl-dir", STL, "--out", BLEND, "--display-png", PNG]).returncode
    if rc == 0 and (not os.path.isfile(BLEND) or os.path.getmtime(BLEND) <= vorher):
        print(f"Szene wurde nicht geschrieben: {BLEND}")
        return 1
    return rc


def blender_starten(source, target, env):
    exe = blender_exe()
    if not exe:
        print("Blender nicht gefunden. Installieren oder Umgebungsvariable BLENDER setzen.")
        return 1
    if not os.path.isfile(BLEND):
        print(f"Szene fehlt: {BLEND}\nZuerst die Aufgabe \"Blender-Szene neu bauen\" ausführen.")
        return 1
    display_bild(env.subst("$PROG_PATH"))
    # Blender eigenständig starten: nicht an das Terminal von VS Code gebunden (sonst schließt es
    # mit dem Terminal), die Aufgabe ist danach sofort fertig. Meldungen von Blender stehen dann
    # in Blender unter Fenster -> Systemkonsole umschalten.
    cmd = [exe, BLEND, "--python", os.path.join(BL_DIR, "live_leds.py"), "--", "sim"]
    if os.name == "nt":
        subprocess.Popen(cmd, cwd=PROJ, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, close_fds=True,
                         creationflags=0x00000008 | 0x00000200)  # DETACHED_PROCESS | NEW_PROCESS_GROUP
    else:
        subprocess.Popen(cmd, cwd=PROJ, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL, close_fds=True, start_new_session=True)
    print("Blender startet. Bedienfeld: in der 3D-Ansicht Taste N, Reiter \"LED\".")
    return 0


env.AddCustomTarget(
    name="blender",
    dependencies="$PROG_PATH",
    actions=blender_starten,
    title="Simulation in Blender starten",
    description="Baut die Simulation und öffnet den Ständer in Blender mit laufenden Effekten",
    always_build=True,
)
env.AddCustomTarget(
    name="blender_szene",
    dependencies="$PROG_PATH",
    actions=szene_bauen,
    title="Blender-Szene neu bauen",
    description="Baut tools/blender/Staender_v8.blend aus den STL-Dateien in tools/blender/stl",
    always_build=True,
)
