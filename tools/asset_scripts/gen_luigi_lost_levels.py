"""Hojas de LUIGI DE LOST LEVELS (CONTENIDO EXTRA): las de Luigi con la paleta
de Documentaciones/Luigi_moderno.png (gorra y remera verdes, mameluco
oliva; con fuego, mameluco celeste). Solo cambia los colores de
luigi_small/big/fire.png -> luigi_ll_small/big/fire.png.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_luigi_lost_levels.py
"""
from PIL import Image

S = "assets/sprites/"
NORMAL = {
    (56, 135, 0): (107, 109, 0),      # verde -> oliva
    (255, 254, 255): (56, 135, 0),    # blanco -> verde
}
FIRE = {
    (247, 57, 16): (56, 135, 0),      # rojo -> verde
    (181, 222, 49): (192, 223, 255),  # verde claro -> celeste
}


def recolor(src, dst, table):
    img = Image.open(S + src).convert("RGBA")
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if a and (r, g, b) in table:
                px[x, y] = table[(r, g, b)] + (a,)
    img.save(S + dst)


def main():
    recolor("luigi_small.png", "luigi_ll_small.png", NORMAL)
    recolor("luigi_big.png", "luigi_ll_big.png", NORMAL)
    recolor("luigi_fire.png", "luigi_ll_fire.png", FIRE)
    print("ok")


if __name__ == "__main__":
    main()
