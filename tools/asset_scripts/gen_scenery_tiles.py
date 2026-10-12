"""Recorta los tiles de escenografia solida de Documentaciones/Tiles.png.

Genera assets/sprites/scenery_tiles.png: una fila de 16x16 por WorldType
(Overworld, Underwater, Underground, Castle, mismo orden que WorldType.h) y
una columna por pieza (ver Tilemap::renderScenery):

  0-2  copa de arbol (izq, medio, der)     3  tronco
  4-6  hongo (izq, medio, der)             7  tope del tallo   8  tallo
  9    baranda del puente de soga         10  tablon del puente
  11   canon (boca)  12 canon (calavera)  13  canon (base)
  14   coral (tope) 15 coral (cuerpo)
  16   agua (olas)  17 agua (cuerpo)      18  superficie bajo el agua
  19   bloque de nube (el piso de las areas de monedas en el cielo,
       de "Palette 2")

Solo el canon cambia de paleta (sale de "Palette 1"); el resto es igual
en las cuatro filas.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_scenery_tiles.py
"""
from PIL import Image

T = 16

# Esquina superior izquierda de cada pieza en "Assembled Structures".
SHARED = [
    (256, 696), (272, 696), (320, 696), (288, 728),   # arbol
    (344, 728), (360, 728), (376, 728), (360, 744), (360, 760),  # hongo
    (40, 656), (40, 672),                             # puente de soga
    None, None, None,                                 # canon (por paleta)
    (312, 640), (312, 656),                           # coral
    (616, 648), (616, 664), (616, 688),               # agua
    (366, 16),                                        # bloque de nube
]

# Canon en "Palette 1": columna 7, filas 0-2 (paso de 17 px), con el
# origen de cada seccion por WorldType.
PALETTE1_ORIGIN = {
    0: (0, 16),       # Overworld
    1: (147, 100),    # Underwater
    2: (147, 16),     # Underground
    3: (0, 100),      # Castle
}


def is_background(rgb):
    r, g, b = rgb
    if (r, g, b) in ((0, 41, 140), (148, 148, 255)):
        return True
    return 135 <= r <= 155 and 135 <= g <= 155 and b >= 240


def copy_tile(sheet, out, sx, sy, dx, dy):
    src = sheet.load()
    dst = out.load()
    for y in range(T):
        for x in range(T):
            rgb = src[sx + x, sy + y]
            if not is_background(rgb):
                dst[dx + x, dy + y] = rgb + (255,)


def main():
    sheet = Image.open("Documentaciones/Tiles.png").convert("RGB")
    out = Image.new("RGBA", (T * len(SHARED), T * 4), (0, 0, 0, 0))
    for row in range(4):
        for col, pos in enumerate(SHARED):
            if pos is None:
                ox, oy = PALETTE1_ORIGIN[row]
                pos = (ox + 7 * 17, oy + (col - 11) * 17)
            copy_tile(sheet, out, pos[0], pos[1], col * T, row * T)
    out.save("assets/sprites/scenery_tiles.png")


if __name__ == "__main__":
    main()
