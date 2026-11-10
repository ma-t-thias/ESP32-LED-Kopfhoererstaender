#!/usr/bin/env python3
"""Einseitige Platine fuer den Sockel des Kopfhoererstaenders.

Erzeugt Fertigungsdaten fuer Leiterplatten-Shops (Gerber-ZIP: Kupfer, Loetstopp, Bestueckungsdruck,
Umriss, Bohrungen), eine 1:1-Aetzvorlage (PNG/PDF) und einen Bestueckungsplan. Koordinaten in mm, Draufsicht von der Bauteilseite,
Ursprung = Mitte des Sockels (wie im SolidCraft-Modell), +y Richtung Display.

Aufruf: pip install shapely pillow && python3 docs/hardware/platine/gen_platine.py
"""
import os
from shapely.geometry import Polygon, Point, LineString
from shapely.ops import unary_union
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)

# ---------------- Vorgaben ----------------
OUTLINE = [(-7.5, -10.5), (4.5, -10.5), (7.0, -13.0), (21.0, -13.0), (23.5, -10.5), (30.5, 0), (33, 4), (33, 7.2),
           (19.5, 7.7), (14.5, 10.2), (10.2, 14.5), (7.5, 20.5), (7.5, 33), (-10.5, 33), (-10.5, 19.0), (-7.5, 19.0), (-7.5, 6), (-13.5, 6), (-13.5, 0)]
MOUNT = [(-5, -6), (27.5, -1), (0, 30)]      # M2,5
MOUNT_DRILL, MOUNT_KEEP = 2.7, 2.4         # Kupfer-Freiraum um die Befestigungsloecher (Radius)
CLEAR = 0.5                                # Abstand Masseflaeche zu fremden Netzen
EDGE = 0.6                                 # Kupfer-Abstand zum Rand
W_SIG, W_PWR = 0.8, 1.5                    # Leiterbahnbreiten

# Pads: name -> (x, y, Pad-Durchmesser, Bohrung, Netz, Form)
P = 2.54
pads = {}
def pad(name, x, y, net, d=1.9, drill=0.9, shape="round"):
    pads[name] = dict(x=x, y=y, d=d, drill=drill, net=net, shape=shape)

# U1 74AHCT125, DIP-14, um 180 Grad gedreht (Kerbe rechts, Pin 1 oben rechts)
UX, UY = 8.5, -2.5
U1 = {}
for i in range(7):
    U1[1 + i] = (UX + 3 * P - i * P, UY + 3.81)     # Pins 1..7 oben, von rechts nach links
    U1[8 + i] = (UX - 3 * P + i * P, UY - 3.81)     # Pins 8..14 unten, von links nach rechts
u1_net = {1: "GND", 2: "IO3", 3: "OUT", 4: "GND", 5: "GND", 6: None, 7: "GND",
          8: None, 9: "GND", 10: "GND", 11: None, 12: "GND", 13: "GND", 14: "+5V"}
for n, (x, y) in U1.items():
    pad(f"U1.{n}", x, y, u1_net[n], d=1.8, drill=0.8, shape="square" if n == 1 else "oval")

