"""Enredadera (la que sale de un ladrillo y se trepa).

  assets/sprites/vine.png   2 tiles de 16x16: la punta (con el brote) y un
                            tramo del tallo que se repite hacia abajo; de
                            "Overworld Objects" en Documentaciones/Objetos.png

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_vine.py
"""
from PIL import Image

# Esquina superior izquierda de cada tile en la hoja (punta, tallo).
TILES = [(130, 90), (148, 90)]
# Fondo de las cajas de la hoja (lila y su sombra) y el marco azul.
BACKGROUNDS = {(146, 144, 255), (108, 106, 255), (0, 41, 140)}


def main():
    sheet = Image.open("Documentaciones/Objetos.png").convert("RGB")
    src = sheet.load()
    out = Image.new("RGBA", (16 * len(TILES), 16), (0, 0, 0, 0))
    dst = out.load()
    for i, (x0, y0) in enumerate(TILES):
        for y in range(16):
            for x in range(16):
                c = src[x0 + x, y0 + y]
                if c not in BACKGROUNDS:
                    dst[i * 16 + x, y] = c + (255,)
    out.save("assets/sprites/vine.png")


if __name__ == "__main__":
    main()
