#!/usr/bin/env python3
"""Erzeugt die Seite mit Schaltplan, Breadboard-Belegung und Bauteilliste."""

P = 20            # Rasterabstand in px (= 2,54 mm)
X0 = 70           # x von Reihe 1
OY = 40           # Versatz nach unten fuer Elko/Beschriftung oben

def x(r):
    return X0 + (r - 1) * P

# y-Koordinaten der Lochreihen (j oben, a unten)
COLS = {"j": 100, "i": 120, "h": 140, "g": 160, "f": 180,
        "e": 240, "d": 260, "c": 280, "b": 300, "a": 320}
COLS = {k: v + OY for k, v in COLS.items()}
TOP_P, TOP_M = 40 + OY, 62 + OY      # obere Schiene: + aussen, - innen
BOT_P, BOT_M = 358 + OY, 380 + OY    # untere Schiene: + innen, - aussen

def hole(r, c):
    return x(r), COLS[c]

RED = "var(--plus)"
BLU = "var(--minus)"
YEL = "var(--data1)"
GRN = "var(--data2)"
PUR = "var(--data3)"

out = []
A = out.append

def wire(pts, color, w=3.2, dash=None):
    d = "M" + " L".join(f"{px},{py}" for px, py in pts)
    da = f' stroke-dasharray="{dash}"' if dash else ""
    A(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{w}" '
      f'stroke-linecap="round" stroke-linejoin="round"{da}/>')
    for px, py in (pts[0], pts[-1]):
        A(f'<circle cx="{px}" cy="{py}" r="3.2" fill="{color}"/>')

def curve(p1, p2, color, lift=26, w=3.2):
    (x1, y1), (x2, y2) = p1, p2
    A(f'<path d="M{x1},{y1} C{x1},{y1-lift} {x2},{y2-lift} {x2},{y2}" fill="none" '
      f'stroke="{color}" stroke-width="{w}" stroke-linecap="round"/>')
    for px, py in (p1, p2):
        A(f'<circle cx="{px}" cy="{py}" r="3.2" fill="{color}"/>')

def text(tx, ty, s, size=11, anchor="middle", weight=400, fill="currentColor", extra=""):
    A(f'<text x="{tx}" y="{ty}" font-size="{size}" text-anchor="{anchor}" '
      f'font-weight="{weight}" fill="{fill}" {extra}>{s}</text>')

W, H = 960, 470 + OY
A(f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Breadboard-Belegung: ESP32-C6 in Reihe 5 bis 10, '
  f'74AHCT125 in Reihe 16 bis 22, 330-Ohm-Widerstand zur Datenleitung, Netzteil und LED-Streifen rechts '
  f'außerhalb des Breadboards" class="bb" font-family="var(--mono)">')

# Breadboard-Koerper
bx1, bx2 = x(1) - 22, x(30) + 22
A(f'<rect x="{bx1}" y="{TOP_P-16}" width="{bx2-bx1}" height="{BOT_M-TOP_P+32}" rx="8" '
  f'fill="var(--board)" stroke="var(--line)"/>')
# Mittelkanal
A(f'<rect x="{bx1+6}" y="{COLS["f"]+14}" width="{bx2-bx1-12}" height="{COLS["e"]-COLS["f"]-28}" '
  f'rx="3" fill="var(--channel)"/>')
# Schienen-Linien
for yy, col, sign in ((TOP_P - 10, RED, "+"), (TOP_M + 10, BLU, "−"),
                      (BOT_P - 10, RED, "+"), (BOT_M + 10, BLU, "−")):
    A(f'<line x1="{x(1)-6}" y1="{yy}" x2="{x(30)+6}" y2="{yy}" stroke="{col}" stroke-width="1.4" opacity=".7"/>')
for yy, col, sign in ((TOP_P, RED, "+"), (TOP_M, BLU, "−"), (BOT_P, RED, "+"), (BOT_M, BLU, "−")):
    text(bx1 + 9, yy + 4, sign, 13, weight=700, fill=col)