# D1 1N5819 stehend links in der verbreiterten Zunge, ca. 3,5 mm neben der ESP-Stiftleiste:
# Koerper ueber der Anode (unten, +5V), Kathode oben (VESP)
pad("D1.A", -8.7, 22.9, "+5V", d=2.0, drill=1.0)
pad("D1.K", -8.7, 27.98, "VESP", d=2.0, drill=1.0, shape="square")
# C2 100 nF, Folienkondensator CY630 104J (Raster 10,16 mm, 11,5 x 5,4 x 10,3 mm), stehend unter U1
pad("C2.1", 17.16, -10.0, "+5V", d=1.9, drill=1.0)
pad("C2.2", 7.0, -10.0, "GND", d=1.9, drill=1.0)
# R1 330 Ohm stehend
pad("R1.1", 21.0, -1.3, "OUT")
pad("R1.2", 21.0, 1.24, "DIN")
# Anschluesse LED-Streifen (rechts): grosse Pads fuer Litze 1 mm2
# USB-C (links): JST-XH 2-polig (Raster 2,5 mm, 3 A), steckbar mit fertigem XH-2P-Kabel zur Buchsenplatine
# (VBUS/GND = Pins 1-2 der 8er-Leiste). Bohrung 1,1 mm: alternativ Litze bis 0,75 mm2 direkt einloeten.
SX = 23.5
pad("STRIP.GND", SX, 3.4, "GND", d=2.6, drill=1.3, shape="square")
pad("STRIP.DIN", SX, -0.4, "DIN", d=2.0, drill=1.0)
pad("STRIP.5V", SX, -4.2, "+5V", d=2.6, drill=1.3)
pad("USB.GND", -10.4, 3.9, "GND", d=2.0, drill=1.1, shape="square")
pad("USB.5V", -10.4, 1.4, "+5V", d=2.0, drill=1.1)
# C1 1000 uF, Elko Ø8,2 x 16,7 liegend in der Zunge (Koerper x -7,5..0,7 / y 4,5..21,2), Beine nach unten abgewinkelt, Raster 3,5
pad("C1.+", -5.15, 3.0, "+5V", d=1.9, drill=0.9, shape="square")
pad("C1.-", -1.65, 3.0, "GND", d=1.9, drill=0.9)
# Zunge: ARGB-Eingang, R3/R4-Spannungsteiler, Anschluss ESP (2,54-mm-Raster)
AX = 5.34
# ARGB-Stecker vom Mainboard (3-polig im 4er-Raster): 5V, D, Lücke (Kodierung), G.
# 5V wird nur mechanisch aufgenommen und ist nicht angeschlossen.
# Liegt parallel zur schraegen Kante (x + y = 24,7), 90-Grad-Stiftleiste zeigt nach aussen.
import math
AC = (11.71, 10.44)                         # Mitte der Reihe, 1,8 mm von der Kante
AU = (math.sqrt(0.5), -math.sqrt(0.5))      # Richtung entlang der Kante
def argb_pos(i):                            # i = 0..3 (5V, D, Luecke, G)
    t = (i - 1.5) * P
    return (round(AC[0] + t * AU[0], 3), round(AC[1] + t * AU[1], 3))
pad("ARGB.5V", *argb_pos(0), "ARGB_5V_NC")
pad("ARGB.D", *argb_pos(1), "ARGB_D")
pad("ARGB.G", *argb_pos(3), "GND", shape="square")
pad("R3.1", AX, 10.54, "ARGB_D")
pad("R3.2", AX, 13.08, "IO23")
pad("R4.1", AX, 15.62, "IO23")
pad("R4.2", AX - P, 15.62, "GND")
EY = 25.5
pad("ESP.VBUS", -3.81, EY, "VESP", shape="square")
pad("ESP.IO3", -1.27, EY, "IO3")
pad("ESP.GND", 1.27, EY, "GND")
pad("ESP.IO23", 3.81, EY, "IO23")
# USB-Daten vom PC (optional): JST-XH 2-polig fuer D+/D- der USB-C-Buchsenplatine (Pins 5-6 der 8er-Leiste)
# -> ESP (J3 Pin 12 = GPIO13 = D+, J3 Pin 11 = GPIO12 = D-). Das Datenkabel ist der Schalter: gesteckt kann der
# PC ueber die Sockel-Buchse Daten senden, abgezogen nur Strom (Netzteil-Betrieb, Flashen ueber die ESP-Buchse).
# XH-Gehaeuse 7,5 x 5,75 x 9,8 mm: Daten x 0,9..8,4 / y 3,8..9,55, Strom x -13,3..-7,5 / y -1,1..6,4 (frei von C1).
pad("USBD.DP", 3.4, 6.9, "USB_DP")
pad("USBD.DM", 5.9, 6.9, "USB_DM")
pad("ESP.DP", 3.3, 28.6, "USB_DP")
pad("ESP.DM", 5.9, 29.8, "USB_DM")
SJ_SIZE = 1.0
smd = {}

def at(n):
    return (pads[n]["x"], pads[n]["y"])

# Leiterbahnen: (Netz, Breite, Punkte)
tracks = [
    ("+5V", W_PWR, [at("USB.5V"), (-1.0, -3.9)]),
    ("+5V", 1.2, [at("C1.+"), at("USB.5V")]),
    ("+5V", 1.2, [at("C1.+"), (-5.15, 21.0), (-8.7, 21.0), at("D1.A")]),
    ("+5V", 1.2, [(-1.0, -3.9), (UX + 3 * P + 1.6, -3.9), (SX - 1.4, -3.9), at("STRIP.5V")]),
    ("+5V", 1.0, [U1[14], (U1[14][0], -3.9)]),
    ("+5V", 1.0, [at("C2.1"), U1[14]]),
    ("VESP", W_SIG + 0.2, [at("D1.K"), (-3.81, 27.98), at("ESP.VBUS")]),
    ("IO3", W_SIG, [U1[2], (U1[2][0], 4.9), (-1.27, 4.9), at("ESP.IO3")]),
    ("OUT", W_SIG, [U1[3], (U1[3][0], -1.3), at("R1.1")]),
    ("DIN", W_SIG, [at("R1.2"), at("STRIP.DIN")]),
    ("ARGB_D", W_SIG, [at("ARGB.D"), at("R3.1")]),
    ("IO23", W_SIG, [at("R3.2"), at("R4.1"), (AX, 23.5), at("ESP.IO23")]),
]

