"""Recorta la tuberia de Documentaciones/Tiles.png ("Palette 0 Tiles").

Genera assets/sprites/pipe.png: 64x64, una fila por WorldType (mismo orden
que src/world/WorldType.h: Overworld, Underwater, Underground, Castle) y dos
frames de 32x16 por fila (0 = tope con el borde, 1 = cuerpo). Cada frame son
dos tiles de 16x16 de la hoja (mitad izquierda + mitad derecha), que en la
hoja estan separados por una linea de 1px que aca se saltea.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_pipe.py
"""
from PIL import Image

TILE = 16
PITCH = 17  # tile + separador de 1px en la grilla de la hoja

# Esquina superior izquierda del tile izquierdo del TOPE de la tuberia en
# cada bloque de paleta; el cuerpo esta justo una fila abajo.
PIPE_TOP = {
    "Overworld": (119, 196),
    "Underground": (283, 196),
    "Castle": (119, 280),
    "Underwater": (283, 280),
}
WORLD_ORDER = ["Overworld", "Underwater", "Underground", "Castle"]


def is_background(rgb):
    r, g, b = rgb
    return 135 <= r <= 155 and 135 <= g <= 155 and b >= 240  # celeste/lavanda


def cut_tile(sheet, x, y):
    tile = sheet.crop((x, y, x + TILE, y + TILE)).convert("RGBA")
    pixels = tile.load()
    for j in range(TILE):
        for i in range(TILE):
            if is_background(pixels[i, j][:3]):
                pixels[i, j] = (0, 0, 0, 0)
    return tile


def main():
    sheet = Image.open("Documentaciones/Tiles.png").convert("RGB")
    out = Image.new("RGBA", (TILE * 4, TILE * len(WORLD_ORDER)), (0, 0, 0, 0))
    for row, world in enumerate(WORLD_ORDER):
        x, y = PIPE_TOP[world]
        for frame, ty in enumerate((y, y + PITCH)):  # 0 = tope, 1 = cuerpo
            out.paste(cut_tile(sheet, x, ty), (frame * 2 * TILE, row * TILE))
            out.paste(cut_tile(sheet, x + PITCH, ty), (frame * 2 * TILE + TILE, row * TILE))
    out.save("assets/sprites/pipe.png")


if __name__ == "__main__":
    main()
