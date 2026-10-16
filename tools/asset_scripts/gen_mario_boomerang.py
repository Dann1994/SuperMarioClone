"""Arma el traje de Boomerang Bro: los mismos cuadros que el de Hammer Bro
(gen_mario_hammer.py) con la paleta azul de Documentaciones/Mario-Hammer-Bro.png
(lo negro pasa a azul), y lo blanco en rojizo muy claro para Mario y verde
muy claro para Luigi, como el de Hammer Bro.

Tambien: las hojas de la estrella (las mismas 12 paletas: lo claro toma el
color de la ropa, lo azul el del pelo/camisa y la piel el de la piel), el
bumeran (4 cuadros de giro, centrados en 16x16) y la flor bumeran.

Salida: assets/sprites/mario_boomerang.png, luigi_boomerang.png,
*_boomerang_star.png, boomerang.png y boomerang_flower.png.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_mario_boomerang.py
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_mario_hammer as hammer  # noqa: E402

SOURCE = "Documentaciones/Mario-Hammer-Bro.png"
BLUE = (32, 56, 236)
# Bumeranes: fila de 10 px, cuatro cuadros de 10x10 (x, y de la esquina).
BOOMERANG_CELLS = [(10, 18), (21, 18), (32, 18), (43, 18)]
BOOMERANG_SIZE = 10
# La flor bumeran (cuadro de 20x20 al lado de la flor martillo).
FLOWER_BOX = (112, 8, 132, 28)


def to_blue(sheet):
    """Lo negro (contorno, rayas, overol) en el azul de la hoja."""
    out = sheet.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if a and r < 40 and g < 40 and b < 40:
                px[x, y] = BLUE + (a,)
    return out


def star_sheet(sheet, suit_color):
    roles = {"suit": suit_color, "skin": hammer.SKIN, "blue": BLUE}

    def role_of(rgb):
        return min(roles, key=lambda r: sum((a - b) ** 2 for a, b in zip(rgb, roles[r])))

    out = Image.new("RGBA", (sheet.width, sheet.height * len(hammer.STAR_PALETTES)), (0, 0, 0, 0))
    src = sheet.load()
    for block, (clothes, skin, hair) in enumerate(hammer.STAR_PALETTES):
        light = hair if clothes == (0, 0, 0) else clothes
        dark = (0, 0, 0) if clothes == (0, 0, 0) else hair
        colors = {"suit": light, "skin": skin, "blue": dark}
        for y in range(sheet.height):
            for x in range(sheet.width):
                r, g, b, a = src[x, y]
                if a:
                    out.putpixel((x, block * sheet.height + y), colors[role_of((r, g, b))] + (a,))
    return out


def main():
    frames = to_blue(hammer.build_frames())
    mario = hammer.recolor(frames, hammer.MARIO_PINK)
    luigi = hammer.recolor(frames, hammer.LUIGI_GREEN)
    mario.save("assets/sprites/mario_boomerang.png")
    luigi.save("assets/sprites/luigi_boomerang.png")
    star_sheet(mario, hammer.MARIO_PINK).save("assets/sprites/mario_boomerang_star.png")
    star_sheet(luigi, hammer.LUIGI_GREEN).save("assets/sprites/luigi_boomerang_star.png")

    sheet = Image.open(SOURCE).convert("RGBA")
    boomerang = Image.new("RGBA", (16 * len(BOOMERANG_CELLS), 16), (0, 0, 0, 0))
    pad = (16 - BOOMERANG_SIZE) // 2
    for i, (x, y) in enumerate(BOOMERANG_CELLS):
        cell = hammer.clean(sheet.crop((x, y, x + BOOMERANG_SIZE, y + BOOMERANG_SIZE)))
        boomerang.paste(cell, (i * 16 + pad, pad))
    boomerang.save("assets/sprites/boomerang.png")

    flower = hammer.clean(sheet.crop(FLOWER_BOX))
    flower = flower.crop(flower.getbbox())
    canvas = Image.new("RGBA", (20, 20), (0, 0, 0, 0))
    canvas.paste(flower, ((20 - flower.width) // 2, 20 - flower.height))
    canvas.save("assets/sprites/boomerang_flower.png")
    print("ok", mario.size, boomerang.size, flower.size)


if __name__ == "__main__":
    main()
