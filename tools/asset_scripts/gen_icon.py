"""Icono del juego: la cabeza de Mario de "Overworld Objects" en
Documentaciones/Objetos.png (la del contador de vidas).

  assets/icon.png     32x32 (icono de la ventana, SDL_SetWindowIcon)
  resources/icon.ico  16/32/48/64/128/256 (icono del .exe, resources/app.rc)

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_icon.py
"""
from PIL import Image


def main():
    sheet = Image.open("Documentaciones/Objetos.png").convert("RGBA")
    head = sheet.crop((16, 8, 32, 24))
    px = head.load()
    for y in range(head.height):
        for x in range(head.width):
            r, g, b, a = px[x, y]
            # Fondo azul y lila de la hoja: transparente.
            if (r, g, b) == (0, 41, 140) or (b > 240 and r < 160):
                px[x, y] = (0, 0, 0, 0)
    head.resize((32, 32), Image.NEAREST).save("assets/icon.png")
    big = head.resize((256, 256), Image.NEAREST)
    big.save("resources/icon.ico", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])


if __name__ == "__main__":
    main()
