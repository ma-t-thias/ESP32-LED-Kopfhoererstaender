"""
build_staender_blend.py - baut die Blender-Szene des LED-Kopfhoererstaenders (Stand Sockel v9 / Auflage v3)

Aufruf (Blender 4.2+ / 5.x):
    blender --background --python build_staender_blend.py -- [--stl-dir DIR] [--out FILE.blend] [--render FILE.png] [--demo]
            [--display-png FILE.png]

    --stl-dir  Ordner mit den STL-Dateien (Standard: Unterordner "stl" neben diesem Skript)
    --out      Ziel-.blend (Standard: Staender_v8.blend neben diesem Skript)
    --render   optional: Eevee-Render als PNG
    --demo     Regenbogen auf die LEDs legen (sonst alle LEDs aus/dunkel)
    --display-png  Bild fuer das Display (Standard: Display_Grundbild.png neben diesem Skript).
               Erzeugen mit der PC-Simulation (gleicher Code wie die Firmware):
               .pio/build/sim/program logo Display_Grundbild.png 0 "V1.0.0"

Die STL-Dateien (Export aus SolidCraft in Montageposition) und die .blend sind nicht im Repo.

Einheiten: 1 Blender-Einheit = 1 mm (unit_settings.scale_length = 0.001).
Koordinaten wie im CAD: Ursprung Sockelmitte unten, +Y Richtung Display (vorne), +Z oben.

Collections:
    Tragteile  - PLA-Teile (Sockel, Boden, Saeule, Auflage, Druckplatte, Display-Gehaeuse, Deckel, TPU-Fuesse)
    Inlays     - Fluor-PLA-Lichtteile (Leuchtring, Augen, Saeulen-Inlays, Turm-Inlay)
    Elektronik - Platine v2 mit Bauteil-Bloecken, ESP32-Modul, LCD, USB-C
    LEDs       - LED_000 ... LED_143 in Datenreihenfolge (LED_COUNT 144, include/config.h)

LED-Farbe: jede LED hat obj.color (RGBA). Alle LEDs teilen das Material "LED_Emission":
    Object Info.Color -> Emission (Staerke 8). Farbe setzen = obj.color aendern, z. B. mit set_leds()
    oder live mit live_leds.py.
    Jede LED hat zusaetzlich ein Kind-Punktlicht "LED_nnn_Licht", dessen Farbe per Treiber obj.color folgt.

LED-Zuordnung (Datenreihenfolge).  ANNAHMEN sind mit * markiert - bitte mit dem echten Aufbau abgleichen.
    WS2812B 144 LED/m -> Raster 6,944 mm.

    | Index       | Anzahl | Ort                                    | Lage / Richtung                                              |
    |-------------|--------|----------------------------------------|--------------------------------------------------------------|
    | 000 - 068   | 69     | Sockel, LED-Rippe hinter Leuchtring    | Ellipse a 83,5 / b 69,5 um (0, 12), z 3,6; *Start hinten     |
    |             |        | (beleuchtet auch die Augen L/R vorne)  |  Mitte (y min), *Laufrichtung gegen Uhrzeigersinn von oben   |
    | 069 - 092   | 24     | Saeule, Lichtkammer hinter Front-Inlay | Bogen r 399 um (y -378,5, z 233) in x = 0, z 42 -> 199,      |
    |             |        |                                        |  *von unten nach oben, LEDs zeigen nach vorne/aussen         |
    | 093 - 143   | 51     | *nicht verbaut (Rest des 1-m-Streifens)| als ausgeblendete Objekte unter dem Sockel abgelegt          |

    Die Anzahl der Ring- und Saeulen-LEDs ergibt sich aus Umfang/Bogenlaenge und Raster; RING_COUNT und
    COLUMN_COUNT unten anpassen, wenn der Streifen anders geschnitten/verlegt wird.
"""

import bpy
import math
import os
import sys