# ---------------- Geometrie ----------------
board = Polygon(OUTLINE)

def pad_shape(p):
    x, y, d = p["x"], p["y"], p["d"]
    if p["shape"] == "square":
        return Polygon([(x - d / 2, y - d / 2), (x + d / 2, y - d / 2), (x + d / 2, y + d / 2), (x - d / 2, y + d / 2)])
    if p["shape"] == "oval":   # DIP: laenglich quer zur Reihe
        return LineString([(x, y - 0.35), (x, y + 0.35)]).buffer(d / 2 - 0.05)
    return Point(x, y).buffer(d / 2, 32)

def track_shape(t):
    return LineString(t[2]).buffer(t[1] / 2, 16, cap_style=1, join_style=1)

# Leiterbahnen oben (Bauteilseite): nur die USB-Datenleitungen, 0,5 mm breit.
# Beim Selbstaetzen (einseitig) stattdessen mit Draht auf der Bauteilseite verbinden.
W_USB = 0.5
def sj(n):
    return smd[n][:2]
top_tracks = [
    ("USB_DP", W_USB, [at("USBD.DP"), (-2.0, 6.9), (-2.0, 23.0), (5.5, 23.0), (5.5, 27.0), at("ESP.DP")]),
    ("USB_DM", W_USB, [at("USBD.DM"), (7.3, 8.3), (7.3, 17.5), (6.5, 18.3), (6.5, 29.8), at("ESP.DM")]),
]

def smd_shape(n):
    x, y, _ = smd[n]
    h = SJ_SIZE / 2
    return Polygon([(x - h, y - h), (x + h, y - h), (x + h, y + h), (x - h, y + h)])

net_cu = {}
for name, p in pads.items():
    net_cu.setdefault(p["net"] or name, []).append(pad_shape(p))
for t in tracks:
    net_cu.setdefault(t[0], []).append(track_shape(t))
net_cu = {k: unary_union(v) for k, v in net_cu.items()}

# Masseflaeche: Platine minus Rand, minus Freiraum um alle fremden Netze und Befestigungsloecher
foreign = unary_union([g.buffer(CLEAR, 16) for k, g in net_cu.items() if k != "GND"])
holes = unary_union([Point(m).buffer(MOUNT_KEEP, 32) for m in MOUNT])
pour = board.buffer(-EDGE, join_style=2).difference(foreign).difference(holes)
# Thermische Entlastung an GND-Pads: Ring freistellen, 4 Stege lassen
gnd_pads = [n for n, p in pads.items() if p["net"] == "GND"]
for n in gnd_pads:
    p = pads[n]
    ring = Point(p["x"], p["y"]).buffer(p["d"] / 2 + 0.45, 32).difference(pad_shape(p))
    spokes = unary_union([LineString([(p["x"] - 3, p["y"]), (p["x"] + 3, p["y"])]).buffer(0.4),
                          LineString([(p["x"], p["y"] - 3), (p["x"], p["y"] + 3)]).buffer(0.4)])
    pour = pour.difference(ring.difference(spokes))
pour = unary_union([pour, net_cu["GND"]]).intersection(board.buffer(-EDGE / 2))
# Inseln ohne GND-Pad entfernen
geoms = list(pour.geoms) if pour.geom_type == "MultiPolygon" else [pour]
keep, gnd_islands = [], 0
for g in geoms:
    if any(g.intersects(Point(at(n))) for n in gnd_pads):
        keep.append(g)
        gnd_islands += 1
pour = unary_union(keep)

copper = unary_union([pour] + [g for k, g in net_cu.items() if k != "GND"])

# ---------------- Pruefungen ----------------
problems = []
names = [k for k in net_cu if k != "GND"]
for i, a in enumerate(names):
    for b in names[i + 1:]:
        dist = net_cu[a].distance(net_cu[b])
        if dist < 0.45:
            problems.append(f"Abstand {a}-{b}: {dist:.2f} mm")
    if net_cu[a].distance(pour) < 0.4:
        problems.append(f"Abstand {a}-GND: {net_cu[a].distance(pour):.2f} mm")
    if not board.buffer(-0.4).contains(net_cu[a]):
        problems.append(f"{a} zu nah am Rand")
    for m in MOUNT:
        if net_cu[a].distance(Point(m)) < MOUNT_DRILL / 2 + 1.0:
            problems.append(f"{a} zu nah an Befestigungsloch {m}")
