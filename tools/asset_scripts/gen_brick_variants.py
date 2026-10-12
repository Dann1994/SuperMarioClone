"""Agrega a assets/sprites/brick_block.png la variante del ladrillo sin brillo.

El original tiene dos ladrillos: con brillo arriba (el de la fila de mas
arriba de un grupo, o una fila suelta tipo plataforma) y sin brillo (los que
tienen otro ladrillo encima). Segun "Palette 1 Tiles" de
Documentaciones/Tiles.png la unica diferencia es la primera fila de pixeles:
en el sin brillo es igual a la fila 8 (el tope de la segunda hilada).

Resultado: columna 0 = con brillo (la que ya estaba), columna 1 = sin brillo,
una fila por WorldType. Se puede correr varias veces (parte siempre de la
columna 0). Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_brick_variants.py
"""
from PIL import Image

PATH = "assets/sprites/brick_block.png"


def main():
    sheet = Image.open(PATH).convert("RGBA")
    rows = sheet.height // 16
    out = Image.new("RGBA", (32, sheet.height), (0, 0, 0, 0))
    shine = sheet.crop((0, 0, 16, sheet.height))
    out.paste(shine, (0, 0))
    plain = shine.copy()
    src = shine.load()
    dst = plain.load()
    for r in range(rows):
        for x in range(16):
            dst[x, r * 16] = src[x, r * 16 + 8]
    out.paste(plain, (16, 0))
    out.save(PATH)


if __name__ == "__main__":
    main()
