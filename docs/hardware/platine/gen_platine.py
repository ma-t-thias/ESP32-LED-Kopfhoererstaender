#!/usr/bin/env python3
"""Einseitige Platine fuer den Sockel des Kopfhoererstaenders.

Erzeugt Gerber (Kupfer unten, Umriss), Excellon-Bohrdatei, eine 1:1-Aetzvorlage (PNG/PDF)
und einen Bestueckungsplan. Koordinaten in mm, Draufsicht von der Bauteilseite,
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
OUTLINE = [(-7.5, -10.5), (23.5, -10.5), (30.5, 0), (33, 4), (33, 6), (6, 6), (6, 26),
           (-6, 26), (-6, 6), (-13.5, 6), (-13.5, 0)]
MOUNT = [(-5, -6), (27.5, -1), (0, 23)]      # M2,5
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
UX, UY = 10.0, -3.0
U1 = {}
for i in range(7):
    U1[1 + i] = (UX + 3 * P - i * P, UY + 3.81)     # Pins 1..7 oben, von rechts nach links
    U1[8 + i] = (UX - 3 * P + i * P, UY - 3.81)     # Pins 8..14 unten, von links nach rechts
u1_net = {1: "GND", 2: "IO3", 3: "OUT", 4: "GND", 5: "GND", 6: None, 7: "GND",
          8: None, 9: "GND", 10: "GND", 11: None, 12: "GND", 13: "GND", 14: "+5V"}
for n, (x, y) in U1.items():
    pad(f"U1.{n}", x, y, u1_net[n], d=1.8, drill=0.8, shape="square" if n == 1 else "oval")

# D1 1N5819 stehend: Anode unten (+5V), Kathode oben (VESP)
pad("D1.A", -1.0, -4.4, "+5V", d=2.0, drill=1.0)
pad("D1.K", -1.0, 0.68, "VESP", d=2.0, drill=1.0, shape="square")
# C2 100 nF
pad("C2.1", 20.16, -6.81, "+5V")
pad("C2.2", 22.70, -6.81, "GND")
# R1 330 Ohm stehend
pad("R1.1", 20.5, -1.6, "OUT")
pad("R1.2", 20.5, 0.94, "DIN")
# Anschluesse LED-Streifen (rechts) und USB-C (links): grosse Pads fuer Litze 1 mm2
SX = 23.5
pad("STRIP.GND", SX, 3.4, "GND", d=2.6, drill=1.3, shape="square")
pad("STRIP.DIN", SX, -0.4, "DIN", d=2.0, drill=1.0)
pad("STRIP.5V", SX, -4.2, "+5V", d=2.6, drill=1.3)
pad("USB.GND", -11.5, 4.0, "GND", d=2.6, drill=1.3, shape="square")
pad("USB.5V", -11.5, 0.4, "+5V", d=2.6, drill=1.3)
# Zunge: ARGB-Eingang, R3/R4-Spannungsteiler, Anschluss ESP (2,54-mm-Raster)
pad("ARGB.G", 1.46, 8.0, "GND", shape="square")
pad("ARGB.D", 4.0, 8.0, "ARGB_D")
pad("R3.1", 4.0, 10.54, "ARGB_D")
pad("R3.2", 4.0, 13.08, "IO23")
pad("R4.1", 4.0, 15.62, "IO23")
pad("R4.2", 1.46, 15.62, "GND")
EY = 19.5
pad("ESP.VBUS", -3.81, EY, "VESP", shape="square")
pad("ESP.IO3", -1.27, EY, "IO3")
pad("ESP.GND", 1.27, EY, "GND")
pad("ESP.IO23", 3.81, EY, "IO23")

def at(n):
    return (pads[n]["x"], pads[n]["y"])

# Leiterbahnen: (Netz, Breite, Punkte)
tracks = [
    ("+5V", W_PWR, [at("USB.5V"), at("D1.A")]),
    ("+5V", W_PWR, [at("D1.A"), (UX + 3 * P + 1.6, -4.4), (SX - 1.4, -4.4), at("STRIP.5V")]),
    ("+5V", 1.0, [U1[14], (U1[14][0], -4.4)]),
    ("+5V", 1.0, [at("C2.1"), (20.16, -4.4)]),
    ("VESP", W_SIG + 0.2, [at("D1.K"), (-3.81, 0.68), at("ESP.VBUS")]),
    ("IO3", W_SIG, [U1[2], (U1[2][0], 3.0), (-1.27, 3.0), at("ESP.IO3")]),
    ("OUT", W_SIG, [U1[3], (U1[3][0], -1.6), at("R1.1")]),
    ("DIN", W_SIG, [at("R1.2"), at("STRIP.DIN")]),
    ("ARGB_D", W_SIG, [at("ARGB.D"), at("R3.1")]),
    ("IO23", W_SIG, [at("R3.2"), at("R4.1"), at("ESP.IO23")]),
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

def write_gerber(fname, func, geom, outline=False):
    L = ["%FSLAX46Y46*%", "%MOMM*%", f"%TF.FileFunction,{func}*%", "%TF.Part,Single*%",
         "%ADD10C,0.150*%", "G01*", "G75*"]
    if outline:
        L.append("D10*")
        pts = list(board.exterior.coords)
        L.append(f"X{fmt(pts[0][0])}Y{fmt(pts[0][1])}D02*")
        for x, y in pts[1:]:
            L.append(f"X{fmt(x)}Y{fmt(y)}D01*")
    else:
        # Kupfer: alle Polygone (Masseflaeche zuerst, sie hat Loecher fuer die anderen Netze)
        for p in polys(pour):
            gerber_poly(L, p)
        for k, g in net_cu.items():
            if k == "GND":
                continue
            for p in polys(g):
                gerber_poly(L, p)
        # Bohrloecher im Kupfer freistellen (hilft beim Aetzen als Koernerpunkt)
        L.append("%LPC*%")
        for p in pads.values():
            c = Point(p["x"], p["y"]).buffer(0.3, 16)
            L.append("G36*")
            pts = list(c.exterior.coords)
            L.append(f"X{fmt(pts[0][0])}Y{fmt(pts[0][1])}D02*")
            for x, y in pts[1:]:
                L.append(f"X{fmt(x)}Y{fmt(y)}D01*")
            L.append("G37*")
        L.append("%LPD*%")
    L.append("M02*")
    open(os.path.join(OUT, fname), "w").write("\n".join(L) + "\n")

write_gerber("platine-B_Cu.gbl", "Copper,L1,Bot", copper)
write_gerber("platine-Edge_Cuts.gm1", "Profile,NP", None, outline=True)

# Excellon
tools = sorted({p["drill"] for p in pads.values()} | {MOUNT_DRILL})
E = ["M48", "METRIC,TZ", "FMAT,2", "G90"]
for i, d in enumerate(tools, 1):
    E.append(f"T{i}C{d:.3f}")
E.append("%")
E.append("G05")
for i, d in enumerate(tools, 1):
    E.append(f"T{i}")
    for p in pads.values():
        if p["drill"] == d:
            E.append(f"X{p['x']:.3f}Y{p['y']:.3f}")
    if d == MOUNT_DRILL:
        for x, y in MOUNT:
            E.append(f"X{x:.3f}Y{y:.3f}")
E.append("T0")
E.append("M30")
open(os.path.join(OUT, "platine.drl"), "w").write("\n".join(E) + "\n")

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
label(-3.0, -2.0, "D1", f1)
label(21.5, -5.3, "C2", f1)
label(21.9, 0.0, "R1", f1)
label(5.3, 11.8, "R3", f1)
label(2.7, 17.0, "R4", f1)
for n, t in (("STRIP.GND", "GND"), ("STRIP.DIN", "DIN"), ("STRIP.5V", "+5V")):
    label(pads[n]["x"] + 3.6, pads[n]["y"], t)
label(-11.5, 6.9, "USB-C", f2, "black")
label(-8.6, 4.0, "GND"); label(-8.6, 0.4, "+5V")
label(2.7, 6.6, "ARGB G  D")
label(0, 21.2, "VBUS IO3 GND IO23", ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", int(0.7 * s)))
label(29.5, 5.0, "LED", f2)
im.save(os.path.join(OUT, "platine_bestueckung.png"))
print("ok", OUT)