for n, p in pads.items():
    if not board.buffer(-0.5).contains(pad_shape(p)):
        problems.append(f"Pad {n} zu nah am Rand")
    if p["net"] == "GND" and not pour.intersects(Point(p["x"], p["y"])):
        problems.append(f"GND-Pad {n} nicht an der Masseflaeche")
# Oberseite: Bahnen gegen alle Pads (die gibt es oben auch) und gegeneinander
top_cu = {}
for n, p in pads.items():
    top_cu.setdefault(p["net"] or n, []).append(pad_shape(p))
for t in top_tracks:
    top_cu.setdefault(t[0], []).append(track_shape(t))
for n, (x, y, net) in smd.items():
    top_cu.setdefault(net, []).append(smd_shape(n))
top_cu = {k: unary_union(v) for k, v in top_cu.items()}
top_nets = {t[0] for t in top_tracks}
for a in top_nets:
    for b in top_cu:
        if b != a and top_cu[a].distance(top_cu[b]) < 0.45:
            problems.append(f"Oben Abstand {a}-{b}: {top_cu[a].distance(top_cu[b]):.2f} mm")
    if not board.buffer(-0.4).contains(top_cu[a]):
        problems.append(f"Oben {a} zu nah am Rand")
    for m in MOUNT:
        if top_cu[a].distance(Point(m)) < MOUNT_DRILL / 2 + 1.0:
            problems.append(f"Oben {a} zu nah an Befestigungsloch {m}")
print("GND-Inseln:", gnd_islands)
print("Probleme:", problems or "keine")

# ---------------- Gerber ----------------
def fmt(v):
    return f"{round(v * 1e6):d}"

def ring_coords(coords):
    return list(coords)

def gerber_poly(lines, poly, dark=True):
    """Polygon mit Loechern: Aussenkontur dunkel, Loecher hell (LPC), danach wieder LPD."""
    lines.append("%LPD*%")
    rings = [poly.exterior]
    for r in rings:
        pts = ring_coords(r.coords)
        lines.append("G36*")
        lines.append(f"X{fmt(pts[0][0])}Y{fmt(pts[0][1])}D02*")
        for x, y in pts[1:]:
            lines.append(f"X{fmt(x)}Y{fmt(y)}D01*")
        lines.append("G37*")
    if poly.interiors:
        lines.append("%LPC*%")
        for r in poly.interiors:
            pts = ring_coords(r.coords)
            lines.append("G36*")
            lines.append(f"X{fmt(pts[0][0])}Y{fmt(pts[0][1])}D02*")
            for x, y in pts[1:]:
                lines.append(f"X{fmt(x)}Y{fmt(y)}D01*")
            lines.append("G37*")
        lines.append("%LPD*%")

def polys(g):
    return list(g.geoms) if g.geom_type in ("MultiPolygon", "GeometryCollection") else [g]

# ---------------- Fertigungsdaten (Gerber fuer Leiterplatten-Shops) ----------------
# Bestellung als 2-lagige Platine: Leiterbahnen und Masseflaeche nur unten (wie beim Aetzen),
# oben nur Loetaugen, damit die Bohrungen durchkontaktiert werden. Loetstopplack beidseitig,
# Bestueckungsdruck oben. Befestigungsloecher ohne Durchkontaktierung (NPTH).
# Alle Gerber-Lagen in Draufsicht von oben (Bauteilseite), wie im Gerber-Format ueblich.
MASK_EXP = 0.05      # Loetstopp-Freistellung um die Pads
SILK_W = 0.16        # Linienbreite Bestueckungsdruck
SILK_H = 1.1         # Schrifthoehe

def gerber_header(func, polarity="Positive"):
    return ["%FSLAX46Y46*%", "%MOMM*%", f"%TF.GenerationSoftware,gen_platine.py*%",
            f"%TF.FileFunction,{func}*%", f"%TF.FilePolarity,{polarity}*%", "%TF.Part,Single*%",
            "%ADD10C,0.150*%", "G01*", "G75*"]

def write_layer(fname, func, geoms):
    """geoms: Liste von Flaechen, nacheinander gezeichnet; groesste zuerst, damit Aussparungen
    einer Flaeche keine kleinere Insel darin loeschen."""
    L = gerber_header(func)
    allp = []
    for g in geoms:
        allp += [p for p in polys(g) if not p.is_empty]
    for p in sorted(allp, key=lambda q: -q.area):
        gerber_poly(L, p)
    L.append("M02*")
    open(os.path.join(OUT, fname), "w").write("\n".join(L) + "\n")

