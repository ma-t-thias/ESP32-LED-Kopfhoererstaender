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
TOP_M, TOP_P = 40 + OY, 62 + OY      # obere Schiene: - aussen, + innen (neben j)
BOT_M, BOT_P = 358 + OY, 380 + OY    # untere Schiene: - innen (neben a), + aussen
TOP_OUT, BOT_OUT = TOP_M, BOT_P

# Zweites Breadboard darunter (Board 2): Reihe j liegt 7 Raster (17,78 mm) unter Reihe a von Board 1,
# genau der Abstand der beiden Stiftleisten des ESP. Board 2 hat nur unten eine Stromschiene.
COLS2 = {k: v + 360 for k, v in COLS.items()}
BOT2_M, BOT2_P = COLS2["a"] + 38, COLS2["a"] + 60

def hole(r, c):
    return x(r), COLS[c]

def hole2(r, c):
    return x(r), COLS2[c]

RED = "var(--plus)"
BLU = "var(--minus)"
YEL = "var(--data1)"
GRN = "var(--data2)"
PUR = "var(--data3)"
V33 = "var(--v33)"
FSRC = "var(--fsr)"
I2S = "var(--i2s)"

out = []
A = out.append

def wire(pts, color, w=3.2, dash=None, halo=False):
    d = "M" + " L".join(f"{px},{py}" for px, py in pts)
    da = f' stroke-dasharray="{dash}"' if dash else ""
    if halo:  # heller Rand, damit Kreuzungen als "drueber gelegt" lesbar sind
        A(f'<path d="{d}" fill="none" stroke="var(--board)" stroke-width="{w+4}" '
          f'stroke-linecap="round" stroke-linejoin="round"/>')
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

W, H = 960, 950 + OY
A(f'<svg viewBox="0 0 {W} {H}" role="img" aria-label="Breadboard-Belegung auf zwei Boards: ESP32-C6-Touch-LCD-1.9 ueber beiden Boards in Reihe 1 bis 20, '
  f'74AHCT125 auf Board 1 in Reihe 23 bis 29, 330-Ohm-Widerstand zur Datenleitung, USB-C-Buchse auf Board 2 in Reihe 23 bis 30, Spalte a, '
  f'Spannungsteiler fuer den Kopfhoerer-Sensor auf Board 2 in Reihe 3 und 4, Mikrofon INMP441 und FSR-Folie per Kabel unter Board 2, '
  f'LED-Streifen rechts außerhalb des Breadboards" class="bb" font-family="var(--mono)">')

# Breadboard-Koerper
bx1, bx2 = x(1) - 22, x(30) + 22
A(f'<rect x="{bx1}" y="{TOP_OUT-16}" width="{bx2-bx1}" height="{BOT_OUT-TOP_OUT+32}" rx="8" '
  f'fill="var(--board)" stroke="var(--line)"/>')
# Mittelkanal
A(f'<rect x="{bx1+6}" y="{COLS["f"]+14}" width="{bx2-bx1-12}" height="{COLS["e"]-COLS["f"]-28}" '
  f'rx="3" fill="var(--channel)"/>')
# Schienen-Linien
for yy, col, sign in ((TOP_M - 10, BLU, "−"), (TOP_P + 10, RED, "+"),
                      (BOT_M - 10, BLU, "−"), (BOT_P + 10, RED, "+")):
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

# --- Board 2 (unten angesteckt) ---
A(f'<rect x="{bx1}" y="{COLS2["j"]-24}" width="{bx2-bx1}" height="{BOT2_P-COLS2["j"]+40}" rx="8" '
  f'fill="var(--board)" stroke="var(--line)"/>')
A(f'<rect x="{bx1+6}" y="{COLS2["f"]+14}" width="{bx2-bx1-12}" height="{COLS2["e"]-COLS2["f"]-28}" '
  f'rx="3" fill="var(--channel)"/>')
for yy, col in ((BOT2_M + 10, BLU), (BOT2_P - 10, V33)):
    A(f'<line x1="{x(1)-6}" y1="{yy}" x2="{x(30)+6}" y2="{yy}" stroke="{col}" stroke-width="1.4" opacity=".7"/>')
text(bx1 + 9, BOT2_M + 4, "−", 13, weight=700, fill=BLU)
text(bx1 + 9, BOT2_P + 4, "3V3", 8, weight=700, fill=V33)
for r in range(1, 31):
    for c, yy in COLS2.items():
        A(f'<rect x="{x(r)-3}" y="{yy-3}" width="6" height="6" rx="1" fill="var(--hole)"/>')
    for yy in (BOT2_M, BOT2_P):
        A(f'<rect x="{x(r)-3}" y="{yy-3}" width="6" height="6" rx="1" fill="var(--hole)"/>')
