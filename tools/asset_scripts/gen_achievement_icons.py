"""Iconos de los LOGROS (16x16 cada uno, en una fila: assets/sprites/
achievements.png, y achievements_locked.png en blanco y negro para los que
todavia no se cumplieron). El orden es el de kAchievements en GameMenus.cpp: cada
logro usa el icono de su posicion.

Se arman con los sprites del juego (Mario, Luigi, Goomba, monedas, hongos,
flores, el martillo, el Hammer Bro...) y algunos dibujados aca (trofeo,
escudo, cano, insignias "+", "?" y la cruz roja de "sin...").

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_achievement_icons.py
"""
from PIL import Image

S = "assets/sprites/"
OUT = S + "achievements.png"
T = (0, 0, 0, 0)
K = (0, 0, 0, 255)
W = (252, 252, 252, 255)
GOLD = (252, 188, 60, 255)
GOLD_D = (200, 120, 16, 255)
SILVER = (188, 188, 188, 255)
SILVER_D = (116, 116, 116, 255)
RED = (228, 40, 40, 255)
GREEN = (0, 168, 0, 255)
GREEN_L = (128, 208, 16, 255)
BLUE = (60, 120, 252, 255)
BLUE_D = (0, 64, 168, 255)


def frame(path, col, row, w=16, h=16):
    img = Image.open(S + path).convert("RGBA")
    return img.crop((col * w, row * h, col * w + w, row * h + h))


def canvas():
    return Image.new("RGBA", (16, 16), T)


def paste(dst, src, x=0, y=0):
    dst.alpha_composite(src, (x, y))
    return dst


def from_rows(rows, colors):
    img = canvas()
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c != ".":
                img.putpixel((x, y), colors[c])
    return img


def red_x(img):
    # Cruz roja con contorno (los logros de "sin...").
    for i in range(1, 15):
        for dx, dy, col in ((0, 0, RED), (1, 0, RED)):
            img.putpixel((i + dx if i + dx < 16 else 15, i), col)
            img.putpixel((15 - i - dx if 15 - i - dx >= 0 else 0, i), col)
    return img


def badge(img, symbol):
    # Insignia 7x7 abajo a la derecha: "+" (PARTIDA +) o "?".
    rows = {
        "+": [".KKKKK.", "KKKWKKK", "KKKWKKK", "KWWWWWK", "KKKWKKK", "KKKWKKK", ".KKKKK."],
        "?": [".KKKKK.", "KKWWWKK", "KKKKWKK", "KKKWWKK", "KKKWKKK", "KKKKKKK", ".KKWKK."],
    }
    for y, row in enumerate(rows[symbol]):
        for x, c in enumerate(row):
            if c == "K":
                img.putpixel((9 + x, 9 + y), (24, 24, 24, 255))
            elif c == "W":
                img.putpixel((9 + x, 9 + y), W)
    return img


TROPHY = [
    "................",
    "..GGGGGGGGGGGG..",
    ".GDGGGGGGGGGGDG.",
    ".GDGGGWGGGGGGDG.",
    "..GGGGWGGGGGGG..",
    "...GGGGGGGGGG...",
    "....GGGGGGGG....",
    ".....GGGGGG.....",
    "......GGGG......",
    ".......GG.......",
    ".......GG.......",
    "......DGGD......",
    ".....GGGGGG.....",
    "....DDDDDDDD....",
    "....GGGGGGGG....",
    "................",
]

SHIELD = [
    "................",
    "..KKKKKKKKKKKK..",
    "..KWWWWWBBBBBK..",
    "..KWWWWWBBBBBK..",
    "..KWWWWWBBBBBK..",
    "..KWWWWWBBBBBK..",
    "..KBBBBBWWWWWK..",
    "..KBBBBBWWWWWK..",
    "...KBBBBWWWWK...",
    "...KBBBBWWWWK...",
    "....KBBBWWWK....",
    ".....KBBWWK.....",
    "......KBWK......",
    ".......KK.......",
    "................",
    "................",
]

PIPE = [
    ".KKKKKKKKKKKKKK.",
    ".KLLGGGGGGGGGGK.",
    ".KLLGGGGGGGGGGK.",
    ".KKKKKKKKKKKKKK.",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KLLGGGGGGGGK..",
    "..KKKKKKKKKKKK..",
    "................",
    "................",
    "................",
]

# BANDERA ARRIBA: mastil, bola, bandera en lo mas alto y flecha hacia arriba.
FLAG_TOP = [
    "..........KKK...",
    ".........KGLGK..",
    ".........KGGGK..",
    ".........WKKK...",
    ".......KKWKLK...",
    ".....KKWWWKLK...",
    "...KKWWGGWKLK...",
    ".....KKWWWKLK...",
    ".......KKWKLK...",
    "...K......KLK...",
    "..KYK.....KLK...",
    ".KYYYK....KLK...",
    "KYYYYYK...KLK...",
    "KKKYKKK...KLK...",
    "..KYK.....KLK...",
    "..KKK.....KKK...",
]

# Flecha hacia abajo (blanca con contorno), arriba del cano de ATAJO.
ARROW = [
    "......KKKK......",
    "......KWWK......",
    "......KWWK......",
    "...KKKKWWKKKK...",
    "....KWWWWWWK....",
    ".....KWWWWK.....",
    "......KWWK......",
] + ["." * 16] * 9