# ----------------------------------------------------------------------------- Parameter
LED_COUNT = 144                 # include/config.h
LED_PITCH = 1000.0 / 144.0      # mm
RING_A, RING_B, RING_CY, RING_Z = 83.5, 69.5, 12.0, 3.6
RING_COUNT = 69
COL_CY, COL_CZ, COL_R = -378.5, 233.0, 399.0   # Saeulenbogen (Weltkoordinaten)
COL_Z0, COL_Z1 = 42.0, 199.0
COLUMN_COUNT = 24

PLA_DARK = (0.035, 0.037, 0.042, 1.0)
PARTS = [
    # Datei, Objektname, Collection, Material
    ("Sockel_oval.stl", "Sockel", "Tragteile", "PLA_Schwarz"),
    ("Boden_Diffusor.stl", "Boden", "Tragteile", "PLA_Schwarz"),
    ("Saeule.stl", "Saeule_B_Rauten", "Tragteile", "PLA_Schwarz"),
    ("Auflage.stl", "Auflage", "Tragteile", "PLA_Schwarz"),
    ("Druckplatte.stl", "Druckplatte_FSR", "Tragteile", "PLA_Grau"),
    ("Display_Gehaeuse.stl", "Display_Gehaeuse", "Tragteile", "PLA_Schwarz"),
    ("ESP32_Lid_Mikro_hochkant.stl", "Display_Deckel", "Tragteile", "PLA_Schwarz"),
    ("TPU_Fuss.stl", "TPU_Fuss_1", "Tragteile", "TPU_Schwarz"),
    ("Inlay_Leuchtring.stl", "Inlay_Leuchtring", "Inlays", "Fluor_PLA"),
    ("Inlay_Augen_links.stl", "Inlay_Augen_links", "Inlays", "Fluor_PLA"),
    ("Inlay_Augen_rechts.stl", "Inlay_Augen_rechts", "Inlays", "Fluor_PLA"),
    ("Diffusor.stl", "Inlay_Saeule_Front", "Inlays", "Fluor_PLA"),
    ("Inlay_Seite_links.stl", "Inlay_Seite_links", "Inlays", "Fluor_PLA"),
    ("Inlay_Seite_rechts.stl", "Inlay_Seite_rechts", "Inlays", "Fluor_PLA"),
    ("Inlay_Turm_hinten.stl", "Inlay_Turm_hinten", "Inlays", "Fluor_PLA"),
    ("PCB2_Platine.stl", "Platine_v2", "Elektronik", "PCB_Gruen"),
    ("PCB2_Bauteile.stl", "Platine_v2_Bauteile", "Elektronik", "Bauteil_Schwarz"),
    ("PCB2_C1_Elko.stl", "Platine_v2_C1_Elko", "Elektronik", "Elko_Blau"),
    ("PCB2_J4_Stecker.stl", "Platine_v2_J4_Stecker", "Elektronik", "Stecker_Weiss"),
    ("ESP32_Modul.stl", "ESP32_C6_Modul", "Elektronik", "Bauteil_Schwarz"),
    ("Display_LCD.stl", "Display_LCD", "Elektronik", "LCD_Glas"),
    ("USB_C_Platine_v2.stl", "USB_C_Platine", "Elektronik", "PCB_Blau"),
    ("USB_C_Buchse_v2.stl", "USB_C_Buchse", "Elektronik", "Metall"),
]
# *Annahme: 4 TPU-Fuesse symmetrisch (nur einer ist im CAD platziert, bei x 55 / y 48; Spiegelachse y = 12)
FOOT_COPIES = [(True, False), (False, True), (True, True)]   # (an x = 0 spiegeln, an y = 12 spiegeln)


