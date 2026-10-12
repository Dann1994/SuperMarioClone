"""Vista previa en miniatura de los niveles (un PNG por mundo), para revisar
el armado sin abrir el juego: un color por tile y un punto por entidad.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/level_scripts/preview.py <carpeta de salida> [mundo...]
"""
import json
import os
import sys

from PIL import Image, ImageDraw

PX = 4
COLORS = {
    '#': (150, 80, 30), 'S': (110, 60, 30), 'B': (200, 90, 40), 'C': (220, 150, 40), '?': (250, 190, 40),
    'D': (210, 100, 60), 'H': (250, 250, 160), 'o': (255, 220, 0), 'T': (40, 180, 40), 'F': (220, 220, 220),
    'L': (230, 40, 20), 'b': (240, 200, 120), '=': (214, 168, 116), 'X': (120, 120, 120), 't': (110, 210, 20),
    'r': (120, 70, 20), 'm': (240, 120, 60), 'n': (250, 210, 180), 'R': (250, 200, 160), 'K': (20, 20, 20),
    'c': (230, 70, 160), 'W': (100, 170, 250), 'w': (30, 60, 200),
}
BG = {"overworld": (92, 148, 252), "underground": (8, 8, 28), "castle": (0, 0, 0), "underwater": (20, 60, 160)}
ENT = {"goomba": (140, 60, 20), "koopa": (0, 160, 0), "buzzy": (40, 40, 160), "piranha": (0, 255, 0),
       "hammer_bro": (0, 255, 255), "lakitu": (255, 255, 255), "blooper": (255, 255, 255), "cheep": (255, 0, 0),
       "podoboo": (255, 128, 0), "firebar": (255, 160, 0), "bowser": (0, 120, 0), "axe": (180, 180, 255)}


def render(path):
    d = json.load(open(path))
    rows = d["rows"]
    w, h = len(rows[0]), len(rows)
    img = Image.new("RGB", (w * PX, h * PX), BG.get(d.get("world", "overworld")))
    draw = ImageDraw.Draw(img)
    for a in d.get("areas", []):
        draw.rectangle([a["start"] * PX, 0, (a["end"] + 1) * PX - 1, h * PX - 1],
                       fill=BG.get(a.get("world", d.get("world", "overworld"))))
    for r, line in enumerate(rows):
        for c, ch in enumerate(line):
            if ch in COLORS:
                draw.rectangle([c * PX, r * PX, c * PX + PX - 1, r * PX + PX - 1], fill=COLORS[ch])
    for p in d.get("platforms", []):
        draw.rectangle([p["col"] * PX, p["row"] * PX, (p["col"] + p["width"]) * PX - 1, p["row"] * PX + 1],
                       fill=(255, 255, 255))
    for e in d["entities"]:
        color = ENT.get(e["type"], (255, 0, 255))
        x, y = e["col"] * PX, e["row"] * PX
        draw.rectangle([x, y, x + PX - 1, y + PX - 1], fill=color, outline=(255, 0, 255))
    return img


def main():
    out = sys.argv[1]
    worlds = sys.argv[2:] or [str(i) for i in range(1, 9)]
    for world in worlds:
        imgs = []
        for level in range(1, 5):
            p = f"levels/world{world}/{world}-{level}.json"
            if os.path.exists(p):
                imgs.append(render(p))
        if not imgs:
            continue
        W = max(i.width for i in imgs)
        H = sum(i.height + 6 for i in imgs)
        sheet = Image.new("RGB", (W, H), (60, 60, 60))
        y = 0
        for i in imgs:
            sheet.paste(i, (0, y))
            y += i.height + 6
        sheet.save(os.path.join(out, f"world{world}.png"))


if __name__ == "__main__":
    main()