for r in (1, 5, 10, 15, 20, 25, 30):
    text(x(r), COLS2["f"] + 30, r, 10, fill="var(--muted)")
for c, yy in COLS2.items():
    text(bx1 + 9, yy + 4, c, 10, fill="var(--muted)")
    text(bx2 - 9, yy + 4, c, 10, fill="var(--muted)")
text(bx2 + 8, COLS["j"] - 30, "Board 1", 10, anchor="start", weight=700, fill="var(--muted)")
text(bx2 + 8, COLS2["j"] - 30, "Board 2", 10, anchor="start", weight=700, fill="var(--muted)")

# --- ESP32-C6-Touch-LCD-1.9 (Display nach oben, USB-C Richtung Reihe 1) ---
# Stiftleisten im Abstand 17,78 mm = 7 Raster: J4 in Board 1 Spalte a, J3 in Board 2 Spalte j,
# Pin 1 jeweils in Reihe 1. Das Display deckt Board 1 d-a und Board 2 j-g ab; pro Pin frei
# erreichbar bleibt nur je ein Loch: Board 1 Spalte e bzw. Board 2 Spalte f.
ex1, ex2 = x(1) - 12, x(22) + 6
ey1, ey2 = COLS["e"] + 10, COLS2["f"] - 10
A(f'<rect x="{ex1}" y="{ey1}" width="{ex2-ex1}" height="{ey2-ey1}" rx="6" fill="var(--pcb)" '
  f'fill-opacity=".9" stroke="var(--pcbedge)" stroke-width="1.5"/>')
A(f'<rect x="{ex1+14}" y="{ey1+40}" width="{ex2-ex1-28}" height="{ey2-ey1-80}" rx="3" fill="#0b0d12" opacity=".75"/>')
text((ex1 + ex2) / 2, (ey1 + ey2) / 2 - 8, "ESP32-C6-Touch-LCD-1.9", 12, weight=700, fill="#e8ecf2")
text((ex1 + ex2) / 2, (ey1 + ey2) / 2 + 10, "Display oben, J4 in Board 1 Spalte a, J3 in Board 2 Spalte j", 9.5, fill="#9aa6b5")
A(f'<rect x="{ex1-16}" y="{(ey1+ey2)/2-18}" width="18" height="36" rx="4" fill="#b9c0c9" stroke="#7d8692"/>')
j4 = ["VBUS", "SYS", "GND", "EN", "3V3", "VREF", "22", "GND", "23", "EX0",
      "RUN", "EX1", "GND", "EX2", "EX3", "EX4", "EX5", "GND", "EX6", "EX7"]
j3 = ["16", "17", "GND", "0", "1", "2", "3", "GND", "8", "9",
      "12", "13", "GND", "14", "15", "18", "19", "GND", "20", "21"]
used_j4, used_j3 = (1, 3, 5, 9), (3, 4, 7, 17, 19, 20)
for r in range(1, 21):
    for hfun, col, labels, used, dy in ((hole, "a", j4, used_j4, -11), (hole2, "j", j3, used_j3, 18)):
        xx, yy = hfun(r, col)
        hl = r in used
        A(f'<rect x="{xx-5}" y="{yy-5}" width="10" height="10" rx="2" fill="{"#f4c542" if hl else "#c8a64a"}" stroke="#6b5a1f"/>')
        text(xx, yy + dy, labels[r - 1], 8 if not hl else 9, weight=700 if hl else 400,
             fill="#ffffff" if hl else "#9aa6b5")
# freie Zugangsloecher der benutzten Pins markieren
for xx, yy in (hole(1, "e"), hole(3, "e"), hole(5, "e"), hole(9, "e"),
               hole2(3, "f"), hole2(4, "f"), hole2(7, "f"), hole2(17, "f"), hole2(19, "f"), hole2(20, "f")):
    A(f'<circle cx="{xx}" cy="{yy}" r="7" fill="none" stroke="#f4c542" stroke-width="2"/>')

# --- 74AHCT125 (Reihen 23 bis 29, Kerbe Richtung Reihe 29) ---
cx1, cx2 = x(23) - 9, x(29) + 9
cy1, cy2 = COLS["f"] + 6, COLS["e"] - 6
A(f'<rect x="{cx1}" y="{cy1}" width="{cx2-cx1}" height="{cy2-cy1}" rx="3" fill="var(--ic)" stroke="#000" stroke-opacity=".4"/>')
A(f'<path d="M{cx2},{(cy1+cy2)/2-7} a7,7 0 0,0 0,14" fill="var(--board)"/>')
text((cx1 + cx2) / 2 - 4, (cy1 + cy2) / 2 + 4, "74AHCT125", 11, weight=700, fill="#e8ecf2")
A(f'<circle cx="{x(29)}" cy="{cy1+8}" r="2.6" fill="#e8ecf2"/>')
for r in range(23, 30):
    for c in ("f", "e"):
        xx, yy = hole(r, c)
        A(f'<rect x="{xx-4}" y="{yy-4}" width="8" height="8" fill="#b9c0c9"/>')
