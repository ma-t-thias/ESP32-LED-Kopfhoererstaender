# PlatformIO-Skript (post): Aufgabe "Update-Datei erzeugen" fuer das Web-Update.
#   update/LED-Kopfhoerer_<Board>_<Tag>.bin   z. B. LED-Kopfhoerer_LCD19_V1.0.0.bin
# Die Datei ist die normale firmware.bin aus .pio/build/<env>/, nur mit sprechendem Namen.
# Die Board-Kennung (LCD19/LCD13) muss im Namen bleiben: das Web-Update prueft sie.
#
# Nur fuer Releases: Die Datei entsteht nur, wenn der aktuelle Commit einen Tag hat und es
# keine nicht eingecheckten Aenderungen gibt. So ist jeder Stand im Web-Update nachvollziehbar.
# Prototypen weiter wie gewohnt mit Build/Upload per USB oder WLAN (test_ota.ini) flashen.
#
# In VS Code: PlatformIO -> Project Tasks -> <env> -> Custom -> "Update-Datei erzeugen"
import os
import re
import shutil
import subprocess

Import("env")  # noqa: F821  (stellt PlatformIO bereit)


def git(*args):
    try:
        r = subprocess.run(["git", *args], cwd=env["PROJECT_DIR"], capture_output=True, text=True, timeout=15)  # noqa: F821
        return r.returncode, r.stdout.strip()
    except Exception:
        return 1, ""


def board_tag():
    defines = [d if isinstance(d, str) else d[0] for d in env.get("CPPDEFINES", [])]  # noqa: F821
    flags = " ".join(str(f) for f in env.get("BUILD_FLAGS", []))  # noqa: F821
    return "LCD13" if "BOARD_LCD_13" in defines or "-DBOARD_LCD_13" in flags else "LCD19"


def firmware_version():
    path = os.path.join(env["PROJECT_DIR"], "include", "fw_version.h")  # noqa: F821
    try:
        m = re.search(r'FW_VERSION "([^"]*)"', open(path, encoding="utf-8").read())
        return m.group(1) if m else ""
    except OSError:
        return ""


def stop(text):
    print("\n*** Keine Update-Datei erzeugt ***\n" + text + "\n")
    return 1  # Aufgabe schlaegt fehl, es entsteht keine Datei


def make_update_file(target, source, env):
    code, tag = git("describe", "--tags", "--exact-match", "HEAD")
    if code != 0 or not tag:
        return stop(
            "Der aktuelle Commit hat keinen Tag. Update-Dateien gibt es nur fuer getaggte Staende.\n"
            "Erst einchecken und taggen, z. B. in Fork: Rechtsklick auf den Commit -> \"New Tag...\",\n"
            "oder im Terminal:  git tag V1.0.0  und  git push origin V1.0.0\n"
            "Prototypen weiter per Build/Upload ueber USB oder WLAN flashen."
        )
    code, changes = git("status", "--porcelain", "--untracked-files=no")
    if code != 0 or changes:
        return stop(
            f"Es gibt nicht eingecheckte Aenderungen. Der Stand entspricht nicht dem Tag {tag}.\n"
            "Erst einchecken (und neu taggen) oder die Aenderungen verwerfen."
        )
    version = firmware_version()
    if version != re.sub(r"[^A-Za-z0-9._+/-]", "_", tag):
        return stop(
            f"Die gebaute Firmware hat die Version \"{version}\", der Tag ist \"{tag}\".\n"
            "Bitte einmal Clean und dann diese Aufgabe erneut ausfuehren."
        )
    code, remote = git("ls-remote", "--tags", "origin", tag)
    if code != 0 or not remote:
        print(f"Hinweis: Tag {tag} ist (noch) nicht auf GitHub. Bitte mit  git push origin {tag}  hochladen.")

    src = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    out_dir = os.path.join(env["PROJECT_DIR"], "update")
    os.makedirs(out_dir, exist_ok=True)
    dest = os.path.join(out_dir, f"LED-Kopfhoerer_{board_tag()}_{version.replace('/', '_')}.bin")
    shutil.copyfile(src, dest)
    print(f"\nUpdate-Datei fuer Version {tag}: {dest}\n")
    return 0


env.AddCustomTarget(  # noqa: F821
    name="update_file",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=[make_update_file],
    title="Update-Datei erzeugen",
    description="Nur fuer getaggte Staende: legt update/LED-Kopfhoerer_<Board>_<Tag>.bin an",
)