# Loecher
for r in range(1, 31):
    for c, yy in COLS.items():
        A(f'<rect x="{x(r)-3}" y="{yy-3}" width="6" height="6" rx="1" fill="var(--hole)"/>')
    for yy in (TOP_P, TOP_M, BOT_P, BOT_M):
        A(f'<rect x="{x(r)-3}" y="{yy-3}" width="6" height="6" rx="1" fill="var(--hole)"/>')
# Beschriftung Reihen/Spalten
for r in (1, 5, 10, 15, 20, 25, 30):
    text(x(r), COLS["f"] + 30, r, 10, fill="var(--muted)")
for c, yy in COLS.items():
    text(bx1 + 9, yy + 4, c, 10, fill="var(--muted)")
    text(bx2 - 9, yy + 4, c, 10, fill="var(--muted)")

# --- ESP32-C6-LCD-1.3 (Display nach oben, USB-C Richtung Reihe 1) ---
ex1, ex2 = x(5) - 87, x(10) + 39
ey1, ey2 = COLS["i"] - 14, COLS["b"] + 14
A(f'<rect x="{ex1}" y="{ey1}" width="{ex2-ex1}" height="{ey2-ey1}" rx="6" fill="var(--pcb)" '
  f'fill-opacity=".88" stroke="var(--pcbedge)" stroke-width="1.5"/>')
# Display-Flaeche
A(f'<rect x="{ex1+14}" y="{ey1+30}" width="{ex2-ex1-28}" height="{ey2-ey1-60}" rx="3" fill="#0b0d12" opacity=".75"/>')
text((ex1 + ex2) / 2, (ey1 + ey2) / 2 - 6, "ESP32-C6", 12, weight=700, fill="#e8ecf2")
text((ex1 + ex2) / 2, (ey1 + ey2) / 2 + 10, "LCD 1.3″", 11, fill="#c9d1dc")
text((ex1 + ex2) / 2, (ey1 + ey2) / 2 + 26, "Display oben", 9.5, fill="#9aa6b5")
# USB-C
A(f'<rect x="{ex1-12}" y="{(ey1+ey2)/2-18}" width="18" height="36" rx="4" fill="#b9c0c9" stroke="#7d8692"/>')
text(ex1 + 20, ey1 + 48, "◀ USB-C", 9.5, anchor="start", fill="#9aa6b5")
# Header-Pins
top_labels = ["5V", "GND", "3V3", "IO1", "IO2", "IO3"]
bot_labels = ["12", "13", "16", "17", "20", "23"]
for k, r in enumerate(range(5, 11)):
    xx, yy = hole(r, "i")
    hl = k in (0, 1, 3)
    A(f'<rect x="{xx-5}" y="{yy-5}" width="10" height="10" rx="2" fill="{"#f4c542" if hl else "#c8a64a"}" stroke="#6b5a1f"/>')
    text(xx, yy + 20, top_labels[k], 9, weight=700 if hl else 400, fill="#ffffff" if hl else "#c9d1dc")
    xx, yy = hole(r, "b")
    A(f'<rect x="{xx-5}" y="{yy-5}" width="10" height="10" rx="2" fill="#c8a64a" stroke="#6b5a1f"/>')
    text(xx, yy - 11, bot_labels[k], 9, fill="#9aa6b5")

# --- 74AHCT125 (Kerbe Richtung Reihe 22) ---
cx1, cx2 = x(16) - 9, x(22) + 9
cy1, cy2 = COLS["f"] + 6, COLS["e"] - 6
A(f'<rect x="{cx1}" y="{cy1}" width="{cx2-cx1}" height="{cy2-cy1}" rx="3" fill="var(--ic)" stroke="#000" stroke-opacity=".4"/>')
A(f'<path d="M{cx2},{(cy1+cy2)/2-7} a7,7 0 0,0 0,14" fill="var(--board)"/>')
text((cx1 + cx2) / 2 - 4, (cy1 + cy2) / 2 + 4, "74AHCT125", 11, weight=700, fill="#e8ecf2")
A(f'<circle cx="{x(22)}" cy="{cy1+8}" r="2.6" fill="#e8ecf2"/>')
for r in range(16, 23):
    for c in ("f", "e"):
        xx, yy = hole(r, c)
        A(f'<rect x="{xx-4}" y="{yy-4}" width="8" height="8" fill="#b9c0c9"/>')