# ----------------------------------------------------------------------------- Hilfen
def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    here = os.path.dirname(os.path.abspath(__file__))
    opt = {"stl_dir": os.path.join(here, "stl"), "out": os.path.join(here, "Staender_v8.blend"),
           "render": None, "demo": False,
           "display_png": os.path.join(here, "Display_Grundbild.png")}  # aus: program logo Display_Grundbild.png
    i = 0
    while i < len(a):
        if a[i] == "--display-png": opt["display_png"] = a[i + 1]; i += 2; continue
        if a[i] == "--stl-dir": opt["stl_dir"] = a[i + 1]; i += 2
        elif a[i] == "--out": opt["out"] = a[i + 1]; i += 2
        elif a[i] == "--render": opt["render"] = a[i + 1]; i += 2
        elif a[i] == "--demo": opt["demo"] = True; i += 1
        else: i += 1
    return opt


def collection(name):
    c = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if c.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(c)
    return c


def principled(name, color, rough=0.5, metal=0.0, transm=0.0, emit=None, emit_strength=0.0, alpha=1.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = color
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    for key in ("Transmission Weight", "Transmission"):
        if key in b.inputs: b.inputs[key].default_value = transm; break
    if emit is not None:
        for key in ("Emission Color", "Emission"):
            if key in b.inputs: b.inputs[key].default_value = emit; break
        b.inputs["Emission Strength"].default_value = emit_strength
    b.inputs["Alpha"].default_value = alpha
    m.diffuse_color = color
    return m


def make_materials():
    principled("PLA_Schwarz", PLA_DARK, rough=0.55)
    principled("PLA_Grau", (0.18, 0.18, 0.19, 1), rough=0.5)
    principled("TPU_Schwarz", (0.02, 0.02, 0.02, 1), rough=0.9)
    m = principled("Fluor_PLA", (0.92, 0.94, 0.96, 1), rough=0.35, transm=0.0,
                   emit=(0.85, 0.9, 1.0, 1), emit_strength=0.05, alpha=0.35)
    # halbtransparent, damit die LEDs dahinter sichtbar sind (Simulation des Lichtdurchgangs)
    if hasattr(m, "surface_render_method"): m.surface_render_method = "BLENDED"
    elif hasattr(m, "blend_method"): m.blend_method = "BLEND"
    principled("PCB_Gruen", (0.03, 0.25, 0.08, 1), rough=0.4)
    principled("PCB_Blau", (0.03, 0.08, 0.35, 1), rough=0.4)
    principled("Bauteil_Schwarz", (0.03, 0.03, 0.03, 1), rough=0.4)
    principled("Elko_Blau", (0.05, 0.15, 0.6, 1), rough=0.3)
    principled("Stecker_Weiss", (0.85, 0.85, 0.82, 1), rough=0.5)
    principled("LCD_Glas", (0.01, 0.01, 0.015, 1), rough=0.05)
    principled("Metall", (0.7, 0.7, 0.72, 1), rough=0.25, metal=1.0)
    # LED: Object Info.Color -> Emission
    m = bpy.data.materials.get("LED_Emission") or bpy.data.materials.new("LED_Emission")
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); out.location = (300, 0)
    em = nt.nodes.new("ShaderNodeEmission"); em.location = (100, 0)
    em.inputs["Strength"].default_value = 8.0
    oi = nt.nodes.new("ShaderNodeObjectInfo"); oi.location = (-120, 0)
    nt.links.new(oi.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])


def import_stl(path):
    before = set(bpy.data.objects)
    if hasattr(bpy.ops.wm, "stl_import"):
        bpy.ops.wm.stl_import(filepath=path, global_scale=1.0)
    else:
        bpy.ops.import_mesh.stl(filepath=path, global_scale=1.0)
    new = [o for o in bpy.data.objects if o not in before]
    return new[0] if new else None


def move_to(obj, coll):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    coll.objects.link(obj)


def assign(obj, matname):
    obj.data.materials.clear()
    obj.data.materials.append(bpy.data.materials[matname])
    for p in obj.data.polygons:
        p.use_smooth = False


