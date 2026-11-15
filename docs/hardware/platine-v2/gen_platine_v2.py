#!/usr/bin/env python3
"""Platine v2 fuer den Sockel des Kopfhoererstaenders v2 (ovaler Sockel).

Neu gegenueber v1: Kopfhoerer-Sensor (FSR-Folie an GPIO0), I2S-Mikrofon INMP441 (GPIO19/20/21),
Polyfuse F1 und TVS-Diode D2 am USB-Eingang, ein gemeinsamer 10-poliger Anschluss zum ESP.
Die USB-Datenleitungen (PC-Sync) laufen nicht mehr ueber die Platine, sondern direkt von der
USB-C-Buchsenplatine zum ESP.

Erzeugt Fertigungsdaten fuer Leiterplatten-Shops (Gerber-ZIP: Kupfer, Loetstopp, Bestueckungsdruck,
Umriss, Bohrungen), eine 1:1-Aetzvorlage (PNG/PDF) und einen Bestueckungsplan. Koordinaten in mm,
Draufsicht von der Bauteilseite, Ursprung = Mitte des Sockels (wie im SolidCraft-Modell), +y Richtung Display.

Aufruf: pip install shapely pillow && python3 docs/hardware/platine-v2/gen_platine_v2.py
"""
import os
from shapely.geometry import Polygon, Point, LineString, box as sbox
from shapely.ops import unary_union
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
os.makedirs(OUT, exist_ok=True)
PRE = "platine-v2"

# ---------------- Vorgaben ----------------
# Rechteck 64 x 52 mm mit Eckradius 2 mm (x -32..32, y -22..30)
OUTLINE = [(round(x, 4), round(y, 4)) for x, y in
           sbox(-30, -20, 30, 28).buffer(2.0, 8).exterior.coords[:-1]]
MOUNT = [(-29, -19), (29, -19), (-29, 21), (29, 21)]   # M2,5 (Dome im Sockel)
MOUNT_DRILL, MOUNT_KEEP = 2.7, 2.4         # Kupfer-Freiraum um die Befestigungsloecher (Radius)
CLEAR = 0.5                                # Abstand Masseflaeche zu fremden Netzen
EDGE = 0.6                                 # Kupfer-Abstand zum Rand
W_SIG, W_PWR = 0.8, 1.5                    # Leiterbahnbreiten

P = 2.54
pads = {}
def pad(name, x, y, net, d=1.9, drill=0.9, shape="round"):
    pads[name] = dict(x=x, y=y, d=d, drill=drill, net=net, shape=shape)

# --- Eingang hinten links: USB-C-Buchsenplatine -> F1 (Polyfuse) -> +5V, D2 (TVS) gegen GND
pad("J1.GND", -22.0, -18.0, "GND", d=2.0, drill=1.1, shape="square")
pad("J1.VIN", -22.0 + P, -18.0, "VIN", d=2.0, drill=1.1)
# F1 Polyfuse RXEF375 (3,75 A halten bei 20 Grad, ca. 3,2 A bei 40 Grad im Sockel), radial, Raster 5,1 mm
pad("F1.1", -15.0, -18.0, "VIN", d=2.0, drill=1.0)
pad("F1.2", -15.0, -18.0 + 2 * P, "+5V", d=2.0, drill=1.0)
# D2 TVS SA5.0A (DO-15) stehend: Koerper ueber der Anode (GND), Kathode (Ring) an +5V
pad("D2.K", -10.0, -18.0 + 2 * P, "+5V", d=2.0, drill=1.1, shape="square")
pad("D2.A", -10.0, -18.0, "GND", d=2.0, drill=1.1)
# C1 1000 uF, Elko 8,2 x 16,7 liegend, Raster 3,5, Koerper Richtung +y (x -28,4..-20,2 / y -7..9,7)
pad("C1.+", -26.0, -8.0, "+5V", shape="square")
pad("C1.-", -22.5, -8.0, "GND")