# Pinnummern
for i, r in enumerate(range(29, 22, -1)):
    text(x(r), COLS["f"] - 9, i + 1, 8.5, fill="var(--muted)")
for i, r in enumerate(range(23, 30)):
    text(x(r), COLS["e"] + 17, i + 8, 8.5, fill="var(--muted)")

# --- Verdrahtung oben (Spalten f-j) ---
# D1 Schottky ueber den Mittelkanal: Anode in f1, Kathode (Ring) in e1 = VBUS (J4 Pin 1)
xa, ya = hole(1, "f")
xk, yk = hole(1, "e")
A(f'<line x1="{xa}" y1="{ya}" x2="{xk}" y2="{yk}" stroke="#9aa3ad" stroke-width="2"/>')
A(f'<rect x="{xa-6}" y="{ya+12}" width="12" height="{yk-ya-24}" rx="3" fill="#1c1f24"/>')
A(f'<rect x="{xa-6}" y="{yk-18}" width="12" height="5" fill="#d7dbe0"/>')
for px, py in ((xa, ya), (xk, yk)):
    A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')
text(xa - 12, (ya + yk) / 2 + 4, "D1", 10, anchor="end", weight=700)
# j1 -> + Schiene oben (Anode)
wire([hole(1, "j"), (x(1), TOP_P)], RED)

# GND ESP (J4 Pin 3): e3 -> Bruecke ueber die obere Haelfte -> - Schiene oben
wire([hole(3, "e"), (x(3) + 8, COLS["f"] + 30), (x(3) + 8, TOP_M), (x(3), TOP_M)], BLU)
# GPIO3 (J3 Pin 7, Board 2 f7) -> durch den Mittelkanal von Board 2 -> hoch zu g28 (U1 Pin 2, 1A)
wire([hole2(7, "f"), (x(7), COLS2["f"] + 30), (x(22) + 10, COLS2["f"] + 30), (x(22) + 10, COLS["g"]),
      hole(28, "g")], YEL)
# Pin 7 GND (f23) -> j23 -> -
wire([hole(23, "j"), (x(23), TOP_P + 10), (x(23) + 10, TOP_P + 10), (x(23) + 10, TOP_M), (x(23), TOP_M)], BLU)
# Pin 5 2A (f25) -> j25 -> -
wire([hole(25, "j"), (x(25), TOP_P + 10), (x(25) + 10, TOP_P + 10), (x(25) + 10, TOP_M), (x(25), TOP_M)], BLU)
# Pin 4 2OE (f26) -> j26 -> +
wire([hole(26, "j"), (x(26), TOP_P)], RED)
# Pin 1 1OE (f29) -> j29 -> -
wire([hole(29, "j"), (x(29), TOP_P + 10), (x(29) + 10, TOP_P + 10), (x(29) + 10, TOP_M), (x(29), TOP_M)], BLU)
# R1 330 Ohm: h27 (1Y) -> h30
rx1, ry = hole(27, "h")
rx2, _ = hole(30, "h")
A(f'<line x1="{rx1}" y1="{ry}" x2="{rx2}" y2="{ry}" stroke="#9aa3ad" stroke-width="2"/>')
A(f'<rect x="{rx1+14}" y="{ry-7}" width="{rx2-rx1-28}" height="14" rx="6" fill="#d9c49a" stroke="#8a7650"/>')
for i, col in enumerate(("#f58220", "#f58220", "#7a4a1d", "#c9a227")):
    A(f'<rect x="{rx1+18+i*7}" y="{ry-7}" width="3.5" height="14" fill="{col}"/>')
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

# --- ARGB-Eingang vom Mainboard (Reihe 21 = Knoten an GPIO23, Reihe 22 = D vom Mainboard) ---
def vres(xa, ya, xb, yb, bands):
    """Widerstand stehend/schraeg zwischen zwei Punkten."""
    A(f'<line x1="{xa}" y1="{ya}" x2="{xb}" y2="{yb}" stroke="#9aa3ad" stroke-width="2"/>')
    mx, my = (xa + xb) / 2, (ya + yb) / 2
    A(f'<rect x="{mx-6}" y="{my-11}" width="12" height="22" rx="5" fill="#d9c49a" stroke="#8a7650"/>')
    for i, c in enumerate(bands):
        A(f'<rect x="{mx-6}" y="{my-8+i*6}" width="12" height="2.6" fill="{c}"/>')
    for px, py in ((xa, ya), (xb, yb)):
        A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')
