"""Corrige las filas por tipo de mundo (0 superficie, 1 agua, 2 subterraneo,
3 castillo) de los sprites que las tenian mal o no las tenian, segun las
paletas del original (GroundPaletteData, WaterPaletteData, ... leidas de la
ROM; ver la seccion de paletas de Documentaciones/Objetos.png y
Enemigos.png):

- Paleta de sprites 1 (la "verde"): superficie 1A 30 27, agua 10 30 27,
  subterraneo y castillo 1C 36 17. La usan el hongo 1UP, la enredadera y el
  Cheep Cheep gris (cheep_cheep_gray.png, sale del rojo cambiando de paleta).
- El hongo rojo usa la paleta 2, igual en todos los mundos: la fila de
  castillo tenia la verde.
- El martillo (paleta 3) tenia cambiadas las filas de agua y castillo.
- Los pedazos de ladrillo (paleta 3): en el agua son negro y gris (la fila
  de agua de brick_debris.png queda como estaba antes de
  gen_underwater_bricks.py, que solo debe tocar el ladrillo).

Se puede correr varias veces. Uso, parado en la raiz del proyecto (necesita
Pillow):
  python tools/asset_scripts/gen_world_palettes.py
"""
from PIL import Image

DIR = "assets/sprites/"

GREEN = (12, 147, 0)
WHITE = (255, 254, 255)
WHITE2 = (255, 255, 255)
ORANGE = (230, 156, 33)
RED = (181, 49, 33)
RED2 = (181, 49, 32)
GRAY = (173, 173, 173)
TEAL = (0, 123, 140)
SKIN = (255, 206, 197)
BROWN = (156, 74, 0)
DARK_GRAY = (99, 99, 99)
BLACK = (0, 0, 0)

# Paleta de sprites 1 por fila: (color 1, color 2, color 3).
SP1 = [
    (GREEN, WHITE, ORANGE),   # superficie
    (GRAY, WHITE, ORANGE),    # agua
    (TEAL, SKIN, BROWN),      # subterraneo
    (TEAL, SKIN, BROWN),      # castillo
]


def recolor(img, swaps):
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if a and (r, g, b) in swaps:
                px[x, y] = swaps[(r, g, b)] + (a,)
    return out


def four_rows_sp1(base, slots):
    """base: imagen de una fila; slots: sus colores para los 3 lugares de la
    paleta (cada lugar puede tener varios RGB casi iguales)."""
    h = base.height
    out = Image.new("RGBA", (base.width, h * 4), (0, 0, 0, 0))
    for row, colors in enumerate(SP1):
        swaps = {}
        for slot, rgbs in enumerate(slots):
            for rgb in rgbs:
                swaps[rgb] = colors[slot]
        out.paste(recolor(base, swaps), (0, row * h))
    return out


def snap(img, palette):
    """Lleva cada color al mas cercano de la paleta (limpia ruido)."""
    out = img.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if not a:
                continue
            best = min(palette, key=lambda c: (c[0] - r) ** 2 + (c[1] - g) ** 2 + (c[2] - b) ** 2)
            px[x, y] = best + (a,)
    return out


def main():
    green_slots = ([GREEN], [WHITE, WHITE2], [ORANGE])

    # 1UP: parte de la fila de superficie (verde).
    oneup = Image.open(DIR + "oneup_mushroom.png").convert("RGBA")
    four_rows_sp1(oneup.crop((0, 0, oneup.width, oneup.height // 4)), green_slots).save(DIR + "oneup_mushroom.png")

    # Enredadera: era de una sola fila (verde).
    vine = Image.open(DIR + "vine.png").convert("RGBA")
    four_rows_sp1(vine.crop((0, 0, vine.width, 16)), green_slots).save(DIR + "vine.png")

    # Cheep Cheep gris: el rojo (paleta 2: rojo, blanco, naranja) con la paleta 1.
    cheep = Image.open(DIR + "cheep_cheep.png").convert("RGBA")
    four_rows_sp1(cheep.crop((0, 0, cheep.width, 16)), ([RED, RED2], [WHITE, WHITE2], [ORANGE])).save(
        DIR + "cheep_cheep_gray.png")

    # Hongo rojo: la fila de castillo igual a la de superficie.
    mush = Image.open(DIR + "mushroom.png").convert("RGBA")
    h = mush.height // 4
    mush.paste(mush.crop((0, 0, mush.width, h)), (0, 3 * h))
    mush.save(DIR + "mushroom.png")

    # Martillo: agua (negro, blanco, gris) y castillo (gris oscuro, blanco, gris).
    hammer = Image.open(DIR + "hammer.png").convert("RGBA")
    h = hammer.height // 4
    water = hammer.crop((0, 3 * h, hammer.width, 4 * h))
    castle = hammer.crop((0, h, hammer.width, 2 * h))
    if (0, 0, 0) in {c[1][:3] for c in water.getcolors(1 << 16) if c[1][3]}:
        hammer.paste(snap(water, [BLACK, WHITE2, GRAY]), (0, h))
        hammer.paste(snap(castle, [DARK_GRAY, WHITE2, GRAY]), (0, 3 * h))
    hammer.save(DIR + "hammer.png")

    # Pedazos de ladrillo: agua negro y gris.
    debris = Image.open(DIR + "brick_debris.png").convert("RGBA")
    h = debris.height // 4
    top = debris.crop((0, 0, debris.width, h))
    debris.paste(recolor(top, {BROWN: GRAY}), (0, h))
    debris.save(DIR + "brick_debris.png")
    print("ok")


if __name__ == "__main__":
    main()
