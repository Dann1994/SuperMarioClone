"""Arma assets/sprites/mario_hammer.png (Mario con el traje de Hammer Bro,
como en Mario 3) y hammer_flower.png (la flor martillo) desde la hoja que
paso el usuario, Documentaciones/Mario-Hammer.png.

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


def main():
    sheet = Image.open(SOURCE).convert("RGBA")
    out = Image.new("RGBA", (W * 5, H * 8), (0, 0, 0, 0))
    for row, frames in LAYOUT.items():
        for col, (src_row, src_col) in enumerate(frames):
            xs = COL_X_SHIFTED.get(src_row, COL_X)
            x0, y0 = xs[src_col], ROW_Y[src_row]
            out.paste(clean(sheet.crop((x0, y0, x0 + W, y0 + H))), (col * W, row * H))
    out.save(OUT)

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
