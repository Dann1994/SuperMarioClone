"""Recorta de Documentaciones/title.png lo de la pantalla de titulo y el HUD:

  assets/sprites/title_logo.png   cartel "SUPER MARIO BROS." (176x88)
  assets/sprites/title_cursor.png el hongo que marca la opcion (8x8)
  assets/sprites/hud_coin.png     moneda del HUD: 3 frames de 8x8 (brillo
                                  que cicla) x una fila por WorldType
                                  (Overworld, Underwater, Underground, Castle)

El fondo lavanda de la hoja queda transparente. Uso, parado en la raiz del
proyecto (necesita Pillow):
  python tools/asset_scripts/gen_title.py
"""
from PIL import Image

BACKGROUNDS = {(146, 144, 255), (148, 148, 255), (0, 41, 140)}

# Primera celda de la moneda de cada WorldType (frames a 10 px de distancia).
COIN_ORIGINS = [(0, 156), (81, 172), (0, 172), (81, 156)]


def crop(sheet, box):
    piece = sheet.crop(box)
    out = Image.new("RGBA", piece.size, (0, 0, 0, 0))
    src, dst = piece.load(), out.load()
    for y in range(piece.height):
        for x in range(piece.width):
            if src[x, y] not in BACKGROUNDS:
                dst[x, y] = src[x, y] + (255,)
    return out


def main():
    sheet = Image.open("Documentaciones/title.png").convert("RGB")
    crop(sheet, (40, 224, 216, 312)).save("assets/sprites/title_logo.png")
    crop(sheet, (0, 440, 8, 448)).save("assets/sprites/title_cursor.png")
    coins = Image.new("RGBA", (8 * 3, 8 * 4), (0, 0, 0, 0))
    for row, (x0, y0) in enumerate(COIN_ORIGINS):
        for frame in range(3):
            x = x0 + frame * 10
            coins.paste(crop(sheet, (x, y0, x + 8, y0 + 8)), (frame * 8, row * 8))
    coins.save("assets/sprites/hud_coin.png")


if __name__ == "__main__":
    main()