def write_outline(fname):
    L = gerber_header("Profile,NP")
    L.append("D10*")
    pts = list(board.exterior.coords)
    L.append(f"X{fmt(pts[0][0])}Y{fmt(pts[0][1])}D02*")
    for x, y in pts[1:]:
        L.append(f"X{fmt(x)}Y{fmt(y)}D01*")
    L.append("M02*")
    open(os.path.join(OUT, fname), "w").write("\n".join(L) + "\n")

# Kleine Strichschrift (Raster 4 x 6) fuer den Bestueckungsdruck
FONT = {
    "A": [[(0, 0), (0, 4), (2, 6), (4, 4), (4, 0)], [(0, 3), (4, 3)]],
    "B": [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3), (0, 3)], [(3, 3), (4, 2), (4, 1), (3, 0), (0, 0)]],
    "C": [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0), (3, 0), (4, 1)]],
    "D": [[(0, 0), (0, 6), (2, 6), (4, 4), (4, 2), (2, 0), (0, 0)]],
    "E": [[(4, 6), (0, 6), (0, 0), (4, 0)], [(0, 3), (3, 3)]],
    "G": [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0), (3, 0), (4, 1), (4, 3), (2, 3)]],
    "I": [[(1, 6), (3, 6)], [(2, 6), (2, 0)], [(1, 0), (3, 0)]],
    "J": [[(4, 6), (4, 1), (3, 0), (1, 0), (0, 1)]],
    "K": [[(0, 0), (0, 6)], [(4, 6), (0, 2)], [(1, 3), (4, 0)]],
    "L": [[(0, 6), (0, 0), (4, 0)]],
    "N": [[(0, 0), (0, 6), (4, 0), (4, 6)]],
    "O": [[(1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5), (4, 1), (3, 0), (1, 0)]],
    "P": [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3), (0, 3)]],
    "R": [[(0, 0), (0, 6), (3, 6), (4, 5), (4, 4), (3, 3), (0, 3)], [(2, 3), (4, 0)]],
    "S": [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 4), (1, 3), (3, 3), (4, 2), (4, 1), (3, 0), (1, 0), (0, 1)]],
    "U": [[(0, 6), (0, 1), (1, 0), (3, 0), (4, 1), (4, 6)]],
    "V": [[(0, 6), (2, 0), (4, 6)]],
    "0": [[(1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5), (4, 1), (3, 0), (1, 0)], [(0, 1), (4, 5)]],
    "1": [[(1, 5), (2, 6), (2, 0)], [(1, 0), (3, 0)]],
    "2": [[(0, 5), (1, 6), (3, 6), (4, 5), (4, 4), (0, 0), (4, 0)]],
    "3": [[(0, 5), (1, 6), (3, 6), (4, 5), (4, 4), (3, 3), (1, 3)], [(3, 3), (4, 2), (4, 1), (3, 0), (1, 0), (0, 1)]],
    "4": [[(3, 0), (3, 6), (0, 2), (4, 2)]],
    "5": [[(4, 6), (0, 6), (0, 3), (3, 3), (4, 2), (4, 1), (3, 0), (0, 0)]],
    "+": [[(2, 1), (2, 5)], [(0, 3), (4, 3)]],
    "-": [[(0, 3), (4, 3)]],
    " ": [],
}

def silk_text(t, x, y, h=SILK_H):
    """Text mittig um (x, y)."""
    u = h / 6
    w = (len(t) * 5.5 - 1.5) * u
    x0, y0 = x - w / 2, y - h / 2
    out = []
    for i, ch in enumerate(t):
        for stroke in FONT[ch]:
            pts = [(x0 + (i * 5.5 + a) * u, y0 + b * u) for a, b in stroke]
            out.append(LineString(pts).buffer(SILK_W / 2, 8))
    return unary_union(out)

def silk_rect(x0, y0, x1, y1):
    return LineString([(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)]).buffer(SILK_W / 2, 8, join_style=2)

def silk_circle(x, y, r):
    return Point(x, y).buffer(r + SILK_W / 2, 32).difference(Point(x, y).buffer(r - SILK_W / 2, 32))

pad_all = unary_union([pad_shape(p) for p in pads.values()])
mask_open_top_extra = unary_union([smd_shape(n).buffer(MASK_EXP, 4, join_style=2) for n in smd] or [Point(0, 0).buffer(0)])
mask_open = unary_union([pad_shape(p).buffer(MASK_EXP, 16) for p in pads.values()] +
                        [Point(m).buffer(MOUNT_DRILL / 2 + 0.1, 32) for m in MOUNT])

