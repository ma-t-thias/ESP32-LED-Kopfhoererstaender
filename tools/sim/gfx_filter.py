# env:sim: Von "GFX Library for Arduino" nur den Zeichenteil bauen. Die Display- und
# Bus-Treiber brauchen ESP-Hardware und lassen sich auf dem PC nicht uebersetzen.
import os

Import("env")

KEEP = {"Arduino_GFX.cpp", "Arduino_G.cpp", "Arduino_DataBus.cpp"}


def only_drawing(env, node):
    path = node.get_path().replace("\\", "/")
    if "GFX Library for Arduino" in path or "GFX_Library_for_Arduino" in path:
        if os.path.basename(path) not in KEEP:
            return None
    return node


env.AddBuildMiddleware(only_drawing)
