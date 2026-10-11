"""Recorta los bloques solidos de Documentaciones/Tiles.png ("Palette 1 Tiles").

Genera assets/sprites/terrain.png: 32x64, una fila por WorldType (mismo orden
que src/world/WorldType.h: Overworld, Underwater, Underground, Castle) y dos
frames de 16x16 por fila:
  0 = piso (tile '#'): el bloque de tierra agrietada del suelo, salvo en
      Castle, donde son los bloques cuadrados de piso de castillo (con las
      juntas transparentes, se ve el fondo negro entre bloque y bloque)
  1 = bloque duro (el biselado de las escaleras y la base de la bandera, 'S')

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_terrain.py
"""
from PIL import Image

TILE = 16
PITCH = 17  # tile + separador de 1px en la grilla de la hoja

# Esquina superior izquierda del tile de piso en cada bloque de paleta (el
# bloque duro esta justo una fila abajo). Ubicadas detectando las lineas
# separadoras de la grilla.
GROUND = {
    "Overworld": (0, 16),
    "Underground": (147, 16),
    "Castle": (0, 100),
    "Underwater": (147, 100),
}
WORLD_ORDER = ["Overworld", "Underwater", "Underground", "Castle"]
# Piso propio de castillo: los bloques cuadrados con borde (primera pieza de
# la cuarta fila del bloque Castle).
CASTLE_FLOOR = (0, 151)


def is_background(rgba):
    r, g, b = rgba[:3]
    return 135 <= r <= 155 and 135 <= g <= 155 and b >= 240  # celeste/lavanda de la hoja


def cut(sheet, x, y):
    tile = sheet.crop((x, y, x + TILE, y + TILE))
    pixels = tile.load()
    for j in range(TILE):
        for i in range(TILE):
            if is_background(pixels[i, j]):
                pixels[i, j] = (0, 0, 0, 0)
    return tile


def main():
    sheet = Image.open("Documentaciones/Tiles.png").convert("RGBA")
    out = Image.new("RGBA", (TILE * 2, TILE * len(WORLD_ORDER)), (0, 0, 0, 0))
    for row, world in enumerate(WORLD_ORDER):
        x, y = GROUND[world]
        floor = CASTLE_FLOOR if world == "Castle" else (x, y)
        hard = (x, y + PITCH)
        for frame, (tx, ty) in enumerate((floor, hard)):
            out.paste(cut(sheet, tx, ty), (frame * TILE, row * TILE))
    out.save("assets/sprites/terrain.png")


if __name__ == "__main__":
    main()