silk = unary_union([
    # U1 mit Kerbe rechts
    silk_rect(UX - 9.6, UY - 2.6, UX + 9.6, UY + 2.6),
    Point(UX + 9.6, UY).buffer(0.9 + SILK_W / 2, 24).difference(Point(UX + 9.6, UY).buffer(0.9 - SILK_W / 2, 24)).intersection(
        Polygon([(UX + 8.5, UY - 2), (UX + 9.6, UY - 2), (UX + 9.6, UY + 2), (UX + 8.5, UY + 2)])),
    silk_text("U1", UX - 5.5, UY),
    # C2 (Folie, stehend) unter U1
    silk_rect(6.4, -12.6, 17.8, -7.4), silk_text("C2", 12.1, -10.0),
    # C1 liegend in der Zunge, Plus links
    silk_rect(-7.0, 4.6, 0.3, 21.0), silk_text("C1", -3.4, 12.8), silk_text("1000", -3.4, 10.6, 0.9),
    silk_text("+", -5.15, 5.4),
    # D1 stehend: Kreis um die Anode (Koerper), Strich an der Kathode
    silk_circle(-8.7, 22.9, 1.55), silk_text("D1", -8.7, 25.45),
    LineString([(-9.95, 29.6), (-7.45, 29.6)]).buffer(SILK_W, 8),
    # Widerstaende stehend
    silk_text("R1", 21.0, -3.1), silk_text("R3", AX - 2.0, 11.8), silk_text("R4", AX + 2.2, 15.62),
    # Anschluesse
    silk_text("5V", SX + 2.6, -4.2), silk_text("D", SX + 1.8, -0.4), silk_text("GND", SX + 3.4, 3.4),
    silk_text("LED", 29.0, 6.0, 0.9),
    silk_rect(-13.28, -1.1, -7.53, 6.4).intersection(board.buffer(-0.35)), silk_text("USB", -10.4, -2.2, 0.9),
    silk_text("+", -12.4, 1.4, 0.9), silk_text("-", -12.4, 3.9, 0.9),
    silk_rect(0.9, 3.8, 8.4, 9.55), silk_text("USB", 1.6, 10.3, 0.7),
    silk_text("ARGB", 9.0, 7.3, 0.9),
    silk_text("5V", argb_pos(0)[0] - 1.45, argb_pos(0)[1] - 1.45, 0.8),
    silk_text("D", argb_pos(1)[0] - 1.45, argb_pos(1)[1] - 1.45, 0.8),
    silk_text("G", argb_pos(3)[0] - 1.45, argb_pos(3)[1] - 1.45, 0.8),
    LineString([(argb_pos(2)[0] - 0.42, argb_pos(2)[1] - 0.42), (argb_pos(2)[0] + 0.42, argb_pos(2)[1] + 0.42)]).buffer(SILK_W / 2, 8),
    silk_text("ESP", -2.4, 22.6, 0.9),
    silk_text("D+", 3.4, 5.0, 0.7), silk_text("D-", 5.9, 5.0, 0.7),
    silk_text("D+", 3.3, 30.5, 0.7), silk_text("D-", 5.9, 31.7, 0.7),
    silk_text("V", -3.81, 27.3, 0.8), silk_text("3", -1.27, 27.3, 0.8),
    silk_text("G", 1.27, 27.3, 0.8), silk_text("23", 3.81, 27.3, 0.8),
])
silk = silk.difference(unary_union([mask_open, mask_open_top_extra]).buffer(0.15)).intersection(board.buffer(-0.3))
for m in MOUNT:
    silk = silk.difference(Point(m).buffer(MOUNT_DRILL / 2 + 0.3))

# Kupfer unten wie beim Aetzen, aber ohne die Koernerpunkte (der Hersteller bohrt selbst)
write_layer("platine-B_Cu.gbl", "Copper,L2,Bot", [pour] + [g for k, g in net_cu.items() if k != "GND"])
write_layer("platine-F_Cu.gtl", "Copper,L1,Top", [unary_union([pad_all] + [track_shape(t) for t in top_tracks] +
                                                                [smd_shape(n) for n in smd])])
write_layer("platine-F_Mask.gts", "Soldermask,Top", [unary_union([mask_open, mask_open_top_extra])])
write_layer("platine-B_Mask.gbs", "Soldermask,Bot", [mask_open])
write_layer("platine-F_SilkS.gto", "Legend,Top", [silk])
write_outline("platine-Edge_Cuts.gm1")