# GPIO23 (J4 Pin 9): e9 -> durch den Mittelkanal -> h21
wire([hole(9, "e"), (x(9), COLS["f"] + 30), (x(21) - 8, COLS["f"] + 30), (x(21) - 8, COLS["h"]), hole(21, "h")], PUR, 2.6)
# R4 2 k: j21 -> obere - Schiene
vres(*hole(21, "j"), x(21) + 10, TOP_M, ("#d22f2f", "#111111", "#d22f2f"))
text(x(21) - 10, (COLS["j"] + TOP_M) / 2 + 4, "R4 2 k", 9, anchor="end", weight=700)
# R3 1 k: g21 -> g22 (stehend)
vres(*hole(21, "g"), *hole(22, "g"), ("#7a4a1d", "#111111", "#d22f2f"))
text(x(22) + 8, COLS["g"] + 18, "R3 1 k", 9, anchor="start", weight=700)
# Kabel vom Mainboard-ARGB-Header: D -> i22, G -> - Schiene (Reihe 24)
wire([(x(22) + 6, 12), (x(22) + 6, COLS["i"] - 10), hole(22, "i")], PUR, 3)
wire([(x(24), 12), (x(24), TOP_M)], "#555b63", 3)
text(x(24) + 6, 10, "ARGB vom Mainboard: D (lila), G (grau), 5V frei", 9.5, anchor="start", fill=PUR, weight=700)

# --- Verdrahtung unten (Spalten a-e) ---
wire([hole(29, "a"), (x(29), BOT_M - 10), (x(29) - 10, BOT_M - 10), (x(29) - 10, BOT_P), (x(29), BOT_P)], RED)   # VCC pin 14
wire([hole(24, "a"), (x(24), BOT_M)], BLU)   # 3A pin 9
wire([hole(25, "a"), (x(25), BOT_M - 10), (x(25) + 10, BOT_M - 10), (x(25) + 10, BOT_P), (x(25), BOT_P)], RED)   # 3OE pin 10
wire([hole(27, "a"), (x(27), BOT_M)], BLU)   # 4A pin 12
wire([hole(28, "a"), (x(28), BOT_M - 10), (x(28) - 10, BOT_M - 10), (x(28) - 10, BOT_P), (x(28), BOT_P)], RED)   # 4OE pin 13
# C2 100 nF: b29 -> - Schiene unten bei Reihe 30
c2x, c2y = hole(29, "b")
A(f'<path d="M{c2x},{c2y} L{x(30)-4},{c2y+20} M{x(30)},{BOT_M} L{x(30)-2},{c2y+30}" stroke="#9aa3ad" stroke-width="2" fill="none"/>')
A(f'<ellipse cx="{x(30)-2}" cy="{c2y+25}" rx="8" ry="6" fill="#d98b2b" stroke="#8f5714"/>')
for px, py in ((c2x, c2y), (x(30), BOT_M)):
    A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')
text(x(30) + 12, c2y + 29, "C2", 10, anchor="start", weight=700)

# Schienen-Bruecken oben <-> unten (rechts um das Board herum)
wire([(x(30), TOP_P), (bx2 + 14, TOP_P), (bx2 + 14, BOT_P), (x(30), BOT_P)], RED, 2.6)
wire([(x(30), TOP_M), (bx2 + 6, TOP_M), (bx2 + 6, BOT_M), (x(30) + 8, BOT_M)], BLU, 2.6)

# --- Extern: Netzteil, WAGO, Streifen ---
nx = 760
# WAGO-Klemmen
for (wy, col, lab) in ((40, RED, "+5V"), (130, BLU, "GND")):
    A(f'<rect x="{nx}" y="{wy}" width="58" height="40" rx="5" fill="var(--wago)" stroke="var(--line)"/>')
    for i in range(3):
        A(f'<rect x="{nx+7+i*17}" y="{wy+6}" width="11" height="14" rx="2" fill="#f2a33a"/>')
    text(nx + 29, wy + 34, lab, 10, weight=700, fill=col)
text(nx + 29, 32, "WAGO 221", 9.5, fill="var(--muted)")
# USB-C-Buchse QUARKZMAN auf Board 2, Spalte a, Reihe 23 (GND) bis 30 (CC2).
# CC1/CC2 haben 5,1 kOhm auf der Platine. Die Stifte stecken nur zur Halterung im Breadboard,
# der Strom laeuft ueber Litze, die oben an VBUS und GND angeloetet ist.
USB_PINS = ("GND", "VBUS", "SBU2", "CC1", "D−", "D+", "SBU1", "CC2")
ua = COLS2["a"]
A(f'<rect x="{x(23)-14}" y="{ua-12}" width="{x(30)-x(23)+28}" height="78" rx="7" fill="#1d3f8f" stroke="#0d2766"/>')
for i, lab in enumerate(USB_PINS):
    px = x(23 + i)
    A(f'<circle cx="{px}" cy="{ua}" r="6" fill="#c9a227" stroke="#7a5f10"/>')
    text(px, ua + 19, lab, 6.6, weight=700, fill="#e8eefc")
