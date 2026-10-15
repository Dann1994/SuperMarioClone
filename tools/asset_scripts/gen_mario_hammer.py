"""Arma assets/sprites/mario_hammer.png (Mario con el traje de Hammer Bro,
como en Mario 3), luigi_hammer.png y hammer_flower.png (la flor martillo)
desde la hoja que paso el usuario, Documentaciones/Mario-Hammer.png.

Colores (pedido del usuario): todo lo blanco de la hoja, en Mario rojizo
muy claro y en Luigi verde muy claro.

Estrella: mario_hammer_star.png y luigi_hammer_star.png, con las mismas 12
paletas que la estrella de Mario y Luigi (3 por tipo de mundo, en el orden
que espera Mario::render): lo claro del traje toma el color de la ropa (o el
del pelo/camisa si la ropa es negra, para no perderse contra el contorno), la
piel el de la piel, y lo negro (rayas, overol, contorno) queda negro: el
traje tiene contorno y Mario no.

La hoja trae cuadros de 16x34 (el casco lo hace 2 px mas alto que Mario
grande) separados por lineas de fondo violeta; cada cuadro tiene el fondo
lila del cielo. Aca se pasan al orden de filas de las hojas de Mario
(Mario.cpp, kAnimRow*): 0 parado, 1 caminar x3, 2 derrape, 3 salto,
4 agachado (el caparazon), 5 trepar x2, 6 nadar x5, 7 tirar x2, con el
fondo transparente.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_mario_hammer.py
"""
from PIL import Image

SOURCE = "Documentaciones/Mario-Hammer.png"
OUT = "assets/sprites/mario_hammer.png"
LUIGI_OUT = "assets/sprites/luigi_hammer.png"
MARIO_PINK = (255, 206, 197)     # rojizo muy claro (0x36 del NES)
LUIGI_GREEN = (189, 247, 173)    # verde muy claro (0x3A del NES)
FLOWER_OUT = "assets/sprites/hammer_flower.png"
W, H = 16, 34
BACKGROUNDS = {(146, 144, 255), (83, 0, 149)}

# (x, y) de la esquina de cada cuadro en la hoja del usuario.
ROW_Y = [0, 37, 72, 108, 145, 181, 217, 255, 292]
COL_X = [0, 17, 34, 51, 68, 85]

# Fila de destino -> cuadros de origen (fila, columna) en la hoja del usuario.
LAYOUT = {
    0: [(0, 0)],                                  # parado
    1: [(1, 0), (1, 1), (1, 2)],                  # caminar
    2: [(2, 0)],                                  # derrape
    3: [(3, 0)],                                  # salto
    4: [(6, 0)],                                  # agachado (caparazon)
    5: [(4, 0), (4, 1)],                          # trepar
    6: [(5, 0), (5, 1), (5, 2), (5, 3), (5, 4)],  # nadar
    7: [(8, 0), (8, 1)],                          # tirar martillo
}
# En las dos ultimas filas los cuadros arrancan 1 px mas a la derecha.
COL_X_SHIFTED = {7: [0, 18], 8: [0, 18]}


def is_background(r, g, b):
    # Tambien los tonos casi iguales al fondo (en los cuadros de tirar hay
    # pixeles de 146,143,255 o 145,143,254 que quedaban como lineas).
    return any(abs(r - br) <= 4 and abs(g - bg) <= 4 and abs(b - bb) <= 4 for br, bg, bb in BACKGROUNDS)


def clean(cell):
    px = cell.load()
    for y in range(cell.height):
        for x in range(cell.width):
            r, g, b, a = px[x, y]
            if is_background(r, g, b):
                px[x, y] = (0, 0, 0, 0)
    return cell


def is_white(c):
    return c[3] and c[0] >= 240 and c[1] >= 240 and c[2] >= 240


