"""Recorta el mastil de la bandera de meta y la bandera de las hojas de referencia.

Genera, con fondo transparente y una fila por WorldType (mismo orden que
src/world/WorldType.h: Overworld, Underwater, Underground, Castle):
  - assets/sprites/flagpole.png: 2 columnas de 16x16 (0 = bola del tope,
    1 = tramo de mastil).
  - assets/sprites/flag.png: 1 columna de 16x16 (la bandera).

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_flagpole.py
"""
from PIL import Image

TILE = 16

# Posicion (x, y) del tile en la hoja de referencia, por WorldType. Se
# ubicaron buscando el mismo patron de pixeles en cada bloque de paleta.
POLE_BALL = {  # Documentaciones/Tiles.png, "Palette 0 Tiles (Pipes and Scenery)"
    "Overworld": (136, 230),
    "Underground": (300, 230),
    "Castle": (136, 314),
    "Underwater": (300, 314),
}
POLE_BODY_DY = 17  # el tramo de mastil esta justo debajo (1px de separador)
FLAG = {  # Documentaciones/Objetos.png, una seccion "<Mundo> Objects" por tipo
    "Overworld": (92, 90),
    "Underground": (92, 198),
    "Castle": (92, 306),
    "Underwater": (92, 414),
}
WORLD_ORDER = ["Overworld", "Underwater", "Underground", "Castle"]

# Colores de fondo de la hoja (no son parte del sprite).
BACKGROUND = {(148, 148, 255), (108, 106, 255), (146, 144, 255)}


def cut(sheet, x, y):
    tile = sheet.crop((x, y, x + TILE, y + TILE)).convert("RGBA")
    pixels = tile.load()
    for j in range(TILE):
        for i in range(TILE):
            if pixels[i, j][:3] in BACKGROUND:
                pixels[i, j] = (0, 0, 0, 0)
    return tile


def main():
    tiles = Image.open("Documentaciones/Tiles.png").convert("RGB")
    objects = Image.open("Documentaciones/Objetos.png").convert("RGB")

    pole = Image.new("RGBA", (TILE * 2, TILE * len(WORLD_ORDER)), (0, 0, 0, 0))
    flag = Image.new("RGBA", (TILE, TILE * len(WORLD_ORDER)), (0, 0, 0, 0))
    for row, world in enumerate(WORLD_ORDER):
        bx, by = POLE_BALL[world]
        pole.paste(cut(tiles, bx, by), (0, row * TILE))
        pole.paste(cut(tiles, bx, by + POLE_BODY_DY), (TILE, row * TILE))
        fx, fy = FLAG[world]
        flag.paste(cut(objects, fx, fy), (0, row * TILE))

    pole.save("assets/sprites/flagpole.png")
    flag.save("assets/sprites/flag.png")


if __name__ == "__main__":
    main()
