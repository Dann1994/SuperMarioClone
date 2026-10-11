"""Recorta la escenografia de fondo de "Assembled Structures" (Documentaciones/Tiles.png).

Genera un PNG por pieza en assets/sprites/scenery_<nombre>.png, con fondo
transparente y paleta Overworld (la de los niveles de superficie, donde
aparecen): castillo chico, colinas, arbustos y nubes. Son decorados puros
(no colisionan), los ubica el campo "decorations" del JSON del nivel.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_scenery.py
"""
from PIL import Image

# (x0, y0, x1, y1) de cada pieza en Tiles.png, ubicadas detectando las
# regiones que no son fondo de la hoja.
PIECES = {
    "castle_small": (24, 696, 104, 776),
    "hill_big": (112, 728, 192, 776),
    "hill_small": (200, 744, 248, 776),
    "bush_big": (112, 704, 192, 720),
    "bush_small": (200, 704, 248, 720),
    "cloud_small": (344, 632, 392, 664),
    "cloud_big": (616, 608, 680, 640),
}


def is_background(rgb):
    r, g, b = rgb
    if (r, g, b) == (0, 41, 140):  # fondo azul oscuro de la hoja
        return True
    # Celeste/lavanda de fondo de cada pieza (con algo de ruido de escalado).
    return 135 <= r <= 155 and 135 <= g <= 155 and b >= 240


def snap(rgb):
    r, g, b = rgb
    if r >= 245 and g >= 245 and b >= 245:
        return (255, 255, 255)
    if r <= 12 and g <= 12 and b <= 12:
        return (0, 0, 0)
    return rgb


def main():
    sheet = Image.open("Documentaciones/Tiles.png").convert("RGB")
    for name, box in PIECES.items():
        piece = sheet.crop(box)
        out = Image.new("RGBA", piece.size, (0, 0, 0, 0))
        src = piece.load()
        dst = out.load()
        for y in range(piece.height):
            for x in range(piece.width):
                rgb = src[x, y]
                if not is_background(rgb):
                    dst[x, y] = snap(rgb) + (255,)
        out.save(f"assets/sprites/scenery_{name}.png")


if __name__ == "__main__":
    main()