def recolor(sheet, color):
    """Todo lo blanco de la hoja, en `color`."""
    out = sheet.copy()
    for fy in range(0, sheet.height, H):
        for fx in range(0, sheet.width, W):
            frame = sheet.crop((fx, fy, fx + W, fy + H))
            for y in range(H):
                for x in range(W):
                    c = frame.getpixel((x, y))
                    if is_white(c):
                        out.putpixel((fx + x, fy + y), color + (c[3],))
    return out


# Las 12 paletas de estrella de Mario/Luigi (mario_big_star.png): por
# bloque, (ropa, piel, pelo/camisa).
STAR_PALETTES = [
    ((12, 147, 0), (255, 254, 255), (234, 158, 34)),
    ((181, 49, 32), (255, 254, 255), (234, 158, 34)),
    ((0, 0, 0), (254, 204, 197), (153, 78, 0)),
    ((173, 173, 173), (255, 254, 255), (234, 158, 34)),
    ((181, 49, 32), (255, 254, 255), (234, 158, 34)),
    ((0, 0, 0), (255, 254, 255), (173, 173, 173)),
    ((0, 0, 0), (0, 124, 141), (153, 78, 0)),
    ((0, 0, 0), (181, 49, 32), (234, 158, 34)),
    ((0, 0, 0), (0, 64, 77), (0, 124, 141)),
    ((153, 78, 0), (0, 124, 141), (0, 0, 0)),
    ((234, 158, 34), (181, 49, 32), (0, 0, 0)),
    ((173, 173, 173), (102, 102, 102), (0, 0, 0)),
]
SKIN = (252, 152, 56)


def star_sheet(sheet, suit_color):
    """Los 12 bloques de la estrella: cada pixel va al color de su papel
    (traje claro, piel o negro) en esa paleta."""
    roles = {"suit": suit_color, "skin": SKIN, "dark": (0, 0, 0)}

    def role_of(rgb):
        return min(roles, key=lambda r: sum((a - b) ** 2 for a, b in zip(rgb, roles[r])))

    out = Image.new("RGBA", (sheet.width, sheet.height * len(STAR_PALETTES)), (0, 0, 0, 0))
    src = sheet.load()
    for block, (clothes, skin, hair) in enumerate(STAR_PALETTES):
        suit = hair if clothes == (0, 0, 0) else clothes
        colors = {"suit": suit, "skin": skin, "dark": (0, 0, 0)}
        for y in range(sheet.height):
            for x in range(sheet.width):
                r, g, b, a = src[x, y]
                if a:
                    out.putpixel((x, block * sheet.height + y), colors[role_of((r, g, b))] + (a,))
    return out


def main():
    sheet = Image.open(SOURCE).convert("RGBA")
    out = Image.new("RGBA", (W * 5, H * 8), (0, 0, 0, 0))
    for row, frames in LAYOUT.items():
        for col, (src_row, src_col) in enumerate(frames):
            xs = COL_X_SHIFTED.get(src_row, COL_X)
            x0, y0 = xs[src_col], ROW_Y[src_row]
            out.paste(clean(sheet.crop((x0, y0, x0 + W, y0 + H))), (col * W, row * H))
    mario = recolor(out, MARIO_PINK)
    luigi = recolor(out, LUIGI_GREEN)
    mario.save(OUT)
    luigi.save(LUIGI_OUT)
    star_sheet(mario, MARIO_PINK).save(OUT.replace(".png", "_star.png"))
    star_sheet(luigi, LUIGI_GREEN).save(LUIGI_OUT.replace(".png", "_star.png"))

    # La flor martillo: arriba a la derecha, recortada a su contenido y
    # centrada abajo en un cuadro de 16x16 si entra (si no, su alto).
    flower = clean(sheet.crop((136, 0, 152, 34)))
    box = flower.getbbox()
    flower = flower.crop(box)
    size = (16, max(16, flower.height))
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.paste(flower, ((16 - flower.width) // 2, size[1] - flower.height))
    canvas.save(FLOWER_OUT)
    print("ok", out.size, canvas.size)


if __name__ == "__main__":
    main()
