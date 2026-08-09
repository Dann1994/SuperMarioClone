from PIL import Image
from collections import Counter

# Paletas por entorno, en terminos de SLOT (no de RGB de un personaje
# puntual): cada sprite base tiene 3 colores no transparentes; el mas
# frecuente es el slot "olive" (overol/ropa grande), el 2do el slot "red"
# (gorra/remera), el 3ro el slot "skin" (piel). Se determino este orden
# comparando mario_small.png/luigi_small.png contra las hojas de referencia.
# Valores derivados de los swatches "Player Color" de la seccion Star Colors.
ENV_PALETTES = {
    "overworld": [
        {"red": (12, 147, 0), "olive": (234, 158, 34), "skin": (255, 254, 255)},
        {"red": (181, 49, 32), "olive": (234, 158, 34), "skin": (255, 254, 255)},
        {"red": (0, 0, 0), "olive": (153, 78, 0), "skin": (254, 204, 197)},
    ],
    "castle": [
        {"red": (153, 78, 0), "olive": (0, 0, 0), "skin": (0, 124, 141)},
        {"red": (234, 158, 34), "olive": (0, 0, 0), "skin": (181, 49, 32)},
        {"red": (173, 173, 173), "olive": (0, 0, 0), "skin": (102, 102, 102)},
    ],
    "underground": [
        {"red": (0, 0, 0), "olive": (153, 78, 0), "skin": (0, 124, 141)},
        {"red": (0, 0, 0), "olive": (234, 158, 34), "skin": (181, 49, 32)},
        {"red": (0, 0, 0), "olive": (0, 124, 141), "skin": (0, 64, 77)},
    ],
    "underwater": [
        {"red": (173, 173, 173), "olive": (234, 158, 34), "skin": (255, 254, 255)},
        {"red": (181, 49, 32), "olive": (234, 158, 34), "skin": (255, 254, 255)},
        {"red": (0, 0, 0), "olive": (173, 173, 173), "skin": (255, 254, 255)},
    ],
}


def detect_slots(im):
    """Devuelve {slot_name: rgb} para los 3 colores no transparentes mas
    frecuentes del sprite, ordenados olive(mas frecuente) > red > skin."""
    counts = Counter()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            if a == 0:
                continue
            counts[(r, g, b)] += 1
    top3 = [c for c, _ in counts.most_common(3)]
    while len(top3) < 3:
        top3.append(None)
    return {"olive": top3[0], "red": top3[1], "skin": top3[2]}


def recolor(im, mapping):
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            if a == 0:
                continue
            newc = mapping.get((r, g, b), (r, g, b))
            out.putpixel((x, y), (newc[0], newc[1], newc[2], 255))
    return out


def build_star_sheet(base_path, out_path, env):
    base = Image.open(base_path).convert("RGBA")
    slots = detect_slots(base)
    w, h = base.size
    swatches = ENV_PALETTES[env]
    sheet = Image.new("RGBA", (w, h * 3), (0, 0, 0, 0))
    for i, swatch in enumerate(swatches):
        mapping = {slots[k]: v for k, v in swatch.items() if slots[k] is not None}
        sheet.paste(recolor(base, mapping), (0, i * h))
    sheet.save(out_path)
    print("saved", out_path, sheet.size, "slots=", slots)


# Orden de bloques que espera Mario.cpp: WorldType::Overworld/Underwater/
# Underground/Castle, 3 swatches cada uno (12 bloques en total).
COMBINED_ENV_ORDER = ["overworld", "underwater", "underground", "castle"]


def build_combined_star_sheet(base_path, out_path, envs=COMBINED_ENV_ORDER):
    base = Image.open(base_path).convert("RGBA")
    slots = detect_slots(base)
    w, h = base.size
    block_count = len(envs) * 3
    sheet = Image.new("RGBA", (w, h * block_count), (0, 0, 0, 0))
    block = 0
    for env in envs:
        for swatch in ENV_PALETTES[env]:
            mapping = {slots[k]: v for k, v in swatch.items() if slots[k] is not None}
            sheet.paste(recolor(base, mapping), (0, block * h))
            block += 1
    sheet.save(out_path)
    print("saved", out_path, sheet.size, "slots=", slots)


if __name__ == "__main__":
    import sys
    if len(sys.argv) == 3:
        base_path, out_path = sys.argv[1], sys.argv[2]
        build_combined_star_sheet(base_path, out_path)
    else:
        base_path, out_path, env = sys.argv[1], sys.argv[2], sys.argv[3]
        build_star_sheet(base_path, out_path, env)