# Pinnummern
for i, r in enumerate(range(22, 15, -1)):
    text(x(r), COLS["f"] - 9, i + 1, 8.5, fill="var(--muted)")
for i, r in enumerate(range(16, 23)):
    text(x(r), COLS["e"] + 17, i + 8, 8.5, fill="var(--muted)")

# --- Verdrahtung oben (Spalten f-j) ---
# D1 Schottky: Anode an + Schiene (Reihe 5), Kathode (Ring) in j5
xa, ya = x(5), TOP_P
xk, yk = hole(5, "j")
A(f'<line x1="{xa}" y1="{ya}" x2="{xk}" y2="{yk}" stroke="#9aa3ad" stroke-width="2"/>')
A(f'<rect x="{xa-6}" y="{ya+14}" width="12" height="28" rx="3" fill="#1c1f24"/>')
A(f'<rect x="{xa-6}" y="{ya+34}" width="12" height="5" fill="#d7dbe0"/>')
for px, py in ((xa, ya), (xk, yk)):
    A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')
text(xa - 12, ya + 32, "D1", 10, anchor="end", weight=700)

# GND ESP: j6 -> - Schiene oben
wire([hole(6, "j"), (x(6), TOP_M)], BLU)
# GPIO1: j8 -> g21 (1A)
curve(hole(8, "j"), hole(21, "g"), YEL, lift=60)
# Pin 7 GND (f16) -> j16 -> -
wire([hole(16, "j"), (x(16), TOP_M)], BLU)
# Pin 5 2A (f18) -> j18 -> -
wire([hole(18, "j"), (x(18), TOP_M)], BLU)
# Pin 4 2OE (f19) -> j19 -> +
wire([hole(19, "j"), (x(19), TOP_P)], RED)
# Pin 1 1OE (f22) -> j22 -> -
wire([hole(22, "j"), (x(22), TOP_M)], BLU)
# R1 330 Ohm: h20 -> h25
rx1, ry = hole(20, "h")
rx2, _ = hole(25, "h")
A(f'<line x1="{rx1}" y1="{ry}" x2="{rx2}" y2="{ry}" stroke="#9aa3ad" stroke-width="2"/>')
A(f'<rect x="{rx1+24}" y="{ry-7}" width="{rx2-rx1-48}" height="14" rx="6" fill="#d9c49a" stroke="#8a7650"/>')
for i, col in enumerate(("#f58220", "#f58220", "#7a4a1d", "#c9a227")):
    A(f'<rect x="{rx1+32+i*8}" y="{ry-7}" width="3.5" height="14" fill="{col}"/>')
for px in (rx1, rx2):
    A(f'<circle cx="{px}" cy="{ry}" r="3" fill="#9aa3ad"/>')
text((rx1 + rx2) / 2, ry + 22, "R1 330 Ω", 10, weight=700)

# C1 Elko 1000 uF an oberer Schiene (Reihe 11/12)
ex, ey = (x(11) + x(12)) / 2, TOP_P - 30
A(f'<line x1="{x(11)}" y1="{TOP_P}" x2="{ex-5}" y2="{ey+10}" stroke="#9aa3ad" stroke-width="2"/>')
A(f'<line x1="{x(12)}" y1="{TOP_M}" x2="{ex+5}" y2="{ey+10}" stroke="#9aa3ad" stroke-width="2"/>')
A(f'<circle cx="{ex}" cy="{ey}" r="15" fill="#20324f" stroke="#0f1b2d"/>')
A(f'<path d="M{ex+7},{ey-13} A15,15 0 0,1 {ex+7},{ey+13}" fill="none" stroke="#c9d3e3" stroke-width="5"/>')
text(ex - 22, ey + 4, "C1", 10, anchor="end", weight=700)
text(ex - 22, ey + 16, "1000 µF", 9, anchor="end", fill="var(--muted)")
for px, py in ((x(11), TOP_P), (x(12), TOP_M)):
    A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')

