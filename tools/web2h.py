#!/usr/bin/env python3
"""Packt web/index.html als INDEX_HTML in src/webui_index.h.

Nach jeder Aenderung an web/index.html einmal ausfuehren:
    python tools/web2h.py
Die Datei web/index.html laesst sich auch direkt im Browser oeffnen; ohne Geraet
zeigt sie dann allerdings keine Daten.
"""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
html = (root / "web" / "index.html").read_text(encoding="utf-8")
if ')HTML"' in html:
    raise SystemExit('web/index.html darf die Zeichenfolge )HTML" nicht enthalten')
out = root / "src" / "webui_index.h"
out.write_text(
    "#pragma once\n"
    "// Automatisch erzeugt aus web/index.html mit tools/web2h.py. Nicht von Hand aendern.\n"
    "#include <Arduino.h>\n\n"
    'static const char INDEX_HTML[] PROGMEM = R"HTML(' + html + ')HTML";\n',
    encoding="utf-8",
)
print(f"{out.relative_to(root)}: {len(html.encode('utf-8'))} Bytes")