# ----------------------------------------------------------------------------- LEDs
def led_positions():
    """Liste (Position, Richtung) in Datenreihenfolge; Rest = None (nicht verbaut)."""
    res = []
    # Ring: gleichmaessig nach Bogenlaenge, Start hinten Mitte (y min), gegen Uhrzeigersinn von oben
    n = 4000
    pts = [(RING_A * math.cos(-math.pi / 2 + 2 * math.pi * k / n),
            RING_CY + RING_B * math.sin(-math.pi / 2 + 2 * math.pi * k / n)) for k in range(n + 1)]
    cum = [0.0]
    for k in range(n):
        cum.append(cum[-1] + math.dist(pts[k], pts[k + 1]))
    total = cum[-1]
    step = total / RING_COUNT
    j = 0
    for i in range(RING_COUNT):
        s = i * step
        while cum[j + 1] < s: j += 1
        t = (s - cum[j]) / max(cum[j + 1] - cum[j], 1e-9)
        x = pts[j][0] + t * (pts[j + 1][0] - pts[j][0])
        y = pts[j][1] + t * (pts[j + 1][1] - pts[j][1])
        nx, ny = x / RING_A ** 2, (y - RING_CY) / RING_B ** 2      # Ellipsen-Normale
        l = math.hypot(nx, ny)
        res.append(((x, y, RING_Z), (nx / l, ny / l, 0.0)))
    # Saeule: Bogen r = COL_R, z von COL_Z0 nach COL_Z1, Abstand = Raster
    phi0 = math.asin((COL_CZ - COL_Z0) / COL_R)
    for i in range(COLUMN_COUNT):
        phi = phi0 - i * LED_PITCH / COL_R
        y = COL_CY + COL_R * math.cos(phi)
        z = COL_CZ - COL_R * math.sin(phi)
        res.append(((0.0, y, z), (0.0, math.cos(phi), -math.sin(phi))))
    while len(res) < LED_COUNT:
        res.append(None)
    return res[:LED_COUNT]