A(f'<rect x="{x(26)-6}" y="{ua+30}" width="52" height="40" rx="5" fill="#b9bec6" stroke="#6b7280"/>')
A(f'<rect x="{x(26)+2}" y="{ua+40}" width="36" height="12" rx="6" fill="#4b5563"/>')
text(x(24)-4, ua + 46, "USB-C", 9, weight=700, fill="#e8eefc")
text(x(24)-4, ua + 58, "5 V · 3 A", 8, fill="#e8eefc")
text(x(29)+6, ua + 46, "CC", 8, fill="#e8eefc")
text(x(29)+6, ua + 58, "5,1k", 8, fill="#e8eefc")
# Optional PC-Sync ueber USB: D- (a27) / D+ (a28) der Buchse -> ESP GPIO12 (J3 Pin 11) / GPIO13 (J3 Pin 12),
# frei erreichbar ueber Board 2 f11 / f12. Gestrichelt = nur fuer PC-Sync stecken, sonst weglassen.
DAT = "var(--data4, #0ea5e9)"
wire([hole2(27, "e"), (x(27), COLS2["f"] + 50), (x(11), COLS2["f"] + 50), hole2(11, "f")], DAT, 2.4, dash="6 4")
wire([hole2(28, "e"), (x(28), COLS2["f"] + 42), (x(12), COLS2["f"] + 42), hole2(12, "f")], DAT, 2.4, dash="6 4")
for xx, yy in (hole2(11, "f"), hole2(12, "f")):
    A(f'<circle cx="{xx}" cy="{yy}" r="7" fill="none" stroke="{DAT}" stroke-width="2"/>')
text(x(29) + 14, COLS2["f"] + 50, "optional PC-Sync: D−/D+", 9, anchor="start", fill=DAT, weight=700)

# --- Kopfhoerer-Sensor (FSR) und Mikrofon INMP441 (Platine v2, hier auf Board 2) ---
# Untere Schienen von Board 2: aussen 3V3 (vom ESP, J4 Pin 5), innen GND. Kein 5 V auf diese Schienen!
# 3V3: Board 1 e5 (J4/5) -> durch den Mittelkanal von Board 1 -> links aussen um beide Boards -> 3V3-Schiene
wire([hole(5, "e"), (x(5), COLS["f"] + 40), (34, COLS["f"] + 40), (34, BOT2_P + 12), (x(2), BOT2_P + 12),
      (x(2), BOT2_P)], V33, 2.6, halo=True)
# GND: Board 2 f3 (J3/3) -> e3, a3 -> GND-Schiene
wire([hole2(3, "f"), hole2(3, "e")], BLU, 2.6)
wire([hole2(3, "a"), (x(3), BOT2_M)], BLU, 2.6)
# GPIO0: Board 2 f4 (J3/4) -> e4. Reihe 4 (a-e) ist der Messknoten.
wire([hole2(4, "f"), hole2(4, "e")], FSRC, 2.6)
# R5 22 k stehend c4 -> c3 (GND), C3 100 nF b4 -> b3 (GND)
vres(*hole2(4, "c"), *hole2(3, "c"), ("#d22f2f", "#d22f2f", "#f58220"))
c3a, c3b = hole2(4, "b"), hole2(3, "b")
A(f'<path d="M{c3a[0]},{c3a[1]} L{(c3a[0]+c3b[0])/2},{c3a[1]+12} L{c3b[0]},{c3b[1]}" stroke="#9aa3ad" stroke-width="2" fill="none"/>')
A(f'<ellipse cx="{(c3a[0]+c3b[0])/2}" cy="{c3a[1]+14}" rx="8" ry="6" fill="#d98b2b" stroke="#8f5714"/>')
for px, py in (c3a, c3b):
    A(f'<circle cx="{px}" cy="{py}" r="3" fill="#9aa3ad"/>')
for ly, lab, lw in ((COLS2["c"] + 4, "R5 22 kΩ", 52), (COLS2["b"] + 4, "C3 100 nF", 60)):
    A(f'<rect x="{x(5)-4}" y="{ly-11}" width="{lw+6}" height="14" rx="3" fill="var(--paper)" opacity="0.9"/>')
    text(x(5) - 1, ly, lab, 9, anchor="start", weight=700)
