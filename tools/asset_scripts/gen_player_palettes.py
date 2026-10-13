"""Paletas de Mario y Luigi segun el tipo de mundo.

El original usa siempre la misma; aca, a pedido, el personaje combina con el
nivel. Los colores salen de los de la estrella de cada mundo (seccion "Star
Colors" de Documentaciones/MarioLuigi.png), que ya son los del escenario.
Genera assets/sprites/<personaje>_<tamano>_<mundo>.png recoloreando los
sprites normales (mario_small.png, luigi_fire.png, ...): subterraneo,
castillo y agua (la superficie usa la normal). Game::init carga las que
existan.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_player_palettes.py
"""
from PIL import Image

RED = (181, 49, 32)        # gorra/overol de Mario, camisa de Mario fuego
OLIVE = (107, 109, 0)      # pelo, camisa y zapatos de Mario
SKIN = (234, 158, 34)
GREEN = (56, 135, 0)       # Luigi
WHITE = (255, 254, 255)    # camisa de Luigi
FIRE_WHITE = (247, 216, 165)

TEAL = (0, 124, 141)       # 0x1C
DARK_TEAL = (0, 66, 74)    # 0x0C
LIGHT_TEAL = (181, 239, 239)  # 0x3C
BROWN = (153, 78, 0)       # 0x17
PALE_SKIN = (254, 204, 197)   # 0x36
DARK_GRAY = (102, 102, 102)   # 0x00
LIGHT_GRAY = (173, 173, 173)  # 0x10
CORAL = (228, 92, 160)        # 0x25, como los corales
DEEP_BLUE = (0, 64, 152)      # 0x02, el agua profunda
SEA_GREEN = (0, 168, 116)     # 0x1A/0x2A, las algas
LIGHT_BLUE = (164, 228, 252)  # 0x3C

# Grises con un toque de verde: Luigi en el castillo no queda igual a Mario.
LUIGI_DARK = (60, 96, 60)
LUIGI_LIGHT = (150, 186, 150)
SEA_DARK = (0, 110, 76)
# Luigi fuego en superficie (paleta pedida): gorra y overol verde lima;
# camisa, pelo y zapatos rojos. Mismo dibujo que Mario fuego.
FIRE_LIME = (181, 222, 49)
FIRE_RED = (247, 57, 16)
LIGHT_GREEN = (150, 230, 120)
SALMON = (255, 120, 90)

# Por mundo y personaje: el mapeo de cada tamano ("small"/"big"/"fire").
# Grande y fuego siempre con colores distintos entre si (antes el blanco
# del grande y el crema del fuego terminaban en el mismo color).
PALETTES = {
    "underground": {
        "mario": dict.fromkeys(("small", "big", "fire"),
                               {RED: TEAL, OLIVE: BROWN, SKIN: PALE_SKIN, FIRE_WHITE: LIGHT_TEAL}),
        "luigi": {
            "small": {GREEN: DARK_TEAL, WHITE: LIGHT_TEAL, SKIN: PALE_SKIN},
            "big": {GREEN: DARK_TEAL, WHITE: LIGHT_TEAL, SKIN: PALE_SKIN},
            "fire": {FIRE_LIME: LIGHT_GREEN, FIRE_RED: DARK_TEAL, SKIN: PALE_SKIN},
        },
    },
    # Castillo: grises (como los bloques) sobre el fondo negro; Luigi con
    # grises verdosos.
    "castle": {
        "mario": dict.fromkeys(("small", "big", "fire"),
                               {RED: LIGHT_GRAY, OLIVE: DARK_GRAY, SKIN: PALE_SKIN, FIRE_WHITE: WHITE}),
        "luigi": {
            "small": {GREEN: LUIGI_DARK, WHITE: LIGHT_GRAY, SKIN: PALE_SKIN},
            "big": {GREEN: LUIGI_DARK, WHITE: LIGHT_GRAY, SKIN: PALE_SKIN},
            "fire": {FIRE_LIME: LUIGI_LIGHT, FIRE_RED: WHITE, SKIN: PALE_SKIN},
        },
    },
    # Agua: coral y azul profundo (Mario), verde agua y celeste (Luigi).
    "underwater": {
        "mario": dict.fromkeys(("small", "big", "fire"),
                               {RED: CORAL, OLIVE: DEEP_BLUE, SKIN: PALE_SKIN, FIRE_WHITE: LIGHT_BLUE}),
        "luigi": {
            "small": {GREEN: SEA_GREEN, WHITE: LIGHT_BLUE, SKIN: PALE_SKIN},
            "big": {GREEN: SEA_GREEN, WHITE: LIGHT_BLUE, SKIN: PALE_SKIN},
            "fire": {FIRE_LIME: SEA_GREEN, FIRE_RED: SALMON, SKIN: PALE_SKIN},
        },
    },
}


def build_luigi_fire():
    """Luigi fuego (superficie) con la paleta pedida: el dibujo de Mario
    fuego con el crema en verde lima y el rojo en el rojo de esa paleta."""
    image = Image.open("assets/sprites/mario_fire.png").convert("RGBA")
    pixels = image.load()
    swap = {FIRE_WHITE: FIRE_LIME, RED: FIRE_RED}
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = pixels[x, y]
            if a and (r, g, b) in swap:
                pixels[x, y] = swap[(r, g, b)] + (a,)
    image.save("assets/sprites/luigi_fire.png")


def main():
    build_luigi_fire()
    for world, characters in PALETTES.items():
        for character, sizes in characters.items():
            for size, mapping in sizes.items():
                image = Image.open(f"assets/sprites/{character}_{size}.png").convert("RGBA")
                pixels = image.load()
                for y in range(image.height):
                    for x in range(image.width):
                        r, g, b, a = pixels[x, y]
                        if a and (r, g, b) in mapping:
                            pixels[x, y] = mapping[(r, g, b)] + (a,)
                image.save(f"assets/sprites/{character}_{size}_{world}.png")


if __name__ == "__main__":
    main()