LIGHTNING = [
    "........KKKK....",
    ".......KYYK.....",
    "......KYYK......",
    ".....KYYK.......",
    "....KYYYKKKK....",
    "....KYYYYYYK....",
    "....KKKKYYK.....",
    ".......KYK......",
    "......KYK.......",
    "......KK........",
]


def trophy(silver=False):
    gold, dark = (SILVER, SILVER_D) if silver else (GOLD, GOLD_D)
    return from_rows(TROPHY, {"G": gold, "D": dark, "W": W})


def main():
    mario = frame("mario_small.png", 0, 0)
    mario_jump = frame("mario_small.png", 0, 3)
    luigi = frame("luigi_small.png", 0, 0)
    goomba = frame("goomba.png", 0, 0)
    coin = frame("coin.png", 0, 0)
    mushroom = frame("mushroom.png", 0, 0)
    oneup = frame("oneup_mushroom.png", 0, 0)   # el verde
    flower = frame("fire_flower.png", 0, 0)
    star = frame("star_item.png", 2, 0)   # la amarilla
    hammer = frame("hammer.png", 0, 0)
    axe = frame("axe.png", 0, 0)
    flag = frame("flag.png", 0, 0)
    clock = Image.open(S + "icon_clock.png").convert("RGBA")
    # La cabeza del Hammer Bro (con el casco), centrada; con la paleta gris
    # (la del agua), que se ve sobre el circulo verde del aviso.
    bro_head = canvas()
    paste(bro_head, Image.open(S + "hammer_bro.png").convert("RGBA").crop((0, 24, 15, 34)), 1, 3)
    # La punta de la llama de Bowser, dos veces (una arriba de la otra).
    flame = Image.open(S + "bowser_fire.png").convert("RGBA").crop((0, 0, 16, 8))
    fireball = canvas()
    paste(fireball, flame, 0, 1)
    paste(fireball, flame, 0, 8)
    pipe = from_rows(PIPE, {"K": K, "L": GREEN_L, "G": GREEN})

    def warp_icon():
        # El cano mas abajo y la flecha hacia abajo entrando.
        img = canvas()
        paste(img, pipe.crop((0, 0, 16, 10)), 0, 6)
        return paste(img, from_rows(ARROW, {"K": K, "W": W}).crop((0, 0, 16, 7)), 0, 0)

    def stack_coins():
        # Tres monedas chicas separadas (a la mitad: un pixel de cada dos).
        img = canvas()
        small = coin.resize((8, 8), Image.NEAREST)
        paste(img, small, 0, 8)
        paste(img, small, 8, 8)
        paste(img, small, 4, 0)
        return img

    def team():
        img = canvas()
        paste(img, mario.crop((0, 0, 8, 16)), 0, 0)
        paste(img, luigi.crop((8, 0, 16, 16)), 8, 0)
        return img

    def flag_top():
        # El mastil con la bola arriba, la bandera colgando en lo mas alto y
        # una flecha dorada hacia arriba.
        return from_rows(FLAG_TOP, {"K": K, "L": GREEN_L, "G": GREEN, "W": W, "Y": GOLD})

    def defeated_goomba():
        # El Goomba aplastado (tercer cuadro de su hoja), centrado.
        img = canvas()
        return paste(img, frame("goomba.png", 2, 0).crop((0, 8, 16, 16)), 0, 4)

    def speedrun():
        img = clock.copy()
        bolt = from_rows(LIGHTNING + ["." * 16] * 6, {"K": K, "Y": GOLD})
        return paste(img, bolt.crop((4, 0, 14, 10)), 8, 0)

    icons = [
        mario,                          # first_step
        axe,                            # first_castle
        warp_icon(),                    # warp
        flag_top(),                     # flag_top
        fireball,                       # bowser_fire
        coin,                           # collector
        stack_coins(),                  # big_collector
        goomba,                         # hunter
        defeated_goomba(),              # exterminator
        team(),                         # teamwork
        hammer,                         # hammer_stomp
        badge(hammer.copy(), "+"),      # hammer_stomp_plus
        trophy(),                       # beat_game
        red_x(paste(canvas(), pipe, 0, 2)),   # no_skip
        luigi,                          # green_brother
        red_x(mushroom.copy()),         # no_mushroom
        red_x(flower.copy()),           # no_flower
        bro_head,                       # hammer_hunter
        badge(oneup.copy(), "?"),       # life_finder
        badge(trophy(silver=True), "+"),  # beat_plus
        from_rows(SHIELD, {"K": K, "W": W, "B": BLUE}),  # untouchable
        badge(bro_head.copy(), "+"),    # hammer_hunter_plus
        speedrun(),                     # speedrun
        star,                           # perfect
    ]
    sheet = Image.new("RGBA", (16 * len(icons), 16), T)
    for i, icon in enumerate(icons):
        sheet.alpha_composite(icon, (i * 16, 0))
    sheet.save(OUT)
    # Los bloqueados: en blanco y negro y mas oscuros (achievements_locked.png).
    locked = sheet.copy()
    px = locked.load()
    for y in range(locked.height):
        for x in range(locked.width):
            r, g, b, a = px[x, y]
            if a:
                gray = int((0.3 * r + 0.59 * g + 0.11 * b) * 0.55)
                px[x, y] = (gray, gray, gray, a)
    locked.save(OUT.replace(".png", "_locked.png"))
    print("ok", len(icons))


if __name__ == "__main__":
    main()