# --- U1 74AHCT125, DIP-14, um 180 Grad gedreht (Kerbe rechts, Pin 1 oben rechts). Kanal 2: A=Pin 5, Y=Pin 6, OE=Pin 4
UX, UY = -2.0, 2.0
U1 = {}
for i in range(7):
    U1[1 + i] = (round(UX + 3 * P - i * P, 3), round(UY + 3.81, 3))   # Pins 1..7 oben, von rechts nach links
    U1[8 + i] = (round(UX - 3 * P + i * P, 3), round(UY - 3.81, 3))   # Pins 8..14 unten, von links nach rechts
u1_net = {1: "GND", 2: "GND", 3: None, 4: "GND", 5: "IO3", 6: "OUT", 7: "GND",
          8: None, 9: "GND", 10: "GND", 11: None, 12: "GND", 13: "GND", 14: "+5V"}
for n, (x, y) in U1.items():
    pad(f"U1.{n}", x, y, u1_net[n], d=1.8, drill=0.8, shape="square" if n == 1 else "oval")
# C2 100 nF Keramik, Raster 5,08, rechts neben U1 Pin 14
pad("C2.1", 8.5, U1[14][1], "+5V")
pad("C2.2", 8.5, U1[14][1] - 2 * P, "GND")
# R1 330 Ohm stehend ueber U1 Pin 6
pad("R1.1", U1[6][0], 9.0, "OUT")
pad("R1.2", U1[7][0], 9.0, "DIN")

# --- LED-Streifen (links): Loetpads im 2,54-Raster fuer Pigtail (JST-SM) oder Stiftleiste
pad("LED.5V", -27.0, 12.0, "+5V", d=2.0, drill=1.1)
pad("LED.GND", -27.0 + P, 12.0, "GND", d=2.0, drill=1.1, shape="square")
pad("LED.DIN", -27.0 + 2 * P, 12.0, "DIN", d=1.9, drill=1.0)

# --- D1 1N5819 stehend: +5V -> VESP (Versorgung des ESP ueber VBUS)
pad("D1.A", -16.0, 19.42, "+5V", d=2.0, drill=1.0)
pad("D1.K", -16.0, 24.5, "VESP", d=2.0, drill=1.0, shape="square")

# --- Kopfhoerer-Sensor: FSR-Folie zwischen 3V3 und IO0, R5 22k nach GND, C3 100 nF parallel
pad("J5.3V3", -12.0, 17.0, "3V3", shape="square")
pad("J5.FSR", -12.0 + P, 17.0, "IO0")
pad("R5.1", -12.0 + P, 17.0 - P, "IO0")
pad("R5.2", -12.0 + P, 17.0 - 2 * P, "GND")
pad("C3.1", -12.0 + 2 * P, 17.0 - P, "IO0")
pad("C3.2", -12.0 + 2 * P, 17.0 - 2 * P, "GND")

# --- Anschluss ESP (J7, 1x10, vorne): ein Flachkabel / Dupont-Kabel zum ESP32-C6-Touch-LCD-1.9
EY = 26.5
J7 = ["VESP", "GND", "3V3", "IO0", "IO3", "IO23", "IO19", "IO20", "IO21", "GND"]
J7X = {}
for i, net in enumerate(J7):
    x = round(-13.43 + i * P, 2)
    J7X[i] = x
    pad(f"J7.{i + 1}", x, EY, net, drill=1.0, shape="square" if i == 0 else "round")

# --- Mikrofon INMP441 (rundes I2S-Modul Ø 14 mm, 2 x 3 Pins, Reihenabstand 7,62). L/R auf GND = linker Kanal.
# Das Modul liegt flach auf einer kurzen Stiftleiste, Mikrofonchip nach unten, Beschriftung und Schallloch nach oben.
# Von oben gesehen (Beschriftungsseite) um 180 Grad gedreht: obere Reihe GND, VDD, SD; untere Reihe L/R, WS, SCK.
J6X0, J6Y2 = 14.5, EY - 3 * P
J6 = {"GND": (J6X0, EY, "GND"), "VDD": (J6X0 + P, EY, "3V3"), "SD": (J6X0 + 2 * P, EY, "IO21"),
      "LR": (J6X0, J6Y2, "GND"), "WS": (J6X0 + P, J6Y2, "IO20"), "SCK": (J6X0 + 2 * P, J6Y2, "IO19")}
