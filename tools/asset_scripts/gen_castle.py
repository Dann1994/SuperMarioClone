"""Recorta las piezas de castillo y de plataformas de las hojas de referencia.

Genera, con fondo transparente:
  - assets/sprites/lava.png: 16x32, dos frames de 16x16 en columna (0 =
    superficie con las ondas, 1 = relleno).
  - assets/sprites/bridge.png: 16x16, un tramo del puente de Bowser.
  - assets/sprites/lift.png: 8x8, un tramo de plataforma movil (se repite a
    lo ancho de la plataforma).
  - assets/sprites/axe.png: 48x16, el hacha del final del puente en 3 frames
    de 16x16 (el brillo de color que comparte con monedas y '?').

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_castle.py
"""
from PIL import Image

# (x0, y0, x1, y1) en cada hoja, ubicadas detectando las regiones que no son
# fondo de la hoja.
LAVA_SURFACE = (616, 728, 632, 744)  # Tiles.png, "Assembled Structures"
LAVA_FILL = (616, 744, 632, 760)
BRIDGE = (184, 600, 200, 616)        # Tiles.png, el puente de 13 tramos
LIFT = (32, 64, 40, 72)              # Objetos.png, "Overworld Objects"
# Tiles.png, "Palette 3 Tiles (Coins)", seccion Castle: fila de las hachas.
AXE_FRAMES = [(490, 129), (507, 129), (524, 129)]


def is_background(rgb):
    r, g, b = rgb
    if (r, g, b) == (0, 41, 140):  # fondo azul oscuro de la hoja
        return True
    return 135 <= r <= 155 and 135 <= g <= 155 and b >= 240  # celeste/lavanda


def cut(sheet, box):
    piece = sheet.crop(box)
    out = Image.new("RGBA", piece.size, (0, 0, 0, 0))
    src = piece.load()
    dst = out.load()
    for y in range(piece.height):
        for x in range(piece.width):
            if not is_background(src[x, y]):
                dst[x, y] = src[x, y] + (255,)
    return out


def main():
    tiles = Image.open("Documentaciones/Tiles.png").convert("RGB")
    objects = Image.open("Documentaciones/Objetos.png").convert("RGB")

    lava = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
    lava.paste(cut(tiles, LAVA_SURFACE), (0, 0))
    lava.paste(cut(tiles, LAVA_FILL), (0, 16))
    lava.save("assets/sprites/lava.png")
    cut(tiles, BRIDGE).save("assets/sprites/bridge.png")
    cut(objects, LIFT).save("assets/sprites/lift.png")

    axe = Image.new("RGBA", (16 * len(AXE_FRAMES), 16), (0, 0, 0, 0))
    for i, (x, y) in enumerate(AXE_FRAMES):
        axe.paste(cut(tiles, (x, y, x + 16, y + 16)), (16 * i, 0))
    axe.save("assets/sprites/axe.png")


if __name__ == "__main__":
    main()