def write_drill(fname, items, plated):
    tools = sorted({d for d, _, _ in items})
    E = ["M48", f"; DRILL file gen_platine.py, {'PTH' if plated else 'NPTH'}",
         f"; #@! TF.FileFunction,{'Plated' if plated else 'NonPlated'},1,2,{'PTH' if plated else 'NPTH'}",
         "METRIC,TZ", "FMAT,2", "G90"]
    for i, d in enumerate(tools, 1):
        E.append(f"T{i}C{d:.3f}")
    E += ["%", "G05"]
    for i, d in enumerate(tools, 1):
        E.append(f"T{i}")
        E += [f"X{x:.3f}Y{y:.3f}" for dd, x, y in items if dd == d]
    E += ["T0", "M30"]
    open(os.path.join(OUT, fname), "w").write("\n".join(E) + "\n")

write_drill("platine-PTH.drl", [(p["drill"], p["x"], p["y"]) for p in pads.values()], True)
write_drill("platine-NPTH.drl", [(MOUNT_DRILL, x, y) for x, y in MOUNT], False)
old = os.path.join(OUT, "platine.drl")
if os.path.exists(old):
    os.remove(old)

import zipfile
FAB = ["platine-F_Cu.gtl", "platine-B_Cu.gbl", "platine-F_Mask.gts", "platine-B_Mask.gbs",
       "platine-F_SilkS.gto", "platine-Edge_Cuts.gm1", "platine-PTH.drl", "platine-NPTH.drl"]
with zipfile.ZipFile(os.path.join(OUT, "platine_gerber.zip"), "w", zipfile.ZIP_DEFLATED) as z:
    for f in FAB:
        z.write(os.path.join(OUT, f), f)

# ---------------- Bilder ----------------
xmin, ymin, xmax, ymax = board.bounds

def raster(dpi, margin=3.0):
    s = dpi / 25.4
    W = int((xmax - xmin + 2 * margin) * s)
    H = int((ymax - ymin + 2 * margin) * s)
    tf = lambda x, y: ((x - xmin + margin) * s, (ymax - y + margin) * s)
    return s, W, H, tf

def draw_geom(dr, g, tf, fill):
    for p in polys(g):
        if p.is_empty:
            continue
        dr.polygon([tf(*c) for c in p.exterior.coords], fill=fill)
        for r in p.interiors:
            dr.polygon([tf(*c) for c in r.coords], fill=None if fill is None else "white")

def draw_copper_mask(dpi):
    s, W, H, tf = raster(dpi)
    im = Image.new("1", (W, H), 1)
    dr = ImageDraw.Draw(im)
    for p in polys(pour):
        dr.polygon([tf(*c) for c in p.exterior.coords], fill=0)
        for r in p.interiors:
            dr.polygon([tf(*c) for c in r.coords], fill=1)
    for k, g in net_cu.items():
        if k == "GND":
            continue
        for p in polys(g):
            dr.polygon([tf(*c) for c in p.exterior.coords], fill=0)
    for p in pads.values():
        x, y = tf(p["x"], p["y"])
        r = 0.3 * s
        dr.ellipse([x - r, y - r, x + r, y + r], fill=1)
    for m in MOUNT:
        x, y = tf(*m)
        r = MOUNT_DRILL / 2 * s
        dr.ellipse([x - r, y - r, x + r, y + r], outline=0, width=max(1, int(0.15 * s)))
    # Umriss als duenne Linie (Schnittkante)
    dr.line([tf(*c) for c in board.exterior.coords], fill=0, width=max(1, int(0.15 * s)))
    return im

# 1:1-Aetzvorlage, 600 dpi, Bauteilseiten-Ansicht (fuer Tonertransfer NICHT spiegeln)
mask = draw_copper_mask(600)
mask.save(os.path.join(OUT, "platine_aetzvorlage_600dpi.png"), dpi=(600, 600))
# A4-PDF mit 2 Kopien
a4 = Image.new("1", (4961, 7016), 1)
a4.paste(mask, (600, 600))
a4.paste(mask, (600 + mask.width + 300, 600))
d = ImageDraw.Draw(a4)
try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 60)
except OSError:
    font = ImageFont.load_default()
d.text((600, 600 + mask.height + 120),
       "Kopfhoererstaender Platine, Kupferseite, Ansicht von der Bauteilseite, 1:1 (nicht skalieren).",
       fill=0, font=font)
d.text((600, 600 + mask.height + 220),
       "Tonertransfer: so drucken wie hier (nicht spiegeln). Pruefmass: Platine 46,5 x 36,5 mm.",
       fill=0, font=font)
