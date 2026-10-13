"""Toad y la princesa (al final de los castillos).

  assets/sprites/retainers.png   2 frames de 16x24: Toad, princesa; de
                                 "End-of-Castle characters" en
                                 Documentaciones/Objetos.png. Vienen sobre
                                 negro: se borra solo el negro que toca el
                                 borde (relleno desde afuera), asi quedan los
                                 contornos de adentro.

Uso, parado en la raiz del proyecto (necesita Pillow):
  python tools/asset_scripts/gen_retainers.py
"""
from PIL import Image

FRAMES = [(0, 444), (18, 444)]
W, H = 16, 24


def main():
    sheet = Image.open("Documentaciones/Objetos.png").convert("RGB")
    src = sheet.load()
    out = Image.new("RGBA", (W * len(FRAMES), H), (0, 0, 0, 0))
    dst = out.load()
    for i, (x0, y0) in enumerate(FRAMES):
        pixels = [[src[x0 + x, y0 + y] for x in range(W)] for y in range(H)]
        outside = [[False] * W for _ in range(H)]
        stack = [(x, y) for x in range(W) for y in (0, H - 1)] + [(x, y) for y in range(H) for x in (0, W - 1)]
        while stack:
            x, y = stack.pop()
            if not (0 <= x < W and 0 <= y < H) or outside[y][x] or pixels[y][x] != (0, 0, 0):
                continue
            outside[y][x] = True
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
        for y in range(H):
            for x in range(W):
                if not outside[y][x]:
                    dst[i * W + x, y] = pixels[y][x] + (255,)
    out.save("assets/sprites/retainers.png")


if __name__ == "__main__":
    main()
