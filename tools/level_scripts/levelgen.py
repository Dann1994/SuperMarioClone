"""Ayudas comunes para generar los niveles de la campana (levels/world*/W-L.json).

Cada nivel se arma "a ojo" siguiendo el original: una grilla de 16 filas
(la fila 0 queda fuera de cuadro, el piso son las filas 14-15) mas listas de
bloques con item, warps, enemigos, decorados y plataformas. Ver el README
(formato de nivel) para el significado de cada tile.
"""
import json
import os
import re

ROWS = 16
GROUND = 14   # primera fila de piso
FLOOR = 13    # fila donde se para Mario sobre el piso
LOW = 10      # bloques "bajos" (4 tiles sobre el piso)
HIGH = 6      # bloques "altos" (8 tiles sobre el piso)


class Level:
    def __init__(self, name, width, world="overworld", time=400):
        self.name = name
        self.w = width
        self.world = world
        self.time = time
        self.g = [['.'] * width for _ in range(ROWS)]
        self.blocks = []
        self.warps = []
        self.entities = []
        self.decorations = []
        self.platforms = []
        self.areas = []
        self.checkpoints = []
        self.intro = None

    # ------------------------------------------------------------ tiles
    def put(self, c, r, ch):
        if 0 <= c < self.w and 0 <= r < ROWS:
            self.g[r][c] = ch

    def get(self, c, r):
        if 0 <= c < self.w and 0 <= r < ROWS:
            return self.g[r][c]
        return '.'

    def fill(self, c0, c1, r0, r1, ch):
        """Rectangulo inclusivo."""
        for c in range(c0, c1 + 1):
            for r in range(r0, r1 + 1):
                self.put(c, r, ch)

    def row(self, c0, c1, r, ch):
        self.fill(c0, c1, r, r, ch)

    def ground(self, c0, c1, top=GROUND, ch='#'):
        self.fill(c0, c1, top, ROWS - 1, ch)

    def pit(self, c0, c1):
        self.fill(c0, c1, 0, ROWS - 1, '.')

    def pipe(self, c, h, base=GROUND):
        """Tuberia de 2 de ancho, h tiles de alto apoyada en la fila base."""
        self.fill(c, c + 1, base - h, base - 1, 'T')
        return base - h  # fila del tope

    def stairs_up(self, c, h, ch='S', base=GROUND, width=None):
        """Escalera que sube hacia la derecha: columna i con altura i+1."""
        for i in range(width or h):
            hh = min(i + 1, h)
            self.fill(c + i, c + i, base - hh, base - 1, ch)

    def stairs_down(self, c, h, ch='S', base=GROUND):
        for i in range(h):
            self.fill(c + i, c + i, base - (h - i), base - 1, ch)

    def column(self, c, top, ch='S', base=GROUND):
        self.fill(c, c, top, base - 1, ch)

    def q(self, c, r, item=None, hits=None):
        self.put(c, r, '?')
        if item:
            self.item(c, r, item, hits)

    def item(self, c, r, item, hits=None):
        entry = {"col": c, "row": r, "item": item}
        if hits:
            entry["hits"] = hits
        self.blocks.append(entry)

    def brick_item(self, c, r, item, hits=None):
        """Ladrillo con algo adentro (dispensador 'D')."""
        self.put(c, r, 'D')
        self.item(c, r, item, hits)

    def hidden(self, c, r, item="one_up"):
        self.put(c, r, 'H')
        self.item(c, r, item)

    def coins(self, c0, c1, r):
        self.row(c0, c1, r, 'o')

    # ------------------------------------------------------------ objetos
    def enemy(self, kind, c, r=FLOOR, **kw):
        e = {"type": kind, "col": c, "row": r}
        e.update(kw)
        self.entities.append(e)

    def goombas(self, cols, r=FLOOR):
        for c in cols:
            self.enemy("goomba", c, r)

    def koopa(self, c, r=FLOOR, color=None, state=None):
        kw = {}
        if color:
            kw["color"] = color
        if state:
            kw["state"] = state
        self.enemy("koopa", c, r, **kw)

    def piranha(self, c, h, base=GROUND):
        """Tuberia con Piranha Plant adentro."""
        top = self.pipe(c, h, base)
        self.enemy("piranha", c, top)

    def deco(self, kind, c, r, front=False):
        d = {"type": kind, "col": c, "row": r}
        if front:
            d["front"] = True
        self.decorations.append(d)

    def platform(self, c, r, width, mode, **kw):
        p = {"col": c, "row": r, "width": width, "mode": mode}
        p.update(kw)
        self.platforms.append(p)

    def warp(self, c, r, dest_c, dest_r, side=False):
        w = {"col": c, "row": r, "dest_col": dest_c, "dest_row": dest_r}
        if side:
            w["dir"] = "right"
        self.warps.append(w)

    def area(self, start, end, world=None):
        a = {"start": start, "end": end}
        if world:
            a["world"] = world
        self.areas.append(a)

    def l_pipe(self, c, r):
        """Tuberia en L de 4x4 con esquina sup. izq. en (c, r): boca libre
        abajo a la izquierda (se entra caminando) y el tramo vertical solido
        invisible; el dibujo es el decorado pipe_side."""
        self.fill(c + 2, c + 3, r, r + 3, 'X')
        self.deco("pipe_side", c, r, front=True)

    # ------------------------------------------------------------ piezas
    def flag_end(self, c, castle=True, stairs=8):
        """Escalera de `stairs` + bandera en c + castillo chico (como 1-1).
        Devuelve la columna del castillo."""
        if stairs:
            self.stairs_up(c - stairs - 2, stairs, width=stairs + 1)
        self.fill(c, c, 3, 12, 'F')
        self.put(c, 13, 'S')
        if castle:
            self.deco("castle_small", c + 4, 9)
        return c + 4

    def overworld_scenery(self, c0, c1, small=False):
        """Colinas, arbustos y nubes repitiendo el patron del 1-1, solo
        donde hay piso."""
        pattern = [
            ("hill_big", 0, 11, 5), ("cloud_small", 8, 3, 0), ("bush_big", 11, 13, 5), ("hill_small", 16, 12, 3),
            ("cloud_big", 19, 2, 0), ("bush_small", 23, 13, 3), ("cloud_big", 27, 3, 0), ("cloud_small", 36, 2, 0),
            ("bush_small", 41, 13, 3),
        ]
        if small:
            pattern = [p for p in pattern if p[0].startswith("cloud")]
        for base in range(c0, c1, 48):
            for kind, dx, r, width in pattern:
                col = base + dx
                if col + max(width, 3) > c1:
                    continue
                if width:
                    # Solo sobre piso continuo y libre.
                    ok = all(self.get(cc, GROUND) == '#' and self.get(cc, FLOOR) == '.' and
                             self.get(cc, r) == '.' for cc in range(col, col + width))
                    if not ok:
                        continue
                self.deco(kind, col, r)

    def clouds(self, c0, c1):
        self.overworld_scenery(c0, c1, small=True)

    def castle_ceiling(self, c0, c1, rows=(1, 2)):
        self.fill(c0, c1, rows[0], rows[1], '#')

    def bowser_bridge(self, c, length=13, row=9):
        """Puente de Bowser desde la columna c: lava abajo, Bowser encima, el
        hacha y el cuarto del Toad despues. Devuelve la ultima columna."""
        self.row(c, c + length - 1, row, 'b')
        self.fill(c, c + length - 1, row + 1, ROWS - 1, '.')
        self.fill(c, c + length - 1, 13, ROWS - 1, 'L')
        self.enemy("bowser", c + length - 5, row - 1)
        end = c + length
        # Plataforma del hacha y el cuarto final.
        self.fill(end, end + 15, row, ROWS - 1, '#')
        self.enemy("axe", end, row - 1)
        self.fill(end + 2, end + 15, 1, row - 5, '#')
        return end + 15

    # ------------------------------------------------------------ guardar
    def save(self, path):
        level = {"tile_size": 16, "name": self.name, "time": self.time}
        if self.world != "overworld":
            level["world"] = self.world
        if self.intro is not None:
            level["intro"] = {"area": self.intro}
        if self.checkpoints:
            level["checkpoints"] = self.checkpoints
        if self.areas:
            level["areas"] = self.areas
        level["blocks"] = self.blocks
        level["warps"] = self.warps
        level["entities"] = sorted(self.entities, key=lambda e: e["col"])
        if self.platforms:
            level["platforms"] = self.platforms
        level["decorations"] = self.decorations
        level["rows"] = [''.join(r) for r in self.g]
        s = json.dumps(level, indent=2)
        s = re.sub(r'\{\n\s+("(?:col|start|type|area)"[^}]*?)\n\s+\}',
                   lambda m: '{' + re.sub(r',\n\s+', ', ', m.group(1)) + '}', s)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', newline='\n') as f:
            f.write(s + '\n')
        print(f"{path}: {self.w} columnas, {len(self.entities)} entidades")


