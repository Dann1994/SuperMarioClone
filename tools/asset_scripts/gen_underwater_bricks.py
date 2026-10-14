"""Rehace la fila de agua (WorldType::Underwater, fila 1) del ladrillo con la
paleta verde del original (los pedazos son sprites, con otra paleta: ver
gen_world_palettes.py).

La fila 1 de brick_block.png tenia los grises del
castillo; segun "Palette 1 Tiles" > "Underwater" de Documentaciones/Tiles.png
el ladrillo de agua es verde, con los mismos colores que el piso de agua
(terrain.png fila 1). Se parte de la fila de superficie (fila 0) cambiando
cada color por su par de agua. Se puede correr varias veces. Uso, parado en
la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_underwater_bricks.py
"""
from PIL import Image

# Superficie -> agua (las muestras de color de cada seccion de Tiles.png).
SWAPS = {
    (255, 206, 197): (189, 247, 173),   # claro
    (156, 74, 0): (16, 148, 0),         # medio
    (0, 0, 0): (0, 0, 0),               # negro
}
ROW = 1  # WorldType::Underwater


def rebuild(path):
    sheet = Image.open(path).convert("RGBA")
    h = sheet.height // 4
    src = sheet.crop((0, 0, sheet.width, h))
    px = src.load()
    for y in range(h):
        for x in range(sheet.width):
            r, g, b, a = px[x, y]
            if a and (r, g, b) in SWAPS:
                px[x, y] = SWAPS[(r, g, b)] + (a,)
    sheet.paste(src, (0, ROW * h))
    sheet.save(path)
    print("ok", path)


def main():
    rebuild("assets/sprites/brick_block.png")


if __name__ == "__main__":
    main()