# --- ARGB-Eingang vom Mainboard: R3 1 k (j9 -> j13), R4 2 k (h13 -> h15), h15 -> GND ---
def resistor(r1, r2, col, label, bands):
    xa, yy = hole(r1, col)
    xb, _ = hole(r2, col)
    A(f'<line x1="{xa}" y1="{yy}" x2="{xb}" y2="{yy}" stroke="#9aa3ad" stroke-width="2"/>')
    A(f'<rect x="{xa+16}" y="{yy-6}" width="{xb-xa-32}" height="12" rx="5" fill="#d9c49a" stroke="#8a7650"/>')
    for i, c in enumerate(bands):
        A(f'<rect x="{xa+22+i*7}" y="{yy-6}" width="3" height="12" fill="{c}"/>')
    for px in (xa, xb):
        A(f'<circle cx="{px}" cy="{yy}" r="3" fill="#9aa3ad"/>')
resistor(9, 13, "j", "R3", ("#7a4a1d", "#111111", "#d22f2f"))
resistor(13, 15, "h", "R4", ("#d22f2f", "#111111", "#d22f2f"))
text(x(11), COLS["j"] - 8, "R3 1 k", 9, weight=700)
text(x(14), COLS["h"] + 16, "R4 2 k", 9, weight=700)
wire([hole(15, "j"), (x(15), TOP_M)], BLU)
# Kabel vom Mainboard-ARGB-Header: D -> g13, G -> - Schiene (Reihe 17)
wire([(x(13) + 10, 12), (x(13) + 10, COLS["g"]), hole(13, "g")], PUR, 3)
wire([(x(17), 12), (x(17), TOP_M)], "#555b63", 3)
text(x(13) + 14, 10, "ARGB vom Mainboard: D (lila), G (grau), 5V nicht anschließen", 9.5, anchor="start", fill=PUR, weight=700)

# --- Verdrahtung unten (Spalten a-e) ---
wire([hole(22, "a"), (x(22), BOT_P)], RED)   # VCC pin 14
wire([hole(17, "a"), (x(17), BOT_M)], BLU)   # 3A pin 9
wire([hole(18, "a"), (x(18), BOT_P)], RED)   # 3OE pin 10
wire([hole(20, "a"), (x(20), BOT_M)], BLU)   # 4A pin 12
wire([hole(21, "a"), (x(21), BOT_P)], RED)   # 4OE pin 13
# C2 100 nF: b22 -> - Schiene unten bei Reihe 24
c2x, c2y = hole(22, "b")
A(f'<path d="M{c2x},{c2y} L{x(23)+4},{c2y+20} M{x(24)},{BOT_M} L{x(23)+10},{c2y+30}" stroke="#9aa3ad" stroke-width="2" fill="none"/>')
A(f'<ellipse cx="{x(23)+8}" cy="{c2y+25}" rx="8" ry="6" fill="#d98b2b" stroke="#8f5714"/>')
for px, py in ((c2x, c2y), (x(24), BOT_M)):
    A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')
text(x(24) + 12, c2y + 29, "C2 100 nF", 10, anchor="start", weight=700)

# Schienen-Bruecken oben <-> unten (rechts um das Board herum)
wire([(x(30), TOP_P), (bx2 + 14, TOP_P), (bx2 + 14, BOT_P), (x(30), BOT_P)], RED, 2.6)
wire([(x(29), TOP_M), (bx2 + 6, TOP_M), (bx2 + 6, BOT_M), (x(29), BOT_M)], BLU, 2.6)

# --- Extern: Netzteil, WAGO, Streifen ---
nx = 760
# WAGO-Klemmen
for (wy, col, lab) in ((40, RED, "+5V"), (130, BLU, "GND")):
    A(f'<rect x="{nx}" y="{wy}" width="58" height="40" rx="5" fill="var(--wago)" stroke="var(--line)"/>')
    for i in range(3):
        A(f'<rect x="{nx+7+i*17}" y="{wy+6}" width="11" height="14" rx="2" fill="#f2a33a"/>')
    text(nx + 29, wy + 34, lab, 10, weight=700, fill=col)
