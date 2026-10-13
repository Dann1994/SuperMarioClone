"""Fuego verde de Luigi, recortado de Documentaciones/Luigi_moderno.png (hoja
"by PixelLuka"):
  assets/sprites/fireball_green.png            4 cuadros de 8x8 girando
  assets/sprites/fireball_explosion_green.png  3 cuadros de 16x16

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_fireball_green.py
"""
from PIL import Image

SHEET = "Documentaciones/Luigi_moderno.png"
BACKGROUNDS = {(146, 144, 255), (201, 200, 255), (0, 41, 140)}
FIREBALL = [(38, 88), (48, 88), (58, 88), (68, 88)]
EXPLOSION = [(38, 99), (56, 99), (74, 99)]


def crop(sheet, x, y, w, h):
    piece = sheet.crop((x, y, x + w, y + h))
    px = piece.load()
    for yy in range(h):
        for xx in range(w):
            if px[xx, yy][:3] in BACKGROUNDS:
                px[xx, yy] = (0, 0, 0, 0)
    return piece


def main():
    sheet = Image.open(SHEET).convert("RGBA")
    balls = Image.new("RGBA", (32, 8), (0, 0, 0, 0))
    for i, (x, y) in enumerate(FIREBALL):
        balls.paste(crop(sheet, x, y, 8, 8), (i * 8, 0))
    balls.save("assets/sprites/fireball_green.png")
    boom = Image.new("RGBA", (48, 16), (0, 0, 0, 0))
    for i, (x, y) in enumerate(EXPLOSION):
        boom.paste(crop(sheet, x, y, 16, 16), (i * 16, 0))
    boom.save("assets/sprites/fireball_explosion_green.png")


if __name__ == "__main__":
    main()
