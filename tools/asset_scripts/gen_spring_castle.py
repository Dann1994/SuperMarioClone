"""Trampolin y castillo grande.

  assets/sprites/spring.png              3 frames de 16x24 (estirado, a medias,
                                         comprimido), apoyados abajo; de
                                         Documentaciones/Objetos.png
  assets/sprites/scenery_castle_big.png  castillo de 5x11 tiles (el del final
                                         de algunos niveles), armado con las
                                         piezas del castillo chico siguiendo el
                                         plano del original (CastleObject)

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

    small = Image.open("assets/sprites/scenery_castle_small.png").convert("RGBA")

    def tile(col, row):
        return small.crop((col * 16, row * 16, col * 16 + 16, row * 16 + 16))

    pieces = {'N': tile(1, 2), 'x': tile(0, 3), 'D': tile(2, 3), 'E': tile(2, 4)}
    big = Image.new("RGBA", (80, 176), (0, 0, 0, 0))
    big.paste(small, (0, 0))
    plan = ["NNNNN", "xDxDx", "xExEx", "xxxxx", "DxDxD", "ExExE"]
    for r, line in enumerate(plan):
        for c, ch in enumerate(line):
            big.paste(pieces[ch], (c * 16, (5 + r) * 16))
    big.save("assets/sprites/scenery_castle_big.png")


if __name__ == "__main__":
    main()
