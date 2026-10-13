"""Cadena del puente de Bowser (MT_BOWSERBRIDGE_CHAIN del original).

La hoja de referencia no la trae, asi que se dibuja: eslabones en diagonal
(del puente, abajo a la izquierda, hacia el hacha, arriba a la derecha) con
los grises de la paleta del castillo y contorno negro.

  assets/sprites/bridge_chain.png   16x16

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_bridge_chain.py
"""
from PIL import Image

# K contorno, W claro, G gris medio, D gris oscuro, . transparente
PLAN = [
    "............KKK.",
    "...........KWWGK",
    "..........KWKKGK",
    "..........KGKKDK",
    ".........KKGDDK.",
    "........KWWKKK..",
    ".......KWKKGK...",
    ".......KGKKDK...",
    "......KKGDDK....",
    ".....KWWKKK.....",
    "....KWKKGK......",
    "....KGKKDK......",
    "...KKGDDK.......",
    "..KWWKKK........",
    ".KWKKGK.........",
    ".KGDDK..........",
]
COLORS = {
    "K": (0, 0, 0, 255),
    "W": (255, 255, 255, 255),
    "G": (173, 173, 173, 255),
    "D": (99, 99, 99, 255),
}


def main():
    out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    px = out.load()
    for y, line in enumerate(PLAN):
        for x, ch in enumerate(line):
            if ch in COLORS:
                px[x, y] = COLORS[ch]
    out.save("assets/sprites/bridge_chain.png")


if __name__ == "__main__":
    main()