text(nx + 29, 32, "WAGO 221", 9.5, fill="var(--muted)")
# Netzteil
A(f'<rect x="{nx-10}" y="{215}" width="78" height="62" rx="6" fill="var(--psu)" stroke="var(--line)"/>')
text(nx + 29, 241, "Netzteil", 11, weight=700)
text(nx + 29, 256, "5 V · 10 A", 10)
text(nx + 29, 269, "(min. 4 A)", 9, fill="var(--muted)")
wire([(nx + 12, 215), (nx + 12, 200), (nx - 14, 200), (nx - 14, 60), (nx, 60)], RED, 4.5)
wire([(nx + 46, 215), (nx + 46, 190), (nx + 70, 190), (nx + 70, 150), (nx + 58, 150)], BLU, 4.5)
# duenne Leitungen zum Breadboard
wire([(nx, 48), (x(26), 48), (x(26), TOP_P)], RED, 2.4)
wire([(nx, 150), (nx - 30, 150), (nx - 30, 64), (x(28), 64), (x(28), TOP_M)], BLU, 2.4)
# LED-Streifen
sx = 880
A(f'<rect x="{sx}" y="20" width="44" height="{H-40}" rx="4" fill="#15171b" stroke="#000"/>')
for i in range(10):
    yy = 160 + i * 28
    A(f'<rect x="{sx+12}" y="{yy}" width="20" height="20" rx="2" fill="#f4f4f2"/>')
    A(f'<circle cx="{sx+22}" cy="{yy+10}" r="5" fill="hsl({i*36},85%,58%)"/>')
text(sx + 22, H - 28, "WS2812B", 9, fill="#c9d1dc", extra=f'transform="rotate(-90 {sx+22} {H-28})"')
# Anschlusspads am Streifen
for (py, col, lab) in ((60, RED, "+5V"), (95, GRN, "DIN"), (130, BLU, "GND")):
    A(f'<rect x="{sx+6}" y="{py-6}" width="32" height="12" rx="2" fill="#c9a227"/>')
    text(sx + 22, py + 4, lab, 8.5, weight=700, fill="#15171b")
wire([(nx + 58, 52), (sx + 6, 60)], RED, 4.5)
wire([(nx + 58, 142), (sx + 6, 130)], BLU, 4.5)
# DIN von j25
wire([hole(25, "j"), (x(25), 86), (840, 86), (840, 95), (sx + 6, 95)], GRN, 3)
text(842, 108, "Daten", 9.5, anchor="start", fill=GRN, weight=700)

A('</svg>')
BB_SVG = "\n".join(out)

# ======================= Schaltplan =======================
out = []
A = out.append
SW, SH = 960, 510
A(f'<svg viewBox="0 0 {SW} {SH}" role="img" aria-label="Schaltplan: 5-V-Netzteil versorgt LED-Streifen direkt; '
  f'ESP32-C6 über Schottky-Diode; GPIO1 über 74AHCT125 und 330 Ohm an DIN; gemeinsame Masse" class="sch" '
  f'font-family="var(--mono)">')
A('<defs><marker id="dot" viewBox="0 0 6 6" refX="3" refY="3" markerWidth="6" markerHeight="6">'
  '<circle cx="3" cy="3" r="3" fill="currentColor"/></marker></defs>')
L = lambda pts, col="currentColor", w=1.8: A(
    f'<path d="M' + " L".join(f"{a},{b}" for a, b in pts) + f'" fill="none" stroke="{col}" stroke-width="{w}" stroke-linejoin="round"/>')
J = lambda a, b, col="currentColor": A(f'<circle cx="{a}" cy="{b}" r="3.6" fill="{col}"/>')

def t(tx, ty, s, size=12, anchor="middle", weight=400, fill="currentColor"):
    A(f'<text x="{tx}" y="{ty}" font-size="{size}" text-anchor="{anchor}" font-weight="{weight}" fill="{fill}">{s}</text>')

