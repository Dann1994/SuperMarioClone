"""Trampolin y castillo grande.

  assets/sprites/spring.png              3 frames de 16x24 (estirado, a medias,
                                         comprimido), apoyados abajo; de
                                         Documentaciones/Objetos.png
  assets/sprites/scenery_castle_big.png  castillo alto de 9x11 tiles (el del
                                         final de algunos niveles), recortado de
                                         la version armada de "Assembled
                                         Structures" en Documentaciones/Tiles.png
                                         (la base es mas ancha que las 5
                                         columnas del objeto: 2 de cada lado)

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_spring_castle.py
"""
from PIL import Image

BACKGROUNDS = {(0, 41, 140), (146, 144, 255), (148, 148, 255)}


def main():
    sheet = Image.open("Documentaciones/Objetos.png").convert("RGB")
    src = sheet.load()
    springs = Image.new("RGBA", (16 * 3, 24), (0, 0, 0, 0))
    dst = springs.load()
    # (x de la hoja, y de arriba, alto)
    for frame, (x0, y0, h) in enumerate([(32, 75, 24), (50, 83, 16), (68, 91, 8)]):
        for y in range(h):
            for x in range(16):
                c = src[x0 + x, y0 + y]
                if c not in BACKGROUNDS:
                    dst[frame * 16 + x, 24 - h + y] = c + (255,)
    springs.save("assets/sprites/spring.png")

    tiles = Image.open("Documentaciones/Tiles.png").convert("RGB")
    src = tiles.load()
    big = Image.new("RGBA", (144, 176), (0, 0, 0, 0))
    dst = big.load()
    for y in range(176):
        for x in range(144):
            c = src[400 + x, 600 + y]
            # Fuera: cielo, marco y lineas de la grilla (todo azulado; el
            # castillo no tiene azul).
            if c[2] < 120 or c[0] > 200:  # ladrillo/negro o el rosado de los bordes
                dst[x, y] = c + (255,)
    big.save("assets/sprites/scenery_castle_big.png")


if __name__ == "__main__":
    main()
