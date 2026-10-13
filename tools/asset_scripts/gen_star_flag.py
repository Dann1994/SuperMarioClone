"""Bandera con estrella que sube en el castillo al terminar el nivel.

  assets/sprites/star_flag.png   16x16 (mastil con la bandera), de
                                 Documentaciones/Objetos.png

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_star_flag.py
"""
from PIL import Image

ORIGIN = (110, 90)
BACKGROUNDS = {(146, 144, 255), (108, 106, 255), (0, 41, 140)}


def main():
    sheet = Image.open("Documentaciones/Objetos.png").convert("RGB")
    src = sheet.load()
    out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    dst = out.load()
    for y in range(16):
        for x in range(16):
            c = src[ORIGIN[0] + x, ORIGIN[1] + y]
            if c not in BACKGROUNDS:
                dst[x, y] = c + (255,)
    out.save("assets/sprites/star_flag.png")


if __name__ == "__main__":
    main()
