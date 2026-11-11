# PlatformIO-Skript (pre): schreibt die Versionskennung nach include/fw_version.h.
# Kennung = letzter Git-Tag + Commit-Hash, z. B. "Prototyp_001-3-1a2b3c4".
#   Prototyp_001          genau auf dem Tag gebaut
#   Prototyp_001-3-1a2b3c4   3 Commits nach dem Tag, Commit 1a2b3c4
#   ...-dirty             mit nicht eingecheckten Aenderungen gebaut
# Die Datei wird nur neu geschrieben, wenn sich die Kennung aendert (spart Neuuebersetzen).
import os
import re
import subprocess

Import("env")  # noqa: F821  (stellt PlatformIO bereit)


def git(*args):
    try:
        return subprocess.check_output(["git", *args], cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL).decode().strip()  # noqa: F821
    except Exception:
        return ""


version = git("describe", "--tags", "--always", "--dirty", "--abbrev=7") or "unbekannt"
version = re.sub(r"-g([0-9a-f]{7})", r"-\1", version)  # kuerzer fuers Display: "-3-1a2b3c4"
version = re.sub(r"[^A-Za-z0-9._+/-]", "_", version)  # nur unkritische Zeichen (C-String, JSON, HTML)
text = (
    "// Automatisch erzeugt von tools/version.py - nicht von Hand aendern, nicht einchecken.\n"
    "#pragma once\n"
    f'#define FW_VERSION "{version}"\n'
)
path = os.path.join(env["PROJECT_DIR"], "include", "fw_version.h")  # noqa: F821
old = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
if old != text:
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
print("Firmware-Version:", version)