# FSR402-LT unter Board 2: ein Anschluss in a4 (Messknoten), der andere an 3V3 (Reihe 6). Polung egal.
fy = BOT2_P + 70
A(f'<rect x="{x(4)-8}" y="{fy}" width="{x(6)-x(4)+16}" height="46" rx="3" fill="#d9d2b8" stroke="#8c836a"/>')
A(f'<circle cx="{x(5)}" cy="{fy+76}" r="30" fill="#d9d2b8" stroke="#8c836a"/>')
A(f'<circle cx="{x(5)}" cy="{fy+76}" r="20" fill="none" stroke="#8c836a" stroke-dasharray="3 3"/>')
wire([(x(4), fy), hole2(4, "a")], FSRC, 2.6, halo=True)
wire([(x(6), fy), (x(6), BOT2_P)], V33, 2.6, halo=True)
text(x(5) + 40, fy + 70, "FSR402-LT", 10, anchor="start", weight=700, fill=FSRC)
text(x(5) + 40, fy + 84, "Kopfhörer-Sensor", 9, anchor="start", fill="var(--muted)")
text(x(5) + 40, fy + 97, "Polung egal", 9, anchor="start", fill="var(--muted)")
# INMP441 unter Board 2, sechs Kabel Buchse-Stecker. VDD -> 3V3 (Reihe 9), GND und L/R -> GND (Reihe 10/11),
# SCK/WS/SD direkt in die freien Loecher der ESP-Pins Board 2 f17 / f19 / f20.
my = BOT2_P + 100
mic = ((9, "VDD", V33, BOT2_P), (10, "GND", BLU, BOT2_M), (11, "L/R", BLU, BOT2_M),
       (17, "SCK", I2S, COLS2["f"]), (19, "WS", I2S, COLS2["f"]), (20, "SD", I2S, COLS2["f"]))
for r, lab, col, ty in mic:
    wire([(x(r), my), (x(r), ty)], col, 2.4, halo=True)
A(f'<rect x="{x(9)-12}" y="{my}" width="{x(20)-x(9)+24}" height="26" rx="5" fill="#2b2f36" stroke="#111"/>')
for r, lab, col, ty in mic:
    A(f'<rect x="{x(r)-5}" y="{my+4}" width="10" height="8" rx="1" fill="#c9a227"/>')
    text(x(r), my + 22, lab, 7.5, weight=700, fill="#e8ecf2")
A(f'<circle cx="{x(23)+6}" cy="{my+30}" r="28" fill="#1d3f8f" stroke="#0d2766"/>')
A(f'<rect x="{x(23)-4}" y="{my+22}" width="20" height="16" rx="2" fill="#b9c0c9"/>')
A(f'<circle cx="{x(23)+6}" cy="{my+30}" r="2.5" fill="#333"/>')
A(f'<path d="M{x(20)+12},{my+13} L{x(23)-22},{my+24}" stroke="#555b63" stroke-width="3"/>')
text(x(23) + 40, my + 24, "INMP441", 10, anchor="start", weight=700, fill=I2S)
text(x(23) + 40, my + 38, "Mikrofon, 6 Kabel", 9, anchor="start", fill="var(--muted)")
text(x(23) + 40, my + 51, "Buchse–Stecker", 9, anchor="start", fill="var(--muted)")
# dicke Litze direkt von den Stiften zu den WAGO-Klemmen
wire([(x(24), ua), (x(24), COLS2["b"] - 9), (nx - 14, COLS2["b"] - 9), (nx - 14, 60), (nx, 60)], RED, 4.5)
wire([(x(23), ua), (x(23), COLS2["c"] - 9), (nx + 70, COLS2["c"] - 9), (nx + 70, 150), (nx + 58, 150)], BLU, 4.5)
# F1 Polyfuse in der +5-V-Litze (optional auf dem Breadboard, auf Platine v2 fest)
A(f'<rect x="{nx-24}" y="400" width="20" height="40" rx="4" fill="#e7b416" stroke="#8a6b00"/>')
text(nx - 2, 416, "F1", 10, anchor="start", weight=700)
text(nx - 2, 429, "4 A", 9, anchor="start", fill="var(--muted)")
A(f'<rect x="{x(13)-4}" y="{COLS2["c"]-17}" width="{x(22)-x(13)+6}" height="14" rx="3" fill="var(--paper)" opacity="0.9"/>')
text(x(22), COLS2["c"] - 6, "Litze 1 mm², oben an die Stifte gelötet", 9, anchor="end", weight=700)
# duenne Leitungen zum Breadboard
wire([(nx, 48), (x(28) + 10, 48), (x(28) + 10, TOP_P), (x(28), TOP_P)], RED, 2.4)
wire([(nx, 150), (nx - 30, 150), (nx - 30, 64), (x(27), 64), (x(27), TOP_M)], BLU, 2.4)
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
# DIN von j30
wire([hole(30, "j"), (x(30) + 10, COLS["j"]), (x(30) + 10, 86), (840, 86), (840, 95), (sx + 6, 95)], GRN, 3)
text(842, 108, "Daten", 9.5, anchor="start", fill=GRN, weight=700)

A('</svg>')
BB_SVG = "\n".join(out)

