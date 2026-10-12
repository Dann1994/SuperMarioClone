"""Genera levels/world1/1-2.json (layout del 1-2 original, a ojo)."""
import json
import re
import sys

ROWS = 16
INTRO_W = 16
U0 = INTRO_W + 1          # primera columna del subterraneo
UW = 142
E0 = U0 + UW + 1          # primera columna de la salida
EW = 28
W = E0 + EW

g = [['.'] * W for _ in range(ROWS)]


def put(c, r, ch):
    if 0 <= c < W and 0 <= r < ROWS:
        g[r][c] = ch


def ground(c0, c1):
    for c in range(c0, c1 + 1):
        put(c, 14, '#')
        put(c, 15, '#')


def column(c, top, ch, bottom=13):
    for r in range(top, bottom + 1):
        put(c, r, ch)


def pipe(c, h):
    for r in range(14 - h, 14):
        put(c, r, 'T')
        put(c + 1, r, 'T')


def l_pipe(c, r):
    """Tuberia en L de 4x4 tiles con esquina sup. izq. en (c, r): boca libre
    (2x2 abajo a la izquierda) y el tramo vertical solido invisible."""
    for rr in range(r, r + 4):
        put(c + 2, rr, 'X')
        put(c + 3, rr, 'X')


decorations = []
entities = []
blocks = []
warps = []
platforms = []

# ---------------------------------------------------------------- intro (superficie)
ground(0, INTRO_W - 1)
l_pipe(10, 10)
decorations += [
    {"type": "pipe_side", "col": 10, "row": 10, "front": True},
    {"type": "cloud_small", "col": 1, "row": 3},
    {"type": "cloud_big", "col": 7, "row": 2},
    {"type": "bush_small", "col": 4, "row": 13},
]
warps.append({"col": 10, "row": 12, "dest_col": U0 + 2, "dest_row": 4, "dir": "right"})

# ---------------------------------------------------------------- separadores
column(INTRO_W, 0, '#', 15)
column(E0 - 1, 0, '#', 15)

# ---------------------------------------------------------------- subterraneo
def u(x):
    return U0 + x


pits = set(range(u(68), u(71))) | set(range(u(112), u(126)))
for c in range(u(0), u(UW)):
    if c not in pits:
        put(c, 14, '#')
        put(c, 15, '#')
column(u(0), 3, 'B')                       # pared de la izquierda
for x in list(range(6, 111)) + list(range(128, UW)):
    put(u(x), 3, 'B')                      # techo (sin techo sobre los elevadores)

# bloques del arranque
put(u(10), 10, '?')
blocks.append({"col": u(10), "row": 10, "item": "powerup"})
for x in range(11, 15):
    put(u(x), 10, '?')

# columnas de ladrillo en arco con monedas arriba
for x, top in [(26, 11), (28, 9), (30, 7), (32, 7), (34, 9), (36, 11)]:
    column(u(x), top, 'B')
    put(u(x), top - 1, 'o')

# pasillo de ladrillos con monedas encima
for x in range(42, 53):
    put(u(x), 9, 'B')
for x in range(43, 52, 2):
    put(u(x), 8, 'o')

# tuberias
pipe(u(55), 3)
pipe(u(60), 4)
pipe(u(94), 2)
pipe(u(100), 3)

# ladrillos con premio
for x in range(74, 81):
    put(u(x), 10, 'B')
put(u(77), 10, '?')
blocks.append({"col": u(77), "row": 10, "item": "powerup"})
put(u(79), 10, 'D')
blocks.append({"col": u(79), "row": 10, "item": "coin", "hits": 10})

# escalera de bloques duros hasta los elevadores
for i in range(4):
    column(u(104 + i), 13 - i, 'S')
for x in range(108, 112):
    column(u(x), 10, 'S')

# elevadores (dos que bajan, dos que suben)
platforms += [
    {"col": u(115), "row": 2, "width": 3, "mode": "lift_down", "speed": 50},
    {"col": u(115), "row": 9, "width": 3, "mode": "lift_down", "speed": 50},
    {"col": u(121), "row": 6, "width": 3, "mode": "lift_up", "speed": 50},
    {"col": u(121), "row": 13, "width": 3, "mode": "lift_up", "speed": 50},
]

# tuberia de salida (se entra caminando) y pared del final
l_pipe(u(133), 10)
decorations.append({"type": "pipe_side", "col": u(133), "row": 10, "front": True})
warps.append({"col": u(133), "row": 12, "dest_col": E0 + 2, "dest_row": 10, "dir": "right"})
for x in range(139, UW):
    column(u(x), 3, 'B')

# enemigos
for x in [16, 18, 58, 84, 86]:
    entities.append({"type": "goomba", "col": u(x), "row": 13})
entities.append({"type": "koopa", "col": u(40), "row": 13})
entities.append({"type": "koopa", "col": u(90), "row": 13, "color": "red"})
entities.append({"type": "goomba", "col": u(47), "row": 13})

# ---------------------------------------------------------------- salida (superficie)
def e(x):
    return E0 + x


ground(e(0), e(EW - 1))
pipe(e(2), 2)
column(e(14), 3, 'F', 12)
put(e(14), 13, 'S')
decorations += [
    {"type": "hill_big", "col": e(5), "row": 11},
    {"type": "cloud_small", "col": e(8), "row": 3},
    {"type": "bush_small", "col": e(10), "row": 13},
    {"type": "castle_small", "col": e(18), "row": 9},
]

level = {
    "tile_size": 16,
    "name": "1-2",
    "time": 400,
    "intro": {"area": 0},
    "checkpoints": [u(72)],
    "areas": [
        {"start": 0, "end": INTRO_W - 1, "world": "overworld"},
        {"start": U0, "end": U0 + UW - 1, "world": "underground"},
        {"start": E0, "end": E0 + EW - 1, "world": "overworld"},
    ],
    "blocks": blocks,
    "warps": warps,
    "entities": entities,
    "platforms": platforms,
    "decorations": decorations,
    "rows": [''.join(r) for r in g],
}
s = json.dumps(level, indent=2)
s = re.sub(r'\{\n\s+("(?:col|start|type|area)"[^}]*?)\n\s+\}', lambda m: '{' + re.sub(r',\n\s+', ', ', m.group(1)) + '}', s)
open(sys.argv[1], 'w', newline='\n').write(s + '\n')
print(W, 'columnas')
