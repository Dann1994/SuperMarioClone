"""Fuente del Super Mario Bros. original (8x8 por caracter) -> tabla C++
para src/render/SmbFontGlyphs.inc.

Los caracteres que trae la hoja Documentaciones/title.png (seccion "Text":
numeros, A-Z, guion, la "x" chica, signo de exclamacion, punto y (c)) se
recortan de ahi; los simbolos que no estan en el juego original (?, :, /,
parentesis...) van dibujados a mano en GLYPHS, con el mismo estilo. Cada
glifo son 8 filas de 8 columnas ('X' = pixel prendido). Uso, parado en la
raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/smb_font.py
"""
from PIL import Image

# Seccion "Text" de title.png: grilla de 9 px (8 de letra + 1 de separacion).
SHEET_ROWS = [
    (105, "0123456789ABCDEFGHIJKLMN"),
    (114, "OPQRSTUVWXYZ-x!.@"),  # '@' = el (c)
]

GLYPHS = {
    '0': ["..XXX...", ".X..XX..", "XX...XX.", "XX...XX.", "XX...XX.", ".XX..X..", "..XXX...", "........"],
    '1': ["...XX...", "..XXX...", "...XX...", "...XX...", "...XX...", "...XX...", ".XXXXXX.", "........"],
    '2': [".XXXXX..", "XX...XX.", "....XXX.", "..XXXX..", ".XXXX...", "XXX.....", "XXXXXXX.", "........"],
    '3': [".XXXXXX.", "....XX..", "...XX...", "..XXXX..", ".....XX.", "XX...XX.", ".XXXXX..", "........"],
    '4': ["...XXX..", "..XXXX..", ".XX.XX..", "XX..XX..", "XXXXXXX.", "....XX..", "....XX..", "........"],
    '5': ["XXXXXX..", "XX......", "XXXXXX..", ".....XX.", ".....XX.", "XX...XX.", ".XXXXX..", "........"],
    '6': ["..XXXX..", ".XX.....", "XX......", "XXXXXX..", "XX...XX.", "XX...XX.", ".XXXXX..", "........"],
    '7': ["XXXXXXX.", "XX...XX.", "....XX..", "...XX...", "..XX....", "..XX....", "..XX....", "........"],
    '8': [".XXXX...", "XX...X..", "XXX..X..", ".XXXX...", "X..XXXX.", "X....XX.", ".XXXXX..", "........"],
    '9': [".XXXXX..", "XX...XX.", "XX...XX.", ".XXXXXX.", ".....XX.", "....XX..", ".XXXX...", "........"],
    'A': ["..XXX...", ".XX.XX..", "XX...XX.", "XX...XX.", "XXXXXXX.", "XX...XX.", "XX...XX.", "........"],
    'B': ["XXXXXX..", "XX...XX.", "XX...XX.", "XXXXXX..", "XX...XX.", "XX...XX.", "XXXXXX..", "........"],
    'C': ["..XXXX..", ".XX..XX.", "XX......", "XX......", "XX......", ".XX..XX.", "..XXXX..", "........"],
    'D': ["XXXXX...", "XX..XX..", "XX...XX.", "XX...XX.", "XX...XX.", "XX..XX..", "XXXXX...", "........"],
    'E': ["XXXXXXX.", "XX......", "XX......", "XXXXXX..", "XX......", "XX......", "XXXXXXX.", "........"],
    'F': ["XXXXXXX.", "XX......", "XX......", "XXXXXX..", "XX......", "XX......", "XX......", "........"],
    'G': ["..XXXXX.", ".XX.....", "XX......", "XX..XXX.", "XX...XX.", ".XX..XX.", "..XXXXX.", "........"],
    'H': ["XX...XX.", "XX...XX.", "XX...XX.", "XXXXXXX.", "XX...XX.", "XX...XX.", "XX...XX.", "........"],
    'I': [".XXXXXX.", "...XX...", "...XX...", "...XX...", "...XX...", "...XX...", ".XXXXXX.", "........"],
    'J': ["....XXX.", ".....XX.", ".....XX.", ".....XX.", "XX...XX.", "XX...XX.", ".XXXXX..", "........"],
    'K': ["XX...XX.", "XX..XX..", "XX.XX...", "XXXX....", "XXXXX...", "XX.XXX..", "XX..XXX.", "........"],
    'L': ["XX......", "XX......", "XX......", "XX......", "XX......", "XX......", "XXXXXXX.", "........"],
    'M': ["XX...XX.", "XXX.XXX.", "XXXXXXX.", "XXXXXXX.", "XX.X.XX.", "XX...XX.", "XX...XX.", "........"],
    'N': ["XX...XX.", "XXX..XX.", "XXXX.XX.", "XXXXXXX.", "XX.XXXX.", "XX..XXX.", "XX...XX.", "........"],
    'O': [".XXXXX..", "XX...XX.", "XX...XX.", "XX...XX.", "XX...XX.", "XX...XX.", ".XXXXX..", "........"],
    'P': ["XXXXXX..", "XX...XX.", "XX...XX.", "XX...XX.", "XXXXXX..", "XX......", "XX......", "........"],
    'Q': [".XXXXX..", "XX...XX.", "XX...XX.", "XX...XX.", "XX.XXXX.", "XX..XX..", ".XXXX.X.", "........"],
    'R': ["XXXXXX..", "XX...XX.", "XX...XX.", "XX..XXX.", "XXXXX...", "XX.XXX..", "XX..XXX.", "........"],
    'S': [".XXXX...", "XX..XX..", "XX......", ".XXXXX..", ".....XX.", "XX...XX.", ".XXXXX..", "........"],
    'T': [".XXXXXX.", "...XX...", "...XX...", "...XX...", "...XX...", "...XX...", "...XX...", "........"],
    'U': ["XX...XX.", "XX...XX.", "XX...XX.", "XX...XX.", "XX...XX.", "XX...XX.", ".XXXXX..", "........"],
    'V': ["XX...XX.", "XX...XX.", "XX...XX.", "XXX.XXX.", ".XXXXX..", "..XXX...", "...X....", "........"],
    'W': ["XX...XX.", "XX...XX.", "XX.X.XX.", "XXXXXXX.", "XXXXXXX.", "XXX.XXX.", "XX...XX.", "........"],
    'X': ["XX...XX.", "XXX.XXX.", ".XXXXX..", "..XXX...", ".XXXXX..", "XXX.XXX.", "XX...XX.", "........"],
    'Y': [".XX..XX.", ".XX..XX.", ".XX..XX.", "..XXXX..", "...XX...", "...XX...", "...XX...", "........"],
    'Z': ["XXXXXXX.", "....XXX.", "...XXX..", "..XXX...", ".XXX....", "XXX.....", "XXXXXXX.", "........"],
    # El "x" chico del contador de monedas (como el HUD original).
    'x': ["........", "........", "XX...XX.", ".XX.XX..", "..XXX...", ".XX.XX..", "XX...XX.", "........"],
    '-': ["........", "........", "........", "XXXXXX..", "........", "........", "........", "........"],
    '!': ["...XX...", "...XX...", "...XX...", "...XX...", "...XX...", "........", "...XX...", "........"],
    '?': [".XXXXX..", "XX...XX.", "....XX..", "...XX...", "...XX...", "........", "...XX...", "........"],
    '.': ["........", "........", "........", "........", "........", "...XX...", "...XX...", "........"],
    ',': ["........", "........", "........", "........", "........", "...XX...", "...XX...", "..XX...."],
    ':': ["........", "...XX...", "...XX...", "........", "...XX...", "...XX...", "........", "........"],
    "'": ["...XX...", "...XX...", "..XX....", "........", "........", "........", "........", "........"],
    '/': [".....XX.", "....XX..", "...XX...", "..XX....", ".XX.....", "XX......", "........", "........"],
    '(': ["....XX..", "...XX...", "..XX....", "..XX....", "..XX....", "...XX...", "....XX..", "........"],
    ')': ["..XX....", "...XX...", "....XX..", "....XX..", "....XX..", "...XX...", "..XX....", "........"],
    '+': ["........", "...XX...", "...XX...", ".XXXXXX.", "...XX...", "...XX...", "........", "........"],
    '=': ["........", "........", "XXXXXX..", "........", "XXXXXX..", "........", "........", "........"],
    '<': ["....XX..", "...XX...", "..XX....", ".XX.....", "..XX....", "...XX...", "....XX..", "........"],
    '>': ["..XX....", "...XX...", "....XX..", ".....XX.", "....XX..", "...XX...", "..XX....", "........"],
    '*': ["........", "XX...XX.", ".XX.XX..", "XXXXXXX.", ".XX.XX..", "XX...XX.", "........", "........"],
    '_': ["........", "........", "........", "........", "........", "........", "........", "XXXXXXX."],
    '[': ["..XXXX..", "..XX....", "..XX....", "..XX....", "..XX....", "..XX....", "..XXXX..", "........"],
    ']': ["..XXXX..", "....XX..", "....XX..", "....XX..", "....XX..", "....XX..", "..XXXX..", "........"],
    '#': [".XX.XX..", ".XX.XX..", "XXXXXXX.", ".XX.XX..", "XXXXXXX.", ".XX.XX..", ".XX.XX..", "........"],
    '%': ["XX...XX.", "XX..XX..", "...XX...", "..XX....", ".XX.....", "XX..XX..", "X...XX..", "........"],
    # El (c) del titulo original.
    '@': ["..XXXX..", ".X....X.", "X..XX..X", "X.X....X", "X.X....X", "X..XX..X", ".X....X.", "..XXXX.."],
}


def load_sheet_glyphs():
    sheet = Image.open("Documentaciones/title.png").convert("RGB")
    px = sheet.load()
    glyphs = {}
    for y0, chars in SHEET_ROWS:
        for i, ch in enumerate(chars):
            x0 = i * 9
            rows = []
            for y in range(8):
                rows.append("".join("X" if min(px[x0 + x, y0 + y]) >= 200 else "." for x in range(8)))
            glyphs[ch] = rows
    return glyphs


def main():
    GLYPHS.update(load_sheet_glyphs())
    lines = ["// Generado por tools/asset_scripts/smb_font.py: no editar a mano.",
             "// {caracter, 8 filas (bit 7 = columna izquierda)}"]
    for ch, rows in GLYPHS.items():
        assert len(rows) == 8 and all(len(r) == 8 for r in rows), ch
        values = ", ".join("0x%02X" % int(r.replace('X', '1').replace('.', '0'), 2) for r in rows)
        lit = "'\\''" if ch == "'" else f"'{ch}'"
        lines.append(f"{{{lit}, {{{values}}}}},")
    with open("src/render/SmbFontGlyphs.inc", "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
