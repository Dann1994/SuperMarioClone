"""Pasa las tuberias en L hechas con el decorado "pipe_side" a tiles: la
horizontal ('Y', boca de 2x2 + los tramos de mas de "length") y el tramo
vertical ('T'), en vez de los bloques invisibles 'X'. No toca nada mas del
nivel (sirve para los niveles retocados a mano, que rom_build.py no
regenera). El editor hace lo mismo al abrir un nivel.

Uso, parado en la raiz del proyecto:
  python tools/level_scripts/convert_pipe_side.py levels/world1/1-1.json [...]
"""
import json
import sys


def convert(level):
    rows = [list(r) for r in level["rows"]]
    height, width = len(rows), len(rows[0]) if rows else 0
    kept, converted = [], 0
    for decoration in level.get("decorations", []):
        if decoration.get("type") != "pipe_side":
            kept.append(decoration)
            continue
        col, row, length = decoration["col"], decoration["row"], decoration.get("length", 0)
        vertical = col + 2 + length

        def put(c, r, ch):
            if 0 <= c < width and 0 <= r < height and rows[r][c] in ".X":
                rows[r][c] = ch

        for c in range(col, vertical):
            for r in (row + 2, row + 3):
                put(c, r, 'Y')
        for c in (vertical, vertical + 1):
            for r in range(row, row + 4):
                put(c, r, 'T')
        converted += 1
    level["decorations"] = kept
    level["rows"] = ["".join(r) for r in rows]
    return converted


def main():
    for path in sys.argv[1:]:
        with open(path, encoding="utf-8") as f:
            level = json.load(f)
        n = convert(level)
        if n:
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(json.dumps(level, indent=2) + "\n")
        print(f"{path}: {n} tuberias en L pasadas a tiles")


if __name__ == "__main__":
    main()