Y5, YG = 60, 470       # 5-V-Leitung oben, Masse unten
# Netzteil
A('<rect x="30" y="150" width="110" height="130" rx="6" fill="var(--psu)" stroke="currentColor" stroke-width="1.4"/>')
t(85, 200, "Netzteil", 13, weight=700); t(85, 218, "5 V DC", 12); t(85, 234, "10 A / 50 W", 12)
t(85, 252, "(min. 4 A + ABL)", 10, fill="var(--muted)")
L([(85, 150), (85, Y5), (900, Y5)], RED, 2.6)
L([(85, 280), (85, YG), (900, YG)], BLU, 2.6)
t(100, Y5 - 10, "+5 V (Litze ≥ 1,0 mm²)", 11, anchor="start", fill=RED, weight=700)
t(100, YG + 22, "GND (gemeinsame Masse)", 11, anchor="start", fill=BLU, weight=700)

# C1
c1x = 200
J(c1x, Y5, RED); J(c1x, YG, BLU)
L([(c1x, Y5), (c1x, 216)]); L([(c1x, 226), (c1x, YG)])
L([(c1x - 18, 216), (c1x + 18, 216)], w=2.6)
A(f'<path d="M{c1x-18},{232} Q{c1x},{222} {c1x+18},{232}" fill="none" stroke="currentColor" stroke-width="2.6"/>')
t(c1x - 12, 208, "+", 12, weight=700)
t(c1x + 24, 214, "C1", 12, anchor="start", weight=700); t(c1x + 24, 230, "1000 µF", 11, anchor="start")
t(c1x + 24, 244, "≥ 10 V", 11, anchor="start", fill="var(--muted)")

# D1 -> ESP 5V
dx = 330
J(dx, Y5, RED)
L([(dx, Y5), (dx, 110)])
A(f'<path d="M{dx-11},110 L{dx+11},110 L{dx},128 Z" fill="currentColor"/>')
L([(dx - 11, 129), (dx + 11, 129)], w=2.6)
L([(dx, 129), (dx, 160)])
t(dx + 16, 116, "D1 1N5819", 11, anchor="start", weight=700)
t(dx + 16, 130, "Schottky", 10, anchor="start", fill="var(--muted)")

# ESP-Block
A('<rect x="280" y="160" width="150" height="150" rx="6" fill="var(--pcb2)" stroke="currentColor" stroke-width="1.4"/>')
t(355, 186, "ESP32-C6-LCD-1.3", 12, weight=700)
t(355, 202, "(Waveshare)", 10, fill="var(--muted)")
t(290, 172 + 0, "", 1)
t(dx, 176, "", 1)
t(300, 228, "5V", 11, anchor="start"); t(300, 296, "GND", 11, anchor="start")
t(420, 252, "GPIO1", 11, anchor="end", weight=700)
t(355, 272, "USB-C nur zum", 9.5, fill="var(--muted)"); t(355, 284, "Flashen/Debuggen", 9.5, fill="var(--muted)")
L([(330, 310), (330, YG)]); J(330, YG, BLU)
L([(430, 248), (520, 248)], YEL, 2.4)

# 74AHCT125
A('<rect x="520" y="170" width="150" height="160" rx="6" fill="var(--ic2)" stroke="currentColor" stroke-width="1.4"/>')
t(595, 192, "U1 74AHCT125", 12, weight=700)
t(595, 207, "3,3 V → 5 V", 10, fill="var(--muted)")
A('<path d="M570,226 L570,270 L612,248 Z" fill="none" stroke="currentColor" stroke-width="1.6"/>')
t(528, 244, "2", 9, anchor="start", fill="var(--muted)"); t(538, 244, "1A", 10, anchor="start")
t(662, 244, "1Y", 10, anchor="end"); t(668, 244, "", 9)
L([(520, 248), (570, 248)])
L([(612, 248), (670, 248)])
L([(591, 259), (591, 300)]);
A('<circle cx="591" cy="263" r="0"/>')
t(600, 296, "1OE̅ (Pin 1)", 9.5, anchor="start")
L([(591, 300), (591, 312), (591, 330)])
# VCC / GND des IC
L([(560, Y5), (560, 170)], RED); J(560, Y5, RED); t(566, 164, "14", 9, anchor="start", fill="var(--muted)")
L([(630, 330), (630, YG)], BLU); J(630, YG, BLU); t(636, 344, "7", 9, anchor="start", fill="var(--muted)")
L([(591, 330), (591, YG)], BLU); J(591, YG, BLU); t(560, 344, "1 →GND", 9, anchor="end", fill="var(--muted)")
# C2 lokal am IC mit Massesymbol
J(560, 100, RED)
L([(560, 100), (640, 100), (640, 116)])
L([(626, 116), (654, 116)], w=2.4); L([(626, 124), (654, 124)], w=2.4)
L([(640, 124), (640, 138)])
L([(628, 138), (652, 138)], w=2); L([(633, 143), (647, 143)], w=2); L([(637, 148), (643, 148)], w=2)
t(662, 118, "C2 100 nF", 11, anchor="start", weight=700)
t(662, 132, "an Pin 14 / GND", 9.5, anchor="start", fill="var(--muted)")