def pipe_intro(lv, width=16):
    """Escena de introduccion de 1-2/2-2/4-2/7-2: Mario camina por la
    superficie hasta una tuberia en L. Ocupa las columnas 0..width-1 mas un
    separador; devuelve (columna del separador, columna del warp) -- el
    destino del warp lo completa quien llama."""
    lv.ground(0, width - 1)
    lv.l_pipe(10, 10)
    lv.deco("cloud_small", 1, 3)
    lv.deco("cloud_big", 7, 2)
    lv.deco("bush_small", 4, 13)
    lv.column(width, 0, '#', ROWS)
    lv.area(0, width - 1, "overworld")
    lv.intro = 0
    return width


def exit_area(lv, e0, width=28, flag=14):
    """Salida a la superficie despues de un subterraneo/agua: tuberia por
    la que se sale, bandera y castillo."""
    lv.ground(e0, e0 + width - 1)
    lv.pipe(e0 + 2, 2)
    lv.fill(e0 + flag, e0 + flag, 3, 12, 'F')
    lv.put(e0 + flag, 13, 'S')
    lv.deco("hill_big", e0 + 5, 11)
    lv.deco("cloud_small", e0 + 8, 3)
    lv.deco("bush_small", e0 + 10, 13)
    lv.deco("castle_small", e0 + flag + 4, 9)
    lv.area(e0, e0 + width - 1, "overworld")


