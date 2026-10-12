"""Soga y polea de las balanzas.

  assets/sprites/pulley.png   4 tiles de 16x16: soga horizontal, esquina
                              izquierda con la rueda, esquina derecha con la
                              rueda, soga vertical; de la polea armada de
                              "Assembled Structures" en Documentaciones/Tiles.png
                              (la esquina izquierda es la derecha espejada)

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_pulley.py
"""
from PIL import Image, ImageOps

# Esquina superior izquierda de la polea armada en la hoja.
ORIGIN = (560, 600)


def is_background(c):
    """Cielo (celeste lila, con variantes de compresion) o marco azul."""
    r, g, b = c
    return b > 200 and r < 170 and g < 170 or (r, g, b) == (0, 41, 140)


def tile(sheet, col, row):
    x0, y0 = ORIGIN[0] + col * 16, ORIGIN[1] + row * 16
    out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    src, dst = sheet.load(), out.load()
    for y in range(16):
        for x in range(16):
            c = src[x0 + x, y0 + y]
            if not is_background(c):
                dst[x, y] = c + (255,)
    return out


def main():
    sheet = Image.open("Documentaciones/Tiles.png").convert("RGB")
    horizontal = tile(sheet, 0, 0)
    corner_right = tile(sheet, 2, 0)
    vertical = tile(sheet, 2, 1)
    corner_left = ImageOps.mirror(corner_right)
    out = Image.new("RGBA", (64, 16), (0, 0, 0, 0))
    for i, t in enumerate([horizontal, corner_left, corner_right, vertical]):
        out.paste(t, (i * 16, 0))
    out.save("assets/sprites/pulley.png")


if __name__ == "__main__":
    main()