def make_leds(coll):
    from mathutils import Vector
    mat = bpy.data.materials["LED_Emission"]
    mesh = bpy.data.meshes.get("LED_5050") or bpy.data.meshes.new("LED_5050")
    if not mesh.vertices:
        w, d = 2.5, 0.8         # 5 x 5 x 1,6 mm, lokale +Z = Abstrahlrichtung
        v = [(-w, -w, -d), (w, -w, -d), (w, w, -d), (-w, w, -d), (-w, -w, d), (w, -w, d), (w, w, d), (-w, w, d)]
        f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        mesh.from_pydata(v, [], f)
        mesh.materials.append(mat)
    reserve = 0
    for i, pd in enumerate(led_positions()):
        name = f"LED_{i:03d}"
        o = bpy.data.objects.get(name) or bpy.data.objects.new(name, mesh)
        if o.name not in coll.objects: coll.objects.link(o)
        o.color = (0.02, 0.02, 0.02, 1.0)
        o["led_index"] = i
        if pd is None:
            o.location = (-60.0 + (reserve % 26) * LED_PITCH, -120.0 - (reserve // 26) * 8.0, 0.0)
            o["segment"] = "nicht_verbaut"
            o.hide_viewport = o.hide_render = True
            reserve += 1
            continue
        (p, dirv) = pd
        o.location = p
        o.rotation_mode = "QUATERNION"
        o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(Vector(dirv))
        o["segment"] = "ring" if i < RING_COUNT else "saeule"
        add_glow(o, coll, 2.5 if i < RING_COUNT else 9.0)   # Saeule: Licht direkt hinter das Front-Inlay
    return reserve


GLOW_POWER = 3000.0  # "W" je LED-Punktlicht; hoch, weil 1 BU = 1 mm (Abstandsgesetz in BU)


def add_glow(led, coll, offset=2.5):
    """Kleines Punktlicht als Kind der LED; Farbe folgt led.color ueber Treiber (nur obj.color setzen)."""
    name = led.name + "_Licht"
    ld = bpy.data.lights.get(name) or bpy.data.lights.new(name, "POINT")
    ld.energy = GLOW_POWER
    ld.shadow_soft_size = 1.5
    if hasattr(ld, "use_shadow"): ld.use_shadow = False
    lo = bpy.data.objects.get(name) or bpy.data.objects.new(name, ld)
    if lo.name not in coll.objects: coll.objects.link(lo)
    lo.parent = led
    lo.location = (0.0, 0.0, offset)
    lo.hide_select = True
    for k in range(3):
        fc = ld.driver_add("color", k)
        drv = fc.driver; drv.type = "AVERAGE"
        v = drv.variables[0] if drv.variables else drv.variables.new()
        v.type = "SINGLE_PROP"
        v.targets[0].id_type = "OBJECT"; v.targets[0].id = led
        v.targets[0].data_path = f"color[{k}]"


# Aktive Flaeche des 1,9"-LCD (170 x 320) aus Display_LCD.stl: Mitte, Normale, Bildoberkante (Schraege)
LCD_CENTER = (0.0, 73.74, 36.91)
LCD_NORMAL = (0.0, 0.94, 0.34)        # zeigt nach vorne/oben (zum Betrachter)
LCD_UP = (0.0, -0.34, 0.94)           # Richtung Bildoberkante
LCD_W, LCD_H = 23.1, 43.1             # mm


def add_display(png, coll):
    """Flaeche knapp vor dem LCD mit dem Display-Bild als leuchtende Textur (Objekt "Display_Bild").
    Erwartet ein Hochkant-Bild 170 x 320 (oder ein Vielfaches); fehlt es, entfaellt die Flaeche."""
    from mathutils import Vector
    if not png or not os.path.exists(png):
        print(f"[staender] kein Display-Bild ({png}); erzeugen mit: "
              ".pio/build/sim/program logo Display_Grundbild.png 0")
        return False
    img = bpy.data.images.load(png, check_existing=True)
    if img.size[0] >= img.size[1]:
        print(f"[staender] Display-Bild {png} ist quer ({img.size[0]} x {img.size[1]}), "
              "erwartet hochkant (Drehung 0) - Display bleibt leer")
        return False
    c = Vector(LCD_CENTER) + Vector(LCD_NORMAL) * 0.15
    up = Vector(LCD_UP) * (LCD_H / 2)
    right = Vector(LCD_UP).cross(Vector(LCD_NORMAL)).normalized() * (LCD_W / 2)   # rechts aus Sicht des Betrachters
    v = [c - right - up, c + right - up, c + right + up, c - right + up]
    me = bpy.data.meshes.new("Display_Bild")
    me.from_pydata([tuple(p) for p in v], [], [(0, 1, 2, 3)])
    uv = me.uv_layers.new(name="UV")
    for li, (uu, vv) in enumerate([(0, 0), (1, 0), (1, 1), (0, 1)]):
        uv.data[li].uv = (uu, vv)
    ob = bpy.data.objects.new("Display_Bild", me)
    coll.objects.link(ob)
    m = bpy.data.materials.new("Display_Bild")
    m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial"); out.location = (400, 0)
    em = nt.nodes.new("ShaderNodeEmission"); em.location = (200, 0)
    em.inputs["Strength"].default_value = 1.5
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.location = (-150, 0)
    tex.image = img
    tex.interpolation = "Closest"           # scharfe Pixel wie am echten Display
    nt.links.new(tex.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    me.materials.append(m)
    return True


def set_leds(colors):
    """colors: Liste von (r, g, b) 0..1 oder 0..255, Index = LED-Datenindex."""
    for i, c in enumerate(colors[:LED_COUNT]):
        o = bpy.data.objects.get(f"LED_{i:03d}")
        if o is None: continue
        r, g, b = c[:3]
        if max(r, g, b) > 1.0: r, g, b = r / 255.0, g / 255.0, b / 255.0
        o.color = (r, g, b, 1.0)


def rainbow():
    import colorsys
    return [colorsys.hsv_to_rgb(i / LED_COUNT, 1.0, 1.0) for i in range(LED_COUNT)]


# ----------------------------------------------------------------------------- Szene
def scene_setup():
    from mathutils import Vector
    sc = bpy.context.scene
    us = sc.unit_settings
    us.system = "METRIC"; us.scale_length = 0.001; us.length_unit = "MILLIMETERS"
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try: sc.render.engine = eng; break
        except TypeError: pass
    sc.render.resolution_x, sc.render.resolution_y = 1600, 1200
    transforms = [i.identifier for i in sc.view_settings.bl_rna.properties["view_transform"].enum_items]
    sc.view_settings.view_transform = "AgX" if "AgX" in transforms else "Filmic"
    world = sc.world or bpy.data.worlds.new("World"); sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.02, 0.022, 0.03, 1); bg.inputs["Strength"].default_value = 0.6
    # Kamera 3/4 von vorne rechts oben
    cam_data = bpy.data.cameras.new("Kamera"); cam_data.lens = 50; cam_data.clip_start = 1; cam_data.clip_end = 20000
    cam = bpy.data.objects.new("Kamera", cam_data); sc.collection.objects.link(cam)
    cam.location = (430.0, 560.0, 330.0)
    d = Vector((0.0, 15.0, 125.0)) - cam.location
    cam.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    sc.camera = cam
    # Licht: Sonne + Flaechenlicht (BU = mm -> Area-Energie hoch)
    sun = bpy.data.objects.new("Sonne", bpy.data.lights.new("Sonne", "SUN")); sc.collection.objects.link(sun)
    sun.data.energy = 3.0; sun.rotation_euler = (math.radians(50), 0, math.radians(140))
    fill = bpy.data.objects.new("Fuelllicht", bpy.data.lights.new("Fuelllicht", "AREA")); sc.collection.objects.link(fill)
    fill.data.size = 400; fill.data.energy = 2.0e6; fill.location = (-350, 300, 400)
    fill.rotation_euler = (Vector((0, 0, 80)) - fill.location).to_track_quat("-Z", "Y").to_euler()
    # Boden
    bpy.ops.mesh.primitive_plane_add(size=1500, location=(0, 0, -4.0))
    pl = bpy.context.active_object; pl.name = "Tisch"
    pl.data.materials.append(principled("Tisch", (0.25, 0.25, 0.27, 1), rough=0.7))


def build(opt):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene_setup()
    make_materials()
    cols = {n: collection(n) for n in ("Tragteile", "Inlays", "Elektronik", "LEDs")}
    missing = []
    for fn, name, cname, mat in PARTS:
        p = os.path.join(opt["stl_dir"], fn)
        if not os.path.exists(p): missing.append(fn); continue
        o = import_stl(p)
        o.name = name; o.data.name = name
        move_to(o, cols[cname]); assign(o, mat)
        if name == "TPU_Fuss_1":
            for k, (mx, my) in enumerate(FOOT_COPIES, start=2):
                c = o.copy(); c.name = f"TPU_Fuss_{k}"
                c.scale = (-1.0 if mx else 1.0, -1.0 if my else 1.0, 1.0)
                c.location = (0.0, 2 * RING_CY if my else 0.0, 0.0)   # Spiegelung an x = 0 bzw. y = 12
                cols[cname].objects.link(c)
    add_display(opt["display_png"], cols["Elektronik"])
    reserve = make_leds(cols["LEDs"])
    set_leds(rainbow() if opt["demo"] else [(0.02, 0.02, 0.02)] * LED_COUNT)
    bpy.ops.wm.save_as_mainfile(filepath=opt["out"])
    if opt["render"]:
        bpy.context.scene.render.filepath = opt["render"]
        bpy.ops.render.render(write_still=True)
        bpy.ops.wm.save_as_mainfile(filepath=opt["out"])
    print(f"[staender] fertig: {opt['out']}  LEDs: ring {RING_COUNT}, saeule {COLUMN_COUNT}, "
          f"nicht verbaut {reserve}; fehlend: {missing}")


if __name__ == "__main__":
    build(args())