# ---------------------------------------------------------------- piezas comunes
def treetop(lv, c, width, top):
    """Copa de arbol de `width` tiles en la fila top, con tronco abajo."""
    lv.row(c, c + width - 1, top, 't')
    if width >= 3:
        lv.fill(c + 1, c + width - 2, top + 1, ROWS - 1, 'r')
    else:
        lv.fill(c, c, top + 1, ROWS - 1, 'r')


def mushroom(lv, c, width, top):
    """Hongo de `width` tiles en la fila top, con el tallo en el medio."""
    lv.row(c, c + width - 1, top, 'm')
    mid = c + width // 2
    lv.fill(mid, mid, top + 1, ROWS - 1, 'n')


def rope_bridge(lv, c0, c1, r=10):
    """Puente de soga entre c0 y c1 (inclusive) en la fila r."""
    lv.row(c0, c1, r, 'R')


def water(lv, c0, c1, top=13):
    """Agua (decorado) en el fondo de un pozo: caer ahi es caer al pozo."""
    lv.fill(c0, c1, top, ROWS - 1, 'W')


def pillar(lv, c, top, width=2, ch='#'):
    lv.fill(c, c + width - 1, top, ROWS - 1, ch)


def cannon(lv, c, h, base=GROUND):
    """Canon de Bill Bala de h tiles apoyado en base (la boca arriba)."""
    lv.fill(c, c, base - h, base - 1, 'K')


def opening(lv, c, powerup="powerup"):
    """El grupo clasico del arranque: ? suelto, fila ladrillo/? y un ? alto."""
    lv.q(c, LOW)
    lv.put(c + 4, LOW, 'B')
    lv.q(c + 5, LOW, powerup)
    lv.put(c + 6, LOW, 'B')
    lv.q(c + 7, LOW)
    lv.put(c + 8, LOW, 'B')
    lv.q(c + 6, HIGH)


def pyramid(lv, c, h, gap=2, ch='S'):
    """Escalera que sube h y baja h con `gap` columnas libres en el medio
    (como las del final del 1-1). Devuelve la columna siguiente."""
    lv.stairs_up(c, h, ch=ch)
    lv.stairs_down(c + h + gap, h, ch=ch)
    return c + 2 * h + gap


def water_ceiling(lv, c0, c1):
    """Superficie del agua: olas en la fila 2 y techo invisible en la 1."""
    lv.row(c0, c1, 1, 'X')
    lv.row(c0, c1, 2, 'w')


def coral(lv, c, top, base=GROUND):
    lv.fill(c, c, top, base - 1, 'c')
