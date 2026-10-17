"""Iconos de 16x16 para los avisos de desbloqueo (CONTENIDO EXTRA) que no
tienen un sprite propio en el juego: un reloj (TIEMPO INFINITO) y una nota
musical blanca (ROCOLA), que se vea sobre el circulo verde del aviso. Dibujados a mano, con la paleta del NES.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_extra_icons.py
"""
from PIL import Image

T = (0, 0, 0, 0)
K = (0, 0, 0, 255)          # contorno
W = (252, 252, 252, 255)    # blanco
Y = (252, 188, 60, 255)     # dorado (borde del reloj)
R = (228, 0, 88, 255)       # aguja roja

CLOCK = [
    "................",
    "......KKKK......",
    "....KKYYYYKK....",
    "...KYYWWWWYYK...",
    "..KYWWWKWWWWYK..",
    "..KYWWWKWWWWYK..",
    ".KYWWWWKWWWWWYK.",
    ".KYWWWWKWWWWWYK.",
    ".KYWWWWKRRRWWYK.",
    ".KYWWWWWWWWWWYK.",
    "..KYWWWWWWWWYK..",
    "..KYWWWWWWWWYK..",
    "...KYYWWWWYYK...",
    "....KKYYYYKK....",
    "......KKKK......",
    "................",
]

NOTE = [
    "................",
    "......WWWWWWWW..",
    "......WWWWWWWW..",
    "......WW....WW..",
    "......WW....WW..",
    "......WW....WW..",
    "......WW....WW..",
    "......WW....WW..",
    "......WW....WW..",
    "......WW....WW..",
    "..WWWWWW..WWWW..",
    ".WWWWWWW.WWWWW..",
    ".WWWWWWW.WWWWW..",
    "..WWWWW..WWWW...",
    "................",
    "................",
]

COLORS = {".": T, "K": K, "W": W, "Y": Y, "R": R}


def draw(rows, path):
    img = Image.new("RGBA", (16, 16), T)
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            img.putpixel((x, y), COLORS[c])
    img.save(path)


if __name__ == "__main__":
    draw(CLOCK, "assets/sprites/icon_clock.png")
    draw(NOTE, "assets/sprites/icon_note.png")
    print("ok")