# R1 -> DIN
L([(670, 248), (700, 248)])
A('<rect x="700" y="238" width="60" height="20" rx="2" fill="none" stroke="currentColor" stroke-width="1.8"/>')
t(730, 230, "R1 330 Ω", 11, weight=700)
L([(760, 248), (820, 248)], GRN, 2.4)

# Streifen
A('<rect x="820" y="140" width="110" height="200" rx="6" fill="#15171b" stroke="currentColor" stroke-width="1.4"/>')
t(875, 166, "WS2812B", 12, weight=700, fill="#e8ecf2"); t(875, 182, "144 LEDs / 1 m", 10, fill="#b8c1cc")
for i in range(6):
    A(f'<circle cx="{842+i*14}" cy="208" r="5" fill="hsl({i*55},85%,60%)"/>')
t(830, 252, "DIN", 11, anchor="start", fill="#e8ecf2", weight=700)
t(875, 296, "5V   GND", 10, fill="#b8c1cc")
L([(900, Y5), (900, 140)], RED, 2.6); J(900, Y5, RED)
L([(900, 340), (900, YG)], BLU, 2.6); J(900, YG, BLU)
t(906, 130, "+5V", 10, anchor="start", fill=RED, weight=700)
t(906, 356, "GND", 10, anchor="start", fill=BLU, weight=700)

# ARGB-Eingang: Header D -> R3 -> GPIO2, Knoten -> R4 -> GND
t(392, 300, "GPIO2", 11, weight=700)
L([(392, 310), (392, 326)], PUR, 2.4); J(392, 326, PUR)
A('<rect x="383" y="334" width="18" height="40" rx="2" fill="none" stroke="currentColor" stroke-width="1.8"/>')
t(378, 358, "R3 1 k", 10, anchor="end", weight=700)
L([(392, 326), (392, 334)], PUR, 2.4); L([(392, 374), (392, 396)], PUR, 2.4)
L([(392, 326), (450, 326), (450, 352)])
A('<rect x="441" y="352" width="18" height="40" rx="2" fill="none" stroke="currentColor" stroke-width="1.8"/>')
t(464, 376, "R4 2 k", 10, anchor="start", weight=700)
L([(450, 392), (450, YG)], BLU); J(450, YG, BLU)
A('<rect x="350" y="396" width="84" height="48" rx="5" fill="var(--psu)" stroke="currentColor" stroke-width="1.4"/>')
t(392, 413, "Mainboard", 10, weight=700); t(392, 426, "ARGB-Header", 10)
t(392, 439, "D · G", 9.5, fill="var(--muted)")
L([(420, 444), (420, YG)], BLU); J(420, YG, BLU)

t(660, 372, "unbenutzte Kanäle 2–4:", 9.5, anchor="start", fill="var(--muted)")
t(660, 385, "OE̅ → +5 V, A → GND", 9.5, anchor="start", fill="var(--muted)")
A('</svg>')
SCH_SVG = "\n".join(out)

HTML = open(__import__("os").path.join(__import__("os").path.dirname(__file__), "schaltung_template.html"), encoding="utf-8").read()
HTML = HTML.replace("{{SCH_SVG}}", SCH_SVG).replace("{{BB_SVG}}", BB_SVG)
open(__import__("os").path.join(__import__("os").path.dirname(__file__), "schaltung.html"), "w", encoding="utf-8").write(HTML)
print("ok", len(HTML))