for n, (x, y, net) in J6.items():
    pad(f"J6.{n}", round(x, 2), round(y, 2), net, drill=1.0, shape="square" if n == "GND" else "round")
MIC_C = (J6X0 + P, (EY + J6Y2) / 2)            # Mitte des Moduls = Schallloch

# --- ARGB-Eingang vom Mainboard (J4, 1x4 90 Grad an der rechten Kante): 5V (nc), D, Luecke, G
AX = 29.5
pad("J4.5V", AX, 0.0, "ARGB_5V_NC", drill=1.0)
pad("J4.D", AX, P, "ARGB_D", drill=1.0)
pad("J4.G", AX, 3 * P, "GND", drill=1.0, shape="square")
pad("R3.1", 25.0, P, "ARGB_D")
pad("R3.2", 25.0, 2 * P, "IO23")
pad("R4.1", 25.0 - P, 2 * P, "IO23")
pad("R4.2", 25.0 - P, P, "GND")

SJ_SIZE = 1.0
smd = {}

def at(n):
    return (pads[n]["x"], pads[n]["y"])

# Leiterbahnen unten: (Netz, Breite, Punkte)
Y5 = at("F1.2")[1]
tracks = [
    ("VIN", W_PWR, [at("J1.VIN"), at("F1.1")]),
    ("+5V", W_PWR, [at("F1.2"), at("D2.K"), (U1[14][0], Y5), U1[14]]),
    ("+5V", 1.0, [U1[14], at("C2.1")]),
    ("+5V", W_PWR, [at("F1.2"), (-26.0, Y5), at("C1.+"), (-27.0, -7.0), at("LED.5V")]),
    ("+5V", 1.2, [at("LED.5V"), (-27.0, 14.5), (-16.0, 14.5), at("D1.A")]),
    ("VESP", 1.0, [at("D1.K"), (-16.0, EY), at("J7.1")]),
    ("OUT", W_SIG, [U1[6], at("R1.1")]),
    ("DIN", W_SIG, [at("R1.2"), (at("LED.DIN")[0], 9.0), at("LED.DIN")]),
    ("IO3", W_SIG, [U1[5], (U1[5][0], 8.5), (J7X[4], 8.5), at("J7.5")]),
    ("3V3", W_SIG, [at("J7.3"), (J7X[2], 22.0), (at("J5.3V3")[0], 22.0), at("J5.3V3")]),
    ("IO0", W_SIG, [at("R5.1"), at("J5.FSR"), (J7X[3], 17.0), at("J7.4")]),
    ("IO0", W_SIG, [at("R5.1"), at("C3.1")]),
    ("ARGB_D", W_SIG, [at("J4.D"), at("R3.1")]),
    ("IO23", W_SIG, [at("R3.2"), at("R4.1"), (at("R4.1")[0], 11.0), (J7X[5], 11.0), at("J7.6")]),
    ("IO21", W_SIG, [at("J6.SD"), (at("J6.SD")[0], 23.5), (J7X[8], 23.5), at("J7.9")]),
    ("IO20", W_SIG, [at("J6.WS"), (at("J6.WS")[0], 21.3), (J7X[7], 21.3), at("J7.8")]),
    ("IO19", W_SIG, [at("J6.SCK"), (at("J6.SCK")[0], 16.4), (J7X[6], 16.4), at("J7.7")]),
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

# Leiterbahnen oben (Bauteilseite): 3V3 zum Mikrofon. Beim Selbstaetzen als Draht auf der Bauteilseite.
def sj(n):
    return smd[n][:2]
top_tracks = [
    ("3V3", W_SIG, [at("J7.3"), (J7X[2], 28.8), (at("J6.VDD")[0], 28.8), at("J6.VDD")]),
    # L/R des Mikrofons zur Masse (die Bahnen WS/SCK schliessen das Pad unten ein)
    ("GND", W_SIG, [at("J6.LR"), at("J6.GND")]),
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
    ring = Point(p["x"], p["y"]).buffer(p["d"] * (0.7072 if p["shape"] == "square" else 0.5) + 0.45, 32).difference(pad_shape(p))
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
    if net_cu[a].distance(pour) < 0.45:
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
# Inseln robust zaehlen: Stege unter 0,4 mm Breite gelten als getrennt (fertigt kein Hersteller zuverlaessig)
_bridged = unary_union([pour.buffer(-0.2, 8).buffer(0.2, 8)] + [track_shape(t) for t in top_tracks if t[0] == "GND"] +
                       [pad_shape(pads[n]) for n in gnd_pads])
_parts = list(_bridged.geoms) if _bridged.geom_type == "MultiPolygon" else [_bridged]
gnd_islands = sum(1 for g in _parts if any(g.intersects(Point(at(n))) for n in gnd_pads))
# Zusammenhang je Netz: Pads + Bahnen unten + Bahnen oben (Pads gehen durch) muessen ein Stueck sein
for net in {p["net"] for p in pads.values() if p["net"] and p["net"] != "GND"}:
    parts = [pad_shape(p) for p in pads.values() if p["net"] == net]
    parts += [track_shape(t) for t in tracks + top_tracks if t[0] == net]
    g = unary_union(parts)
    if g.geom_type != "Polygon":
        problems.append(f"Netz {net} nicht verbunden ({len(g.geoms)} Teile)")
print("GND-Inseln (Stege >= 0,4 mm, mit Bruecke oben):", gnd_islands)
if gnd_islands != 1:
    problems.append(f"Masseflaeche in {gnd_islands} Teile getrennt")
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
    return ["%FSLAX46Y46*%", "%MOMM*%", f"%TF.GenerationSoftware,Matthias Steis,gen_platine_v2.py,2*%",
            f"%TF.FileFunction,{func}*%", f"%TF.FilePolarity,{polarity}*%", "%TF.Part,Single*%",
            "%ADD10C,0.150*%", "G01*", "G75*"]

def write_layer(fname, func, geoms, polarity="Positive"):
    """geoms: Liste von Flaechen, nacheinander gezeichnet; groesste zuerst, damit Aussparungen
    einer Flaeche keine kleinere Insel darin loeschen."""
    L = gerber_header(func, polarity)
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
    "F": [[(4, 6), (0, 6), (0, 0)], [(0, 3), (3, 3)]],
    "H": [[(0, 0), (0, 6)], [(4, 0), (4, 6)], [(0, 3), (4, 3)]],
    "M": [[(0, 0), (0, 6), (2, 3), (4, 6), (4, 0)]],
    "Q": [[(1, 0), (0, 1), (0, 5), (1, 6), (3, 6), (4, 5), (4, 1), (3, 0), (1, 0)], [(2, 2), (4, 0)]],
    "T": [[(0, 6), (4, 6)], [(2, 6), (2, 0)]],
    "W": [[(0, 6), (1, 0), (2, 3), (3, 0), (4, 6)]],
    "X": [[(0, 0), (4, 6)], [(0, 6), (4, 0)]],
    "Y": [[(0, 6), (2, 3), (4, 6)], [(2, 3), (2, 0)]],
    "Z": [[(0, 6), (4, 6), (0, 0), (4, 0)]],
    "6": [[(4, 5), (3, 6), (1, 6), (0, 5), (0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (3, 3), (0, 3)]],
    "7": [[(0, 6), (4, 6), (1, 0)]],
    "8": [[(1, 3), (0, 4), (0, 5), (1, 6), (3, 6), (4, 5), (4, 4), (3, 3), (1, 3), (0, 2), (0, 1), (1, 0), (3, 0), (4, 1), (4, 2), (3, 3)]],
    "9": [[(0, 1), (1, 0), (3, 0), (4, 1), (4, 5), (3, 6), (1, 6), (0, 5), (0, 4), (1, 3), (4, 3)]],
    "(": [[(3, 6), (2, 5), (2, 1), (3, 0)]],
    ")": [[(1, 6), (2, 5), (2, 1), (1, 0)]],
    ".": [[(2, 0), (2, 0.4)]],
    # Kleinbuchstaben (x-Hoehe 4)
    "a": [[(0.5, 4), (3, 4), (4, 3), (4, 0)], [(4, 2.2), (1, 2.2), (0, 1.5), (0, 0.7), (1, 0), (4, 0)]],
    "e": [[(0, 2), (4, 2), (4, 3), (3, 4), (1, 4), (0, 3), (0, 1), (1, 0), (4, 0)]],
    "h": [[(0, 6), (0, 0)], [(0, 3), (1, 4), (3, 4), (4, 3), (4, 0)]],
    "i": [[(2, 0), (2, 4)], [(2, 5.4), (2, 5.8)]],
    "s": [[(4, 4), (1, 4), (0, 3.2), (1, 2), (3, 2), (4, 1), (3, 0), (0, 0)]],
    "t": [[(1.5, 6), (1.5, 1), (2.5, 0), (3.5, 0)], [(0, 4), (3.5, 4)]],
    "+": [[(2, 1), (2, 5)], [(0, 3), (4, 3)]],
    "-": [[(0, 3), (4, 3)]],
    " ": [],
}

COPYRIGHT = ("(C) 2026", "Matthias Steis")     # Bestueckungsdruck unten links, zwei Zeilen

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

silk_items = [
    # U1 mit Kerbe rechts
    silk_rect(UX - 9.6, UY - 1.9, UX + 9.6, UY + 1.9),
    Point(UX + 9.6, UY).buffer(0.9 + SILK_W / 2, 24).difference(Point(UX + 9.6, UY).buffer(0.9 - SILK_W / 2, 24)).intersection(
        Polygon([(UX + 8.5, UY - 2), (UX + 9.6, UY - 2), (UX + 9.6, UY + 2), (UX + 8.5, UY + 2)])),
    silk_text("U1", UX, UY),
    silk_text("C2", 10.7, -4.35),
    # C1 liegend, Plus links
    silk_rect(-28.4, -6.9, -20.2, 9.7), silk_text("C1", -24.3, 3.0), silk_text("1000", -24.3, 1.0, 1.0),
    silk_text("+", -28.3, -8.0),
    # F1 und D2 (stehend, Kreis um den Koerper, Strich an der Kathode)
    silk_text("F1", -12.5, -15.46), silk_circle(-10.0, -18.0, 1.85), silk_text("D2", -7.6, -15.46),
    LineString([(-11.3, -10.6), (-8.7, -10.6)]).buffer(SILK_W, 8),
    silk_text("USB IN", -20.73, -20.6, 1.0), silk_text("-", -22.0, -15.9, 1.0), silk_text("+", -19.46, -15.9, 1.0),
    # D1 stehend
    silk_circle(-16.0, 19.42, 1.85), silk_text("D1", -19.3, 19.42),
    LineString([(-17.2, 26.0), (-14.9, 26.0)]).buffer(SILK_W, 8),
    # R1, R5, C3
    silk_text("R1", -4.8, 9.3), silk_text("R5", -11.6, 13.2), silk_text("C3", -4.9, 13.2),
    silk_text("FSR", -10.73, 18.9),
    # LED
    silk_text("LED", -24.46, 16.2), silk_text("+", -27.0, 13.8, 1.0), silk_text("G", -24.46, 13.8, 1.0),
    silk_text("D", -21.92, 13.8, 1.0),
    # ESP-Anschluss
    silk_text("ESP32 GPIO", -2.0, 24.2, 1.0),
    # Mikrofon (Umriss des Moduls)
    silk_circle(MIC_C[0], MIC_C[1], 7.1), silk_text("MIC", MIC_C[0], MIC_C[1], 1.0),
    # ARGB
    silk_text("ARGB", AX, 10.0, 1.0), silk_text("R3", 25.0, 7.1), silk_text("R4", 20.5, 3.8),
    silk_text("5V", 27.4, 0.0, 1.0), silk_text("D", 27.4, P, 1.0), silk_text("G", 27.4, 3 * P, 1.0),
    LineString([(AX - 0.42, 2 * P - 0.42), (AX + 0.42, 2 * P + 0.42)]).buffer(SILK_W / 2, 8),
    # Copyright
    silk_text("KOPFHOERER V2", 6.0, -17.0, 1.0),
    silk_text(COPYRIGHT[0], 6.0, -18.9, 1.0), silk_text(COPYRIGHT[1], 6.0, -20.1, 1.0),
]
J7L = ["5V", "G", "3V3", "0", "3", "23", "19", "20", "21", "G"]
for i, t in enumerate(J7L):
    silk_items.append(silk_text(t, J7X[i], 28.6, 0.9))
for n, t in (("GND", "G"), ("VDD", "3V3"), ("SD", "SD")):
    silk_items.append(silk_text(t, at("J6." + n)[0], 28.6, 0.9))
for n in ("LR", "WS", "SCK"):
    silk_items.append(silk_text(n, at("J6." + n)[0], J6Y2 + 1.8, 0.8))
silk = unary_union(silk_items)
silk = silk.difference(unary_union([mask_open, mask_open_top_extra]).buffer(0.15)).intersection(board.buffer(-0.3))
for m in MOUNT:
    silk = silk.difference(Point(m).buffer(MOUNT_DRILL / 2 + 0.3))

write_layer(f"{PRE}-B_Cu.gbl", "Copper,L2,Bot", [pour] + [g for k, g in net_cu.items() if k != "GND"])
write_layer(f"{PRE}-F_Cu.gtl", "Copper,L1,Top", [unary_union([pad_all] + [track_shape(t) for t in top_tracks] +
                                                               [smd_shape(n) for n in smd])])
write_layer(f"{PRE}-F_Mask.gts", "Soldermask,Top", [unary_union([mask_open, mask_open_top_extra])], "Negative")
write_layer(f"{PRE}-B_Mask.gbs", "Soldermask,Bot", [mask_open], "Negative")
write_layer(f"{PRE}-F_SilkS.gto", "Legend,Top", [silk])
write_outline(f"{PRE}-Edge_Cuts.gm1")

def write_drill(fname, items, plated):
    tools = sorted({d for d, _, _ in items})
    E = ["M48", f"; DRILL file gen_platine_v2.py, {'PTH' if plated else 'NPTH'}",
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

write_drill(f"{PRE}-PTH.drl", [(p["drill"], p["x"], p["y"]) for p in pads.values()], True)
write_drill(f"{PRE}-NPTH.drl", [(MOUNT_DRILL, x, y) for x, y in MOUNT], False)

import zipfile
FAB = [f"{PRE}-F_Cu.gtl", f"{PRE}-B_Cu.gbl", f"{PRE}-F_Mask.gts", f"{PRE}-B_Mask.gbs",
       f"{PRE}-F_SilkS.gto", f"{PRE}-Edge_Cuts.gm1", f"{PRE}-PTH.drl", f"{PRE}-NPTH.drl"]
with zipfile.ZipFile(os.path.join(OUT, "platine_v2_gerber.zip"), "w", zipfile.ZIP_DEFLATED) as z:
    for f in FAB:
        zi = zipfile.ZipInfo(f, date_time=(2026, 1, 1, 0, 0, 0))
        zi.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(zi, open(os.path.join(OUT, f), "rb").read())

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
    dr.line([tf(*c) for c in board.exterior.coords], fill=0, width=max(1, int(0.15 * s)))
    return im

# 1:1-Aetzvorlage, 600 dpi, Bauteilseiten-Ansicht (fuer Tonertransfer NICHT spiegeln)
mask = draw_copper_mask(600)
mask.save(os.path.join(OUT, "platine_v2_aetzvorlage_600dpi.png"), dpi=(600, 600))
a4 = Image.new("1", (4961, 7016), 1)
a4.paste(mask, (600, 600))
a4.paste(mask, (600, 600 + mask.height + 500))
d = ImageDraw.Draw(a4)
try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 60)
except OSError:
    font = ImageFont.load_default()
ty = 600 + 2 * mask.height + 500 + 120
d.text((600, ty), "Kopfhoererstaender Platine v2, Kupferseite, Ansicht von der Bauteilseite, 1:1 (nicht skalieren).",
       fill=0, font=font)
d.text((600, ty + 100), "Tonertransfer: so drucken wie hier (nicht spiegeln). Pruefmass: Platine 64,0 x 52,0 mm.",
       fill=0, font=font)
d.text((600, ty + 200), "Oben (Bauteilseite) als Draht: 3V3 von ESP-Anschluss Pin 3 zum Mikrofon Pin 6.",
       fill=0, font=font)
a4.convert("L").save(os.path.join(OUT, "platine_v2_aetzvorlage_A4.pdf"), resolution=600)

# Bestueckungsplan (farbig, Draufsicht)
s, W, H, tf = raster(800)
im = Image.new("RGB", (W, H), "white")
dr = ImageDraw.Draw(im)
dr.polygon([tf(*c) for c in board.exterior.coords], fill="#2f6b3a", outline="black")
draw_geom(dr, pour, tf, "#c8a24a")
colors = {"+5V": "#d9534f", "VIN": "#a83232", "VESP": "#f08a24", "IO3": "#e6c229", "OUT": "#9b6bd6",
          "DIN": "#2e9e5b", "ARGB_D": "#8e44ad", "IO23": "#6c5ce7", "IO0": "#17a2b8", "3V3": "#e83e8c",
          "IO19": "#3b7dd8", "IO20": "#5a9be8", "IO21": "#2a5db0"}
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
    fs = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", int(0.7 * s))
except OSError:
    f1 = f2 = fs = ImageFont.load_default()

def label(x, y, t, f=f2, fill="white"):
    X, Y = tf(x, y)
    dr.text((X, Y), t, fill=fill, font=f, anchor="mm", stroke_width=3, stroke_fill="black")

def box(x0, y0, x1, y1, t, col="#ffffff"):
    a, b = tf(x0, y1), tf(x1, y0)
    dr.rectangle([a, b], outline=col, width=4)
    if t:
        label((x0 + x1) / 2, (y0 + y1) / 2, t, f1)

box(UX - 9.6, UY - 3.0, UX + 9.6, UY + 3.0, "U1 74AHCT125")
X, Y = tf(UX + 9.6, UY)
dr.ellipse([X - 0.9 * s, Y - 0.9 * s, X + 0.9 * s, Y + 0.9 * s], outline="white", width=4)
box(-28.4, -6.9, -20.2, 9.7, "")
label(-24.3, 1.5, "C1 1000µF", f2)
label(-24.2, -9.9, "+ C1 −", fs)
label(10.9, -4.35, "C2", f1)
label(-12.5, -15.4, "F1", f1)
label(-7.4, -15.4, "D2", f1)
label(-20.7, -20.5, "USB IN  − / +", fs)
label(-18.7, 19.4, "D1", f1)
label(-4.6, 9.3, "R1", f1)
label(-11.8, 13.2, "R5", f1)
label(-4.6, 13.2, "C3", f1)
label(-10.7, 19.0, "FSR", f2)
label(-24.46, 16.3, "LED  + G D", f2)
label(-2.0, 24.0, "ESP32 (J7)", f2)
for i, t in enumerate(J7L):
    label(J7X[i], 28.6, t, fs)
X, Y = tf(*MIC_C)
dr.ellipse([X - 7 * s, Y - 7 * s, X + 7 * s, Y + 7 * s], outline="white", width=4)
label(MIC_C[0], MIC_C[1], "Mikrofon INMP441", fs)
for n, (x, y, net) in J6.items():
    label(x, y + (1.9 if y == EY else -1.6), n, fs)
label(AX - 0.4, 10.2, "ARGB 90°", fs)
label(27.3, 0.0, "5V nc", fs)
label(27.4, P, "D", f2)
label(27.4, 3 * P, "G", f2)
label(25.0, 7.2, "R3", f1)
label(20.4, 3.8, "R4", f1)
for k in ("D1.K", "D2.K"):   # Kathode (Ring) am eckigen Pad
    X, Y = tf(*at(k))
    r = 1.6 * s
    dr.ellipse([X - r, Y - r, X + r, Y + r], outline="white", width=5)
label(-13.6, 23.2, "K", fs)
label(-8.0, -11.2, "K", fs)
im.save(os.path.join(OUT, "platine_v2_bestueckung.png"))
print("ok", OUT)
