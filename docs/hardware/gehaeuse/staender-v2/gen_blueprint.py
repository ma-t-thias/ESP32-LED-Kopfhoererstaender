#!/usr/bin/env python3
"""Erzeugt blueprint.html: Maßzeichnung des Staenders v2 im Blueprint-Stil.

Die Ansichten (bilder/ansicht_*.png) sind exakt orthografisch, 4,0 px/mm, Bildmitte = Welt (0, 12, 137,5).
Das Skript legt Masslinien darueber. Aufruf (im Repo-Hauptordner): python3 docs/hardware/gehaeuse/staender-v2/gen_blueprint.py
Die Masse stammen aus der Baugruppe Staender_v10_Gesamt_B (Stand 22.11.2026).
"""
import os

PX = 4.0                      # Pixel pro mm
CX, CY, CZ = 0.0, 12.0, 137.5  # Weltpunkt in der Bildmitte
HERE = os.path.dirname(os.path.abspath(__file__))
LINE = "#ffe27a"               # Masslinien
HID = "#9fd0ff"                # verdeckte Teile (gestrichelt)
PAD = 150                      # Rand um das Bild fuer Masslinien (px)


class View:
    def __init__(self, name, w, h, fx, fy, title):
        self.name, self.w, self.h, self.fx, self.fy, self.title = name, w, h, fx, fy, title
        self.parts = []

    # Welt -> Bild
    def X(self, a):
        return self.w / 2 + self.fx(a) * PX

    def Y(self, b):
        return self.h / 2 + self.fy(b) * PX

    def add(self, s):
        self.parts.append(s)

    def hdim(self, a1, a2, b, text, off=60, side=1):
        """waagerechte Masslinie zwischen den Weltwerten a1/a2, Bezugskante bei b, Abstand off px"""
        x1, x2, yb = self.X(a1), self.X(a2), self.Y(b)
        y = yb + side * off
        self._ext(x1, yb, x1, y, side)
        self._ext(x2, yb, x2, y, side)
        self._line(x1, y, x2, y, text, False)

    def vdim(self, b1, b2, a, text, off=60, side=1):
        y1, y2, xb = self.Y(b1), self.Y(b2), self.X(a)
        x = xb + side * off
        self._ext(xb, y1, x, y1, side)
        self._ext(xb, y2, x, y2, side)
        self._line(x, y1, x, y2, text, True)

    def _ext(self, x1, y1, x2, y2, side):
        dx, dy = x2 - x1, y2 - y1
        n = (dx * dx + dy * dy) ** 0.5 or 1
        ux, uy = dx / n, dy / n
        self.add(f'<line x1="{x1 + ux * 6:.1f}" y1="{y1 + uy * 6:.1f}" x2="{x2 + ux * 14:.1f}" y2="{y2 + uy * 14:.1f}" class="ext"/>')

    def _line(self, x1, y1, x2, y2, text, vert):
        self.add(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" class="dim" marker-start="url(#a)" marker-end="url(#a)"/>')
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        if vert:
            self.add(f'<text x="{mx - 12:.1f}" y="{my:.1f}" class="t" transform="rotate(-90 {mx - 12:.1f} {my:.1f})" text-anchor="middle">{text}</text>')
        else:
            self.add(f'<text x="{mx:.1f}" y="{my - 10:.1f}" class="t" text-anchor="middle">{text}</text>')

    def rect(self, a1, a2, b1, b2, cls="hid"):
        xa, xb = sorted((self.X(a1), self.X(a2)))
        ya, yb = sorted((self.Y(b1), self.Y(b2)))
        self.add(f'<rect x="{xa:.1f}" y="{ya:.1f}" width="{xb - xa:.1f}" height="{yb - ya:.1f}" class="{cls}"/>')

    def circle(self, a, b, r, cls="hid"):
        self.add(f'<circle cx="{self.X(a):.1f}" cy="{self.Y(b):.1f}" r="{r * PX:.1f}" class="{cls}"/>')

    def label(self, a, b, text, dx=0, dy=0, anchor="start"):
        self.add(f'<text x="{self.X(a) + dx:.1f}" y="{self.Y(b) + dy:.1f}" class="n" text-anchor="{anchor}">{text}</text>')

    def leader(self, a, b, dx, dy, text):
        x, y = self.X(a), self.Y(b)
        self.add(f'<line x1="{x:.1f}" y1="{y:.1f}" x2="{x + dx:.1f}" y2="{y + dy:.1f}" class="ext"/>')
        anchor = "start" if dx >= 0 else "end"
        self.add(f'<text x="{x + dx + (8 if dx >= 0 else -8):.1f}" y="{y + dy + 6:.1f}" class="n" text-anchor="{anchor}">{text}</text>')

    def svg(self):
        body = "\n    ".join(self.parts)
        return f'''<svg viewBox="{-PAD} {-PAD} {self.w + 2 * PAD} {self.h + 2 * PAD}" role="img" aria-label="{self.title}">
    <image href="bilder/{self.name}" width="{self.w}" height="{self.h}" class="img"/>
    {body}
  </svg>'''


# --- Vorne: Bild rechts = -x, oben = +z
vorne = View("ansicht_vorne.png", 1600, 1300, lambda x: -(x - CX), lambda z: -(z - CZ), "Ansicht von vorn")
vorne.hdim(-92, 92, -4, "184", off=70, side=1)
vorne.vdim(-4, 279, -92, "283", off=-90, side=1)
vorne.hdim(-25, 25, 279, "50", off=50, side=-1)
vorne.hdim(-23, 23, 150, "46", off=0, side=1)
vorne.vdim(28, 233, 23, "205 (Säule)", off=80, side=-1)
vorne.vdim(213, 279, -25, "66 (Auflage)", off=90, side=1)
vorne.vdim(0, 66.9, 92, "66,9 (Turm)", off=70, side=1)
vorne.leader(-55, 14, 120, 110, "Augen-Fenster (3 je Seite)")
vorne.leader(0, 45, -330, -160, "Display-Turm")

# --- Seite: Bild rechts = +y, oben = +z
seite = View("ansicht_seite.png", 1600, 1300, lambda y: (y - CY), lambda z: -(z - CZ), "Ansicht von der Seite")
seite.hdim(-66, 90, -4, "156", off=70, side=1)
seite.vdim(-4, 279, -66, "283", off=-90, side=1)
seite.hdim(18.5, 81.5, 279, "63 (Auflage)", off=50, side=-1)
seite.hdim(-49.1, 33.5, 233, "82,6 (Säule unten)", off=-60, side=-1)
seite.vdim(0, 28, 90, "28 (Deck)", off=60, side=1)
seite.vdim(213, 279, 81.5, "66", off=50, side=1)
seite.leader(-58, 10, -60, 120, "USB-C (hinten)")
seite.leader(76, 52, 150, -90, "Display, 20° nach hinten")

# --- Oben: Bild rechts = +x, oben = +y
oben = View("ansicht_oben.png", 1600, 1000, lambda x: (x - CX), lambda y: -(y - CY), "Ansicht von oben")
oben.hdim(-92, 92, -66, "184", off=60, side=1)
oben.vdim(-66, 90, -92, "156", off=-60, side=1)
oben.hdim(-25, 25, 81.5, "50 (Auflage)", off=45, side=-1)
oben.vdim(18.5, 81.5, 25, "63", off=90, side=1)
oben.rect(-32, 32, -22, 30)
oben.label(0, -27, "Platine v2 64 × 52 (verdeckt)", anchor="middle")
oben.circle(17.04, 22.69, 5)
oben.leader(17.04, 22.69, 150, -90, "Mikrofon INMP441 (x 17,04 / y 22,69)")
oben.rect(-14.4, 14.4, 61, 82)
oben.leader(14.4, 78, 150, -60, "Display-Fenster 28,8 × 21")

# --- Unten: Bild rechts = +x, oben = -y (also +y nach unten)
unten = View("ansicht_unten.png", 1600, 1000, lambda x: (x - CX), lambda y: (y - CY), "Ansicht von unten")
unten.hdim(-91.5, 91.5, 89.5, "183 (Boden)", off=45, side=1)
unten.vdim(-65.5, 89.5, -91.5, "155", off=-60, side=1)
for sx in (-29, 29):
    for yy in (-19, 21):
        unten.circle(sx, yy, 3.2, "dome")
unten.hdim(-29, 29, -19, "58", off=60, side=-1)
unten.vdim(-19, 21, 29, "40", off=60, side=1)
unten.leader(29, 21, 120, 60, "4 Dome M2,5, Kernloch Ø2,2")
unten.rect(-4.5, 4.5, -61.4, -54)
unten.leader(0, -58, -150, -70, "USB-C-Buchse")

VIEWS = [vorne, seite, oben, unten]
TITLES = ["Vorderansicht (Blick auf das Display)", "Seitenansicht (von rechts)", "Draufsicht", "Untersicht (Blick auf den Boden)"]

html = f'''<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Blueprint Ständer v2</title>
<!-- Erzeugt mit python3 docs/hardware/gehaeuse/staender-v2/gen_blueprint.py, nicht von Hand ändern. -->
<style>
:root{{--bg:#0d3b7a;--grid:rgba(255,255,255,.07);--ink:#eaf3ff;--dim:{LINE};--hid:{HID}}}
*{{box-sizing:border-box}}
body{{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
 background-image:linear-gradient(var(--grid) 1px,transparent 1px),linear-gradient(90deg,var(--grid) 1px,transparent 1px);background-size:24px 24px}}
main{{max-width:1180px;margin:0 auto;padding:24px 16px 56px}}
h1{{font-size:1.6rem;margin:0 0 4px;letter-spacing:.02em}}
.sub{{margin:0 0 20px;opacity:.85}}
.sheet{{border:2px solid var(--ink);margin:0 0 22px;background:rgba(5,30,70,.35)}}
.sheet h2{{margin:0;padding:8px 14px;font-size:.95rem;font-weight:600;border-bottom:1px solid var(--ink);display:flex;justify-content:space-between;gap:12px}}
.sheet h2 span{{opacity:.8;font-weight:400}}
svg{{display:block;width:100%;height:auto}}
.img{{filter:invert(1);mix-blend-mode:screen;opacity:.92}}
.dim{{stroke:var(--dim);stroke-width:2}}
.ext{{stroke:var(--dim);stroke-width:1.2;opacity:.85}}
.t{{fill:var(--dim);font:600 26px ui-monospace,Menlo,Consolas,monospace;paint-order:stroke;stroke:#0d3b7a;stroke-width:6px}}
.n{{fill:var(--ink);font:22px ui-monospace,Menlo,Consolas,monospace;paint-order:stroke;stroke:#0d3b7a;stroke-width:5px}}
.hid{{fill:none;stroke:var(--hid);stroke-width:2;stroke-dasharray:10 7}}
.dome{{fill:none;stroke:var(--hid);stroke-width:2}}
table{{border-collapse:collapse;width:100%;font-size:.92rem}}
td,th{{border:1px solid rgba(255,255,255,.45);padding:6px 10px;text-align:left}}
th{{background:rgba(255,255,255,.1)}}
.block{{border:2px solid var(--ink);padding:14px;margin:0 0 22px;background:rgba(5,30,70,.35)}}
.block h2{{margin:0 0 10px;font-size:1rem}}
.foot{{font-size:.85rem;opacity:.8}}
@media (prefers-reduced-motion:reduce){{*{{transition:none!important}}}}
</style>
</head>
<body>
<main>
<h1>Blueprint · Kopfhörerständer v2</h1>
<p class="sub">Maßzeichnung der Baugruppe Staender_v10_Gesamt_B, Stand 22.11.2026 (Sockel v9, Auflage v4, Säule B „Rauten“). Alle Maße in mm. Gelb = Maße, gestrichelt = verdeckte Teile. Anleitung zum Aufbau: <a style="color:#ffe27a" href="README.md">README.md</a>.</p>
'''
for v, t in zip(VIEWS, TITLES):
    html += f'''<section class="sheet">
  <h2>{t}<span>Maßstab im Bild 4,0 px/mm</span></h2>
  {v.svg()}
</section>
'''
html += '''<section class="block">
<h2>Wichtige Maße</h2>
<table>
<tr><th>Teil</th><th>B × T × H (mm)</th><th>Hinweis</th></tr>
<tr><td>Baugruppe</td><td>184 × 156 × 283</td><td>x −92…92, y −66…90, z −4…279 (z −4 = Unterkante TPU-Fuß, 0 = Unterseite Boden)</td></tr>
<tr><td>Sockel-Schale v9</td><td>180 × 152 × 63,9</td><td>z 3…66,9, Deckoberseite z 28, Display-Turm bis z 66,9</td></tr>
<tr><td>Boden v5</td><td>183 × 155 × 11,5</td><td>4 Dome M2,5 für die Platine v2</td></tr>
<tr><td>Leuchtring v3</td><td>184 × 156 × 6</td><td>rundum, von unten sichtbar</td></tr>
<tr><td>Säule B „Rauten“</td><td>46 × 82,6 × 205</td><td>gebogen, z 28…233, Front-Streifen 19 breit</td></tr>
<tr><td>Auflage v4</td><td>50 × 63 × 66</td><td>z 213…279, Außenkanten R 2</td></tr>
<tr><td>Druckplatte FSR</td><td>Ø19 × 4</td><td>plus 2 Nasen 3 × 0,9 × 0,4</td></tr>
<tr><td>Platine v2</td><td>64 × 52 × 1,6</td><td>Oberseite z 8,5, Lochabstand 58 × 40</td></tr>
<tr><td>ESP32-C6-Touch-LCD-1.9</td><td>x −15…14,5, y 48,6…76, z 10…62</td><td>hochkant im Turm, Display-Glas x ±14,4, y 61…82, z 14…63</td></tr>
<tr><td>Mikrofon INMP441</td><td>Ø14 (Modul)</td><td>Mitte x 17,04 / y 22,69, Oberkante z 12,2, 4 Schalllöcher Ø2,0 auf Lochkreis R 2,4</td></tr>
<tr><td>USB-C-Buchse</td><td>x ±4,5, y −61,4…−54, z 10,25…13,45</td><td>Durchbruch in der Schale 13 × 7,2</td></tr>
<tr><td>FSR-Tasche (Auflage)</td><td>Ø19,4 × 5,25 tief</td><td>Mitte x 0 / y 49,0, Boden z 245,85</td></tr>
</table>
<p class="foot">Die Ansichten sind aus den STL-Teilen in Montagelage berechnet (Kanten, ohne Schatten). Die Maße gelten für die Konstruktion, nicht für Druck-Toleranzen.</p>
</section>
</main>
</body>
</html>
'''
# Marker fuer die Pfeilspitzen
marker = '<svg width="0" height="0" style="position:absolute"><defs><marker id="a" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="9" markerHeight="9" orient="auto-start-reverse"><path d="M0,1 L10,5 L0,9 z" fill="#ffe27a"/></marker></defs></svg>\n'
html = html.replace("<main>", marker + "<main>", 1)
out = os.path.join(HERE, "blueprint.html")
with open(out, "w", encoding="utf-8") as f:
    f.write(html)
print("geschrieben:", out, len(html), "Bytes")