a4.convert("L").save(os.path.join(OUT, "platine_aetzvorlage_A4.pdf"), resolution=600)

# Bestueckungsplan (farbig, Draufsicht)
s, W, H, tf = raster(800)
im = Image.new("RGB", (W, H), "white")
dr = ImageDraw.Draw(im)
dr.polygon([tf(*c) for c in board.exterior.coords], fill="#2f6b3a", outline="black")
draw_geom(dr, pour, tf, "#c8a24a")
colors = {"+5V": "#d9534f", "VESP": "#f08a24", "IO3": "#e6c229", "OUT": "#9b6bd6", "DIN": "#2e9e5b",
          "ARGB_D": "#8e44ad", "IO23": "#6c5ce7"}
for k, g in net_cu.items():
    if k == "GND":
        continue
    draw_geom(dr, g, tf, colors.get(k, "#c8a24a"))
for t in top_tracks:   # Bahnen oben (Bauteilseite) gestrichelt hellblau
    line = LineString(t[2])
    d0 = 0.0
    while d0 < line.length:
        seg = [line.interpolate(d0), line.interpolate(min(d0 + 0.8, line.length))]
        dr.line([tf(seg[0].x, seg[0].y), tf(seg[1].x, seg[1].y)], fill="#7fd3ff", width=int(0.5 * s))
        d0 += 1.4
for p in pads.values():
    x, y = tf(p["x"], p["y"])
    r = p["drill"] / 2 * s
    dr.ellipse([x - r, y - r, x + r, y + r], fill="white")
for m in MOUNT:
    x, y = tf(*m)
    r = MOUNT_DRILL / 2 * s
    dr.ellipse([x - r, y - r, x + r, y + r], fill="white", outline="black", width=3)
try:
    f1 = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", int(1.3 * s))
    f2 = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", int(0.95 * s))
except OSError:
    f1 = f2 = ImageFont.load_default()

def label(x, y, t, f=f2, fill="white"):
    X, Y = tf(x, y)
    dr.text((X, Y), t, fill=fill, font=f, anchor="mm", stroke_width=3, stroke_fill="black")

# Bauteilumrisse (Oberseite)
def box(x0, y0, x1, y1, t, col="#ffffff"):
    a, b = tf(x0, y1), tf(x1, y0)
    dr.rectangle([a, b], outline=col, width=4)
    label((x0 + x1) / 2, (y0 + y1) / 2, t, f1)
box(UX - 9.6, UY - 3.0, UX + 9.6, UY + 3.0, "U1 74AHCT125")
X, Y = tf(UX + 9.6, UY)
dr.ellipse([X - 0.9 * s, Y - 0.9 * s, X + 0.9 * s, Y + 0.9 * s], outline="white", width=4)
label(-6.4, 22.9, "D1", f1)
box(6.4, -12.7, 17.9, -7.3, "C2")
box(-7.5, 4.5, 0.7, 21.2, "")
label(-3.4, 12.5, "C1 1000µF", f2)
label(-3.4, 1.4, "+  C1  −", f2)
label(19.4, -0.1, "R1", f1)
label(AX + 0.2, 11.8, "R3", f1)
label(AX - 1.3, 17.0, "R4", f1)
for n, t in (("STRIP.GND", "GND"), ("STRIP.DIN", "DIN"), ("STRIP.5V", "+5V")):
    label(pads[n]["x"] + 3.6, pads[n]["y"], t)
box(-13.28, -1.1, -7.53, 6.4, "")
label(-10.4, -2.3, "XH USB +5V/GND", f2)
label(9.0, 7.1, "ARGB 90°", f2)
label(argb_pos(0)[0] - 1.3, argb_pos(0)[1] - 1.9, "5V nc", ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", int(0.7 * s)))
label(argb_pos(1)[0] - 1.5, argb_pos(1)[1] - 1.5, "D", f2)
label(argb_pos(3)[0] - 1.5, argb_pos(3)[1] - 1.5, "G", f2)
label(0, 27.2, "VBUS IO3 GND IO23", ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", int(0.7 * s)))
fs = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", int(0.7 * s))
box(0.9, 3.8, 8.4, 9.55, "")
label(4.65, 4.6, "XH USB D+ D-", fs)
for n in smd:
    q = smd_shape(n)
    dr.polygon([tf(*c) for c in q.exterior.coords], fill="#c8a24a", outline="black")
label(3.3, 30.3, "D+", fs)
label(5.9, 31.3, "D-", fs)
label(29.5, 5.0, "LED", f2)
im.save(os.path.join(OUT, "platine_bestueckung.png"))
print("ok", OUT)