# ======================= Schaltplan =======================
out = []
A = out.append
SW, SH = 960, 510
A(f'<svg viewBox="0 0 {SW} {SH}" role="img" aria-label="Schaltplan: 5-V-Netzteil versorgt LED-Streifen direkt; '
  f'ESP32-C6 über Schottky-Diode; GPIO3 über 74AHCT125 und 330 Ohm an DIN; gemeinsame Masse" class="sch" '
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
t(85, 192, "USB-C-Netzteil", 12, weight=700); t(85, 210, "5 V / 3 A", 12); t(85, 226, "(15 W)", 11)
t(85, 244, "Buchse mit", 10, fill="var(--muted)"); t(85, 257, "CC 5,1 kΩ", 10, fill="var(--muted)"); t(85, 270, "ABL 2400 mA", 10, fill="var(--muted)")
L([(85, 150), (85, Y5), (900, Y5)], RED, 2.6)
# F1 Polyfuse in der +5-V-Leitung, D2 TVS danach gegen Masse (Platine v2)
A('<rect x="76" y="106" width="18" height="36" rx="3" fill="var(--psu)" stroke="currentColor" stroke-width="1.6"/>')
L([(78, 136), (92, 112)], w=1.4)
t(70, 122, "F1", 11, anchor="end", weight=700); t(70, 136, "4 A", 10, anchor="end", fill="var(--muted)")
J(85, 84, RED)
L([(85, 84), (165, 84), (165, 92)], RED, 2)
A('<path d="M155,110 L175,110 L165,94 Z" fill="currentColor"/>')
L([(153, 90), (155, 93), (175, 93), (177, 96)], w=2.2)
L([(165, 110), (165, 120)])
L([(155, 120), (175, 120)], w=2); L([(159, 125), (171, 125)], w=2); L([(162, 130), (168, 130)], w=2)
t(180, 104, "D2", 11, anchor="start", weight=700); t(180, 117, "TVS", 10, anchor="start", fill="var(--muted)")
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
t(355, 186, "ESP32-C6-Touch-LCD-1.9", 11, weight=700)
t(355, 202, "(Waveshare)", 10, fill="var(--muted)")
t(290, 172 + 0, "", 1)
t(dx, 176, "", 1)
t(300, 228, "VBUS", 11, anchor="start"); t(300, 296, "GND", 11, anchor="start")
t(420, 252, "GPIO3", 11, anchor="end", weight=700)
t(355, 272, "USB-C am Board:", 9.5, fill="var(--muted)"); t(355, 284, "Flashen/Debuggen", 9.5, fill="var(--muted)")
L([(330, 310), (330, YG)]); J(330, YG, BLU)
# optional: USB-Daten der Sockel-Buchse an GPIO12/13 (PC-Sync ueber USB, steckbar per 2,54-mm-Stiftleiste)
A('<path d="M140,290 L280,290" fill="none" stroke="var(--data4, #0ea5e9)" stroke-width="2" stroke-dasharray="6 4"/>')
t(145, 304, "D+/D− optional", 9.5, anchor="start", fill="var(--muted)")
t(145, 316, "→ GPIO13/12 (PC-Sync)", 9.5, anchor="start", fill="var(--muted)")
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
t(875, 166, "WS2812B", 12, weight=700, fill="#e8ecf2"); t(875, 182, "≈100 LEDs / 70 cm", 10, fill="#b8c1cc")
for i in range(6):
    A(f'<circle cx="{842+i*14}" cy="208" r="5" fill="hsl({i*55},85%,60%)"/>')
t(830, 252, "DIN", 11, anchor="start", fill="#e8ecf2", weight=700)
t(875, 296, "5V   GND", 10, fill="#b8c1cc")
L([(900, Y5), (900, 140)], RED, 2.6); J(900, Y5, RED)
L([(900, 340), (900, YG)], BLU, 2.6); J(900, YG, BLU)
t(906, 130, "+5V", 10, anchor="start", fill=RED, weight=700)
t(906, 356, "GND", 10, anchor="start", fill=BLU, weight=700)

# ARGB-Eingang: Header D -> R3 -> GPIO23, Knoten -> R4 -> GND
t(392, 300, "GPIO23", 11, weight=700)
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

# ======================= Sensoren: Kopfhoerer-Sensor und Mikrofon =======================
out = []
A = out.append
SEW, SEH = 960, 330
A(f'<svg viewBox="0 0 {SEW} {SEH}" role="img" aria-label="Schaltplan Sensoren: FSR-Folie zwischen 3,3 V und GPIO0, '
  f'22 kOhm und 100 nF von GPIO0 nach GND; Mikrofon INMP441 an 3,3 V, SCK an GPIO19, WS an GPIO20, SD an GPIO21, '
  f'L/R an GND" class="sch" font-family="var(--mono)">')

def gnd(gx, gy):
    L([(gx, gy), (gx, gy + 8)])
    L([(gx - 12, gy + 8), (gx + 12, gy + 8)], w=2); L([(gx - 7, gy + 13), (gx + 7, gy + 13)], w=2)
    L([(gx - 3, gy + 18), (gx + 3, gy + 18)], w=2)

def v33(vx, vy):
    L([(vx - 12, vy), (vx + 12, vy)], V33, 2.4)
    t(vx, vy - 7, "3V3", 10, weight=700, fill=V33)

# ESP-Block mit den benutzten Pins
A('<rect x="40" y="40" width="190" height="250" rx="6" fill="var(--pcb2)" stroke="currentColor" stroke-width="1.4"/>')
t(135, 64, "ESP32-C6-Touch-", 11, weight=700); t(135, 78, "LCD-1.9", 11, weight=700)
pins = ((110, "GPIO19", "J3/17"), (140, "GPIO20", "J3/19"), (170, "GPIO21", "J3/20"), (240, "GPIO0", "J3/4"))
for py, name, hdr in pins:
    t(222, py + 4, name, 11, anchor="end", weight=700)
    t(52, py + 4, hdr, 9.5, anchor="start", fill="var(--muted)")
t(52, 284, "3V3 = J4/5 · GND = J3/3", 9.5, anchor="start", fill="var(--muted)")

# Mikrofon
for py, col in ((110, I2S), (140, I2S), (170, I2S)):
    L([(230, py), (430, py)], col, 2.2)
A('<rect x="430" y="88" width="170" height="104" rx="6" fill="var(--ic2)" stroke="currentColor" stroke-width="1.4"/>')
t(515, 108, "INMP441", 12, weight=700); t(515, 122, "I2S-Mikrofon", 9.5, fill="var(--muted)")
for py, lab in ((110, "SCK"), (140, "WS"), (170, "SD")):
    t(438, py + 4, lab, 10, anchor="start")
L([(480, 88), (480, 60)]); v33(480, 60); t(488, 84, "VDD", 9.5, anchor="start")
L([(520, 192), (520, 214)]); gnd(520, 214); t(526, 186, "GND", 9.5, anchor="start")
L([(570, 192), (570, 214)]); gnd(570, 214); t(576, 186, "L/R", 9.5, anchor="start")
t(338, 102, "SCK", 9.5, fill="var(--muted)"); t(338, 132, "WS", 9.5, fill="var(--muted)"); t(338, 162, "SD", 9.5, fill="var(--muted)")
t(515, 256, "L/R an GND = linker Kanal", 9.5, fill="var(--muted)")

# Kopfhoerer-Sensor
L([(230, 240), (720, 240)], FSRC, 2.2); J(720, 240, FSRC)
t(470, 232, "Messknoten", 9.5, fill="var(--muted)")
# FSR (veraenderlicher Widerstand) von 3V3 nach unten zum Knoten
L([(720, 60), (720, 150)]); v33(720, 60)
A('<rect x="711" y="150" width="18" height="48" rx="2" fill="none" stroke="currentColor" stroke-width="1.8"/>')
L([(700, 210), (742, 140)], w=1.6)
A('<path d="M742,140 l-9,2 l5,6 Z" fill="currentColor"/>')
L([(720, 198), (720, 240)])
t(750, 170, "FSR402-LT", 11, anchor="start", weight=700, fill=FSRC)
t(750, 184, "Kopfhörer-Sensor", 9.5, anchor="start", fill="var(--muted)")
t(750, 197, "ca. 10 kΩ … > 1 MΩ", 9.5, anchor="start", fill="var(--muted)")
# R5 und C3 nach GND
L([(720, 240), (720, 250)])
A('<rect x="711" y="250" width="18" height="40" rx="2" fill="none" stroke="currentColor" stroke-width="1.8"/>')
L([(720, 290), (720, 298)]); gnd(720, 298)
t(704, 274, "R5 22 k", 10, anchor="end", weight=700)
L([(720, 240), (820, 240), (820, 262)]); J(720, 240, FSRC)
L([(804, 262), (836, 262)], w=2.4); L([(804, 270), (836, 270)], w=2.4)
L([(820, 270), (820, 298)]); gnd(820, 298)
t(844, 270, "C3 100 nF", 10, anchor="start", weight=700)
A('</svg>')
SEN_SVG = "\n".join(out)

HTML = open(__import__("os").path.join(__import__("os").path.dirname(__file__), "schaltung_template.html"), encoding="utf-8").read()
HTML = HTML.replace("{{SCH_SVG}}", SCH_SVG).replace("{{BB_SVG}}", BB_SVG).replace("{{SEN_SVG}}", SEN_SVG)
open(__import__("os").path.join(__import__("os").path.dirname(__file__), "schaltung.html"), "w", encoding="utf-8").write(HTML)
print("ok", len(HTML))
