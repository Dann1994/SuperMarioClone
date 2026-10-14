"""Arma los 32 niveles a partir de la ROM original: la ROM se usa solo como
referencia de como estan armados; los tiles, graficos y el motor son los
nuestros. La ROM no va en el repositorio (.gitignore).

Uso, parado en la raiz del proyecto:
  python tools/level_scripts/rom_build.py "Documentaciones/Super Mario Bros (E).nes" [carpeta]
(carpeta por defecto: levels). Con --dump W-L imprime ese nivel en texto.
Los niveles guardados con el editor (marca "edited": true) no se regeneran;
--force los pisa igual.
"""
import json
import os
import re
import sys

from rom_levels import (Rom, parse_area, parse_enemies, has_intro_pipe, STAIRCASE_HEIGHT, STAIRCASE_ROW,
                        HALFWAY_PAGE_NYBBLES, WATER, GROUND, UNDERGROUND, CASTLE, WORLD_NAMES, ROW_OFFSET, ROWS)

TERRAIN_BITS = [
    0b0000000000000, 0b1100000000000, 0b1100000000001, 0b1100000000111,
    0b1100000001111, 0b1100011111111, 0b1111100000001, 0b1111100000111,
    0b1111100001111, 0b1111110000001, 0b0000000000001, 0b1111110001111,
    0b1111111110001, 0b1100011111001, 0b1100011110001, 0b1111111111111,
]

# Escenografia de fondo del original (nubes / colinas y arbustos / arboles y
# cercos): por pagina % 3 y columna; nibble alto = fila, bajo = pieza
# (1-3 nube, 4-6 arbusto, 7-9 colina, 10 cerco, 11 arbol alto, 12 bajo).
BACK_SCENERY = [
    [[0x93, 0, 0, 0x11, 0x12, 0x12, 0x13, 0, 0, 0x51, 0x52, 0x53, 0, 0, 0, 0],
     [0, 0, 0x01, 0x02, 0x02, 0x03, 0, 0, 0, 0, 0, 0, 0x91, 0x92, 0x93, 0],
     [0, 0, 0, 0x51, 0x52, 0x53, 0x41, 0x42, 0x43, 0, 0, 0, 0, 0, 0x91, 0x92]],
    [[0x97, 0x87, 0x88, 0x89, 0x99, 0, 0, 0, 0x11, 0x12, 0x13, 0xa4, 0xa5, 0xa5, 0xa5, 0xa6],
     [0x97, 0x98, 0x99, 0x01, 0x02, 0x03, 0, 0xa4, 0xa5, 0xa6, 0, 0x11, 0x12, 0x12, 0x12, 0x13],
     [0, 0, 0, 0, 0x01, 0x02, 0x02, 0x03, 0, 0xa4, 0xa5, 0xa5, 0xa6, 0, 0, 0]],
    [[0x11, 0x12, 0x12, 0x13, 0, 0, 0, 0, 0, 0, 0, 0x9c, 0, 0x8b, 0xaa, 0xaa],
     [0xaa, 0xaa, 0x11, 0x12, 0x13, 0x8b, 0, 0x9c, 0x9c, 0, 0, 0x01, 0x02, 0x03, 0x11, 0x12],
     [0x12, 0x13, 0, 0, 0, 0, 0xaa, 0xaa, 0x9c, 0xaa, 0, 0x8b, 0, 0x01, 0x02, 0x03]],
]

# RenderUnderPart no pisa bloques ?, monedas ni el medio de copas/hongos.
PROTECTED = set('?otm')
# Altura de entrada del jugador segun el encabezado del area.
ENTRANCE_Y = [0x00, 0x20, 0xb0, 0x50, 0x00, 0x00, 0xb0, 0xb0]
TIMER = {0: 400, 1: 400, 2: 300, 3: 200}


def piece_group(kind):
    if kind <= 3:
        return 'cloud'
    if kind <= 6:
        return 'bush'
    if kind <= 9:
        return 'hill'
    return 'other'


class AreaBuild:
    """Un area del original dibujada en nuestra grilla (filas 0..15)."""

    def __init__(self, rom, pointer, no_piranhas, hard):
        self.pointer = pointer & 0x7f
        self.header, self.objects = parse_area(rom, self.pointer)
        self.enemies_raw, self.changes = parse_enemies(rom, self.pointer)
        self.type = self.header["type"]
        self.no_piranhas = no_piranhas
        self.hard = hard
        self.stair_h = list(rom.bytes(STAIRCASE_HEIGHT, 9))
        self.stair_r = list(rom.bytes(STAIRCASE_ROW, 9))
        last = max([o.col + (o.length if o.row not in (0x0D, 0x0E) else 0) for o in self.objects] +
                   [e.col for e in self.enemies_raw] + [16])
        self.width = (last // 16 + 2) * 16
        self.cols = [['.'] * ROWS for _ in range(self.width)]
        self.blocks = []          # (col, row, item, hits)
        self.entities = []
        self.decorations = []
        self.platforms = []
        self.pipes = []           # (col, row, kind): 'vertical' o 'side'
        self.vines = []           # (col, row): ladrillos con enredadera
        self.flag_col = None
        self.render()
        self.place_enemies()

    # -- utilidades
    def put(self, col, mrow, ch, protect=True):
        """mrow: fila de metatile del original (0..12)."""
        r = mrow + ROW_OFFSET
        if 0 <= col < self.width and 0 <= r < ROWS:
            if protect and self.cols[col][r] in PROTECTED:
                return
            self.cols[col][r] = ch

    def under(self, col, mrow, ch, count):
        """RenderUnderPart: count+1 metatiles desde mrow hacia abajo (hasta la 12)."""
        n = 1 if count > 0x80 else count + 1
        for k in range(n):
            if mrow + k > 12:
                break
            self.put(col, mrow + k, ch)

    def block_item(self, col, mrow, item, hits=None):
        self.blocks.append((col, mrow + ROW_OFFSET, item, hits))

    # -- dibujo columna por columna, como el original
    def render(self):
        terrain = self.header["terrain"]
        background = self.header["background"]
        foreground = self.header["foreground"]
        by_col = {}
        for o in self.objects:
            by_col.setdefault(o.col, []).append(o)
        active = []
        self.scenery_cols = []
        self.wall_cols = []       # columnas con muralla de castillo (foreground 2)
        for col in range(self.width):
            # AreaParserCore: primero el escenario y el terreno de la columna
            # con los valores vigentes, despues los objetos (ProcessAreaData).
            # Un cambio de terreno (fila E) recien se ve desde la columna
            # siguiente.
            if col > 0:
                for o in by_col.get(col - 1, []):
                    if o.row == 0x0E:
                        if (o.data1 & 0x40) == 0:
                            terrain = o.data1 & 0x0f
                            background = (o.data1 >> 4) & 3
                        else:
                            foreground = (o.data1 & 7) if (o.data1 & 7) < 4 else 0
            self.scenery_cols.append(background)
            if foreground == 1:          # bajo el agua: superficie arriba
                self.put(col, 0, 'w', False)
                self.cols[col][ROW_OFFSET - 1] = 'X'
            elif foreground == 2:        # muralla de castillo de fondo (filas 5-10)
                self.wall_cols.append(col)
            elif foreground == 3:        # sobre el agua (lava en los castillos)
                fill = 'L' if self.type == CASTLE else 'W'
                self.put(col, 11, fill, False)
                self.put(col, 12, fill, False)
            bits = TERRAIN_BITS[terrain]
            if self.header["cloud"]:
                bits &= 0x08ff
            for j in range(13):
                if bits & (1 << j):
                    ch = 'B' if (self.type == UNDERGROUND and j < 11) else '#'
                    if self.header["cloud"]:
                        ch = 'N'     # areas de nubes: el piso son bloques de nube
                    self.put(col, j, ch, False)
            still = []
            for o, first in active:
                if col - first <= self.object_length(o):
                    self.render_object(o, col, col - first)
                    still.append((o, first))
            for o in by_col.get(col, []):
                if o.row == 0x0E:
                    continue
                self.render_object(o, col, 0)
                if self.object_length(o) > 0:
                    still.append((o, col))
            active = still
        self.make_scenery()

    def object_length(self, o):
        r, k, L = o.row, o.kind, o.length
        if r <= 0x0B:
            if k == 1:
                return L if self.header["style"] in (0, 1) else 0
            if k in (2, 3, 4):
                return L
            if k == 7:
                return 1
            return 0
        if r == 0x0C:
            return L
        if r == 0x0D:
            sub = o.data1 & 0x3f
            return {0x00: 3, 0x04: 12}.get(sub, 0)
        if r == 0x0F:
            return {2: 4, 3: L, 4: 3}.get(k, 0)
        return 0

    def render_object(self, o, col, off):
        r, k, L = o.row, o.kind, o.length
        t = self.type
        if r <= 0x0B:
            if k == 0:
                if off == 0:
                    self.render_special(col, r, L)
            elif k == 1:
                style = self.header["style"]
                if style == 0:      # copa de arbol con tronco
                    self.put(col, r, 't')
                    if 0 < off < L:
                        self.under(col, r + 1, 'r', 0x0f)
                elif style == 1:    # hongo con tallo en el medio
                    self.put(col, r, 'm')
                    if off == L // 2:
                        self.under(col, r + 1, 'n', 0x0f)
                else:               # canon de Bill Bala
                    self.under(col, r, 'K', L)
            elif k == 2:
                # RowOfBricks: en las areas de nubes son bloques de nube.
                self.put(col, r, 'N' if self.header["cloud"] else 'c' if t == WATER else 'B')
            elif k == 3:
                self.put(col, r, '#' if t in (WATER, CASTLE) else 'S')
            elif k == 4:
                self.put(col, r, 'o')
            elif k == 5:
                self.under(col, r, 'c' if t == WATER else 'B', L)
            elif k == 6:
                self.under(col, r, '#' if t in (WATER, CASTLE) else 'S', L)
            elif k == 7:
                height = L & 7
                self.under(col, r, 'T', height)
                if off == 0:
                    if L & 8:
                        self.pipes.append((col, r + ROW_OFFSET, 'vertical'))
                    if not self.no_piranhas:
                        self.entities.append({"type": "piranha", "col": col, "row": r + ROW_OFFSET})
            return
        if r == 0x0C:
            if k == 0:          # pozo
                for j in range(8, 13):
                    self.put(col, j, '.', False)
                # En el agua cada pozo es un remolino (Hole_Empty del
                # original): desde un bloque antes del pozo hasta su borde
                # derecho, L + 2 bloques.
                if off == 0 and t == WATER:
                    self.entities.append({"type": "whirlpool", "col": col - 1, "row": 13, "length": L + 2})
            elif k == 2:
                self.put(col, 7, 'R')
            elif k == 3:
                self.put(col, 8, 'R')
            elif k == 4:
                self.put(col, 10, 'R')
            elif k == 5:        # pozo con agua (lava en los castillos)
                for j in range(8, 13):
                    self.put(col, j, '.', False)
                fill = 'L' if t == CASTLE else 'W'
                for j in (10, 11, 12):
                    self.put(col, j, fill, False)
            elif k == 6:
                self.put(col, 3, '?')
            elif k == 7:
                self.put(col, 7, '?')
            return
        if r == 0x0D:
            sub = o.data1 & 0x3f
            if sub == 0x00 and off == 0:     # tuberia de la escena de entrada (x-2)
                self.l_pipe(col, 7 + ROW_OFFSET, 'intro')
            elif sub == 0x01:                # bandera
                for j in range(0, 10):
                    self.put(col, j, 'F', False)
                self.put(col, 10, 'S', False)
                self.flag_col = col
            elif sub == 0x02:                # hacha
                self.entities.append({"type": "axe", "col": col, "row": 6 + ROW_OFFSET})
            elif sub == 0x03:                # cadena del puente (ChainObj, fila 7)
                self.entities.append({"type": "bridge_chain", "col": col, "row": 7 + ROW_OFFSET})
            elif sub == 0x04:                # puente de Bowser
                self.put(col, 8, 'b', False)
            elif sub == 0x08:
                self.entities.append({"type": "cheep_jump", "col": col, "row": 13})
            elif sub == 0x09:
                self.entities.append({"type": "frenzy", "col": col, "row": 13})
            elif sub == 0x0A:
                self.entities.append({"type": "stop_frenzy", "col": col, "row": 13})
            return
        if r == 0x0F:
            if k == 2 and off == 0:          # castillo
                kind = "castle_small" if L >= 6 else "castle_big"
                # El alto tiene la base de 9 tiles: 2 de cada lado del objeto
                # (dx: se aplica al final, asi no queda afuera del area el que
                # esta al principio de los x-1).
                self.decorations.append({"type": kind, "col": col, "row": L + ROW_OFFSET,
                                         "dx": -2 if kind == "castle_big" else 0})
            elif k == 3:                     # escalera
                ctrl = 8 - off
                if 0 <= ctrl <= 8:
                    self.under(col, self.stair_r[ctrl], 'S', self.stair_h[ctrl])
            elif k == 4 and off == 0:        # tuberia de salida en L
                self.l_pipe(col, L - 3 + ROW_OFFSET, 'exit')

    def render_special(self, col, r, L):
        t = self.type
        if L == 0x0:
            self.put(col, r, '?')
            self.block_item(col, r, "powerup")
        elif L == 0x1:
            self.put(col, r, '?')
        elif L == 0x2:
            self.put(col, r, 'H')
            self.block_item(col, r, "coin")
        elif L == 0x3:
            self.put(col, r, 'H' if t == GROUND else 'D')
            self.block_item(col, r, "one_up")
        elif L in (0x4, 0x5, 0x6, 0x7, 0x8):
            item = {4: "powerup", 5: "vine", 6: "star", 7: "coin", 8: "one_up"}[L]
            self.put(col, r, 'D')
            self.block_item(col, r, item, 10 if L == 7 else None)
            if L == 0x5:
                self.vines.append((col, r + ROW_OFFSET))
        elif L == 0x9:                       # boca de tuberia de costado (agua)
            self.l_pipe(col, r - 2 + ROW_OFFSET, 'water')
        elif L == 0xA:
            self.put(col, r, 'U')
        elif L == 0xB:
            self.entities.append({"type": "spring", "col": col, "row": r + 1 + ROW_OFFSET})

    def l_pipe(self, col, top, kind):
        """Tuberia en L, con tiles: la horizontal ('Y', boca de 2x2 abajo a la
        izquierda) y el tramo vertical ('T') a la derecha (en la de salida
        sigue hasta arriba de todo). El juego dibuja el codo donde se juntan."""
        for c in (col, col + 1):
            for rr in (top + 2, top + 3):
                if 0 <= c < self.width and 0 <= rr < ROWS:
                    self.cols[c][rr] = 'Y'
        for c in (col + 2, col + 3):
            if not 0 <= c < self.width:
                continue
            for rr in range(top, top + 4):
                if 0 <= rr < ROWS:
                    self.cols[c][rr] = 'T'
            if kind == 'exit':
                for rr in range(ROW_OFFSET, top):
                    self.cols[c][rr] = 'T'
        self.pipes.append((col, top + 2, 'side'))

    def make_scenery(self):
        """Agrupa las piezas de escenografia de fondo en nuestros decorados."""
        if self.type != GROUND:
            return
        pieces = []
        for col in range(self.width):
            bg = self.scenery_cols[col]
            v = BACK_SCENERY[bg - 1][(col // 16) % 3][col % 16] if bg else 0
            pieces.append((v >> 4, v & 0xf) if v else None)
        col = 0
        while col < self.width:
            p = pieces[col]
            if p is None:
                col += 1
                continue
            group = piece_group(p[1])
            if group == 'other':
                # Cerco, arbol alto y arbol bajo: una pieza por columna.
                name = {10: "fence", 11: "tree_tall", 12: "tree_short"}.get(p[1])
                if name:
                    self.decorations.append({"type": name, "col": col, "row": p[0] + ROW_OFFSET})
                col += 1
                continue
            end = col
            rows = [p[0]]
            while end + 1 < self.width and pieces[end + 1] is not None:
                k2 = pieces[end + 1][1]
                if piece_group(k2) != group or k2 in (1, 4) or (k2 == 7 and pieces[end][1] == 9):
                    break
                end += 1
                rows.append(pieces[end][0])
            width = end - col + 1
            top = min(rows) + ROW_OFFSET
            if group == 'cloud':
                self.decorations.append({"type": "cloud_big" if width >= 4 else "cloud_small", "col": col, "row": top})
            elif group == 'bush':
                self.decorations.append({"type": "bush_big" if width >= 4 else "bush_small", "col": col, "row": top})
            elif group == 'hill':
                self.decorations.append({"type": "hill_big" if width >= 5 else "hill_small", "col": col, "row": top})
            col = end + 1
        # En el original el castillo se dibuja encima de la escenografia y la
        # tapa; aca los dos son decorados, asi que se saca lo que quede debajo.
        castles = [d for d in self.decorations if d["type"].startswith("castle")]
        def hidden(d):
            if d["type"].startswith("castle") or d["type"] == "pipe_side":
                return False
            w = 4 if d["type"].endswith("big") else 2
            return any(c["col"] + c["dx"] - w < d["col"] < c["col"] + c["dx"] + (9 if c["type"] == "castle_big" else 5)
                       and d["row"] + 1 >= c["row"] for c in castles)
        self.decorations = [d for d in self.decorations if not hidden(d)]
        # La muralla pisa la escenografia de las filas 5-10 (se dibuja despues
        # en el original) y queda detras de castillos y demas decorados.
        if self.wall_cols:
            walls = set(self.wall_cols)
            widths = {"fence": 1, "tree_short": 1, "tree_tall": 1}
            def under_wall(d):
                if d["type"].startswith("castle") or d["type"] == "pipe_side":
                    return False
                if d["type"].startswith("cloud") and d["row"] + 1 < 5 + ROW_OFFSET:
                    return False   # nube alta, por encima de la muralla
                w = widths.get(d["type"], 5 if d["type"].endswith("big") else 3)
                return any(c in walls for c in range(d["col"], d["col"] + w))
            self.decorations = [d for d in self.decorations if not under_wall(d)]
            self.decorations[:0] = [{"type": "castle_wall", "col": c, "row": 5 + ROW_OFFSET} for c in self.wall_cols]

    # -- enemigos
    def place_enemies(self):
        for e in self.enemies_raw:
            if e.row == 0x0F:   # aparece debajo de la pantalla: no se ve nunca
                continue
            first = len(self.entities)
            self.add_enemy(e.ident, e.col, e.row)
            # Los del modo dificil: en un nivel normal aparecen solo en la
            # segunda vuelta (PrimaryHardMode activa el modo dificil).
            if e.hard_only and not self.hard:
                for ent in self.entities[first:]:
                    ent["hard_only"] = True
        # El original pone a los enemigos 8 px mas abajo de su fila
        # (CheckpointEnemyID); los que se ubican por el borde de arriba lo
        # llevan aparte (los demas apoyan en el piso igual).
        for ent in self.entities:
            if ent["type"] in ("blooper", "cheep", "lakitu", "bullet_bill"):
                ent["y_offset"] = 8

    def add_enemy(self, i, col, r):
        feet = r + 2           # fila donde apoya (nuestra convencion)
        top = r + 1            # fila del borde de arriba
        ent = None
        if i in (0x00, 0x01, 0x03):
            ent = {"type": "koopa", "col": col, "row": feet}
            if i != 0x00:
                ent["color"] = "red"
        elif i == 0x02:
            ent = {"type": "buzzy", "col": col, "row": feet}
        elif i == 0x05:
            ent = {"type": "hammer_bro", "col": col, "row": feet}
        elif i == 0x06:
            ent = {"type": "goomba", "col": col, "row": feet}
        elif i == 0x07:
            ent = {"type": "blooper", "col": col, "row": top}
        elif i == 0x08:
            ent = {"type": "bullet_bill", "col": col, "row": top}
        elif i == 0x09:
            ent = {"type": "koopa", "col": col, "row": feet, "state": "flying"}
        elif i == 0x10:
            # Paratroopa verde que vuela de un lado a otro (7-3).
            ent = {"type": "koopa", "col": col, "row": feet, "state": "flying_h"}
        elif i in (0x0A, 0x0B):
            ent = {"type": "cheep", "col": col, "row": top, "color": "red" if i == 0x0B else "gray"}
        elif i == 0x0C:
            ent = {"type": "podoboo", "col": col, "row": 13}
        elif i == 0x0E:
            ent = {"type": "koopa", "col": col, "row": feet, "state": "jumping"}
        elif i == 0x0F:
            ent = {"type": "koopa", "col": col, "row": feet, "color": "red", "state": "flying"}
        elif i == 0x11:
            ent = {"type": "lakitu", "col": col, "row": top}
        elif i == 0x14:
            ent = {"type": "cheep_jump", "col": col, "row": 13}
        elif i == 0x15:
            ent = {"type": "flame_frenzy", "col": col, "row": 13}
        elif i == 0x17:
            ent = {"type": "frenzy", "col": col, "row": 13}
        elif i == 0x18:
            ent = {"type": "stop_frenzy", "col": col, "row": 13}
        elif 0x1B <= i <= 0x1F:
            speeds = {0x1B: 105, 0x1C: 148, 0x1D: -105, 0x1E: -148, 0x1F: 105}
            ent = {"type": "firebar", "col": col, "row": top, "length": 12 if i == 0x1F else 6, "speed": speeds[i]}
        elif 0x24 <= i <= 0x2C:
            modes = {0x24: ("vertical", 3), 0x25: ("vertical", 3), 0x26: ("lift_up", 3), 0x27: ("lift_down", 3),
                     0x28: ("horizontal", 3), 0x29: ("fall", 3), 0x2A: ("horizontal", 3),
                     0x2B: ("lift_up", 2), 0x2C: ("lift_down", 2)}
            mode, width = modes[i]
            # Las plataformas grandes (6 sprites) en los castillos y en el modo
            # dificil secundario (desde el 5-3) son de 4 sprites: 2 bloques
            # (SPBBox / DrawLargePlatform del original).
            if width == 3 and (self.hard or self.type == CASTLE):
                width = 2
            p = {"col": col, "row": top, "width": width, "mode": mode}
            if i == 0x24:
                # Balanza (van de a dos seguidas): colgada de la polea de la
                # fila 0 del original.
                p.update({"mode": "balance", "rope_row": ROW_OFFSET})
            if mode in ("vertical", "horizontal"):
                p.update({"range": 4, "period": 4})
            elif mode.startswith("lift"):
                p["speed"] = 60
            self.platforms.append(p)
        elif i == 0x2D:
            ent = {"type": "bowser", "col": col, "row": feet}
        elif i == 0x35:
            # Toad / princesa al final del castillo (Y fija 0xB8: parado en
            # la fila 14 nuestra).
            ent = {"type": "retainer", "col": col, "row": 13}
        elif 0x37 <= i <= 0x3E:
            d = i - 0x37
            kind = "koopa" if d & 4 else "goomba"
            row = (0x70 // 16 + 2) if d & 2 else (0xb0 // 16 + 2)
            for k in range(3 if d & 1 else 2):
                self.entities.append({"type": kind, "col": col, "row": row, "x_offset": 24 * k})
        if ent is not None:
            self.entities.append(ent)


# ---------------------------------------------------------------- niveles
def level_groups(rom, world):
    """Areas de cada nivel del mundo: (area de entrada o None, area principal)."""
    ptrs = rom.area_pointers(world)
    levels = []
    i = 0
    while i < len(ptrs):
        if has_intro_pipe(parse_area(rom, ptrs[i])[1]) and i + 1 < len(ptrs):
            levels.append((ptrs[i], ptrs[i + 1]))
            i += 2
        else:
            levels.append((None, ptrs[i]))
            i += 1
    return levels


def build_level(rom, world, level):
    intro_ptr, main_ptr = level_groups(rom, world)[level]
    hard = world >= 5 or (world == 4 and level >= 2)
    no_piranhas = world == 0 and level == 0
    order = []
    areas = {}
    entrance_pages = {}

    def add(ptr):
        ptr &= 0x7f
        if ptr not in areas:
            areas[ptr] = AreaBuild(rom, ptr, no_piranhas, hard)
            order.append(ptr)
        return areas[ptr]

    if intro_ptr is not None:
        add(intro_ptr)
    main = add(main_ptr)
    crop = {}
    # Cuartos bonus de una pagina: que paginas usa este nivel (si son varias
    # del mismo area, cada una queda como un area aparte).
    room_pages = {}
    main_key = main_ptr & 0x7f
    intro_key = intro_ptr & 0x7f if intro_ptr is not None else None

    def crop_for(a, page):
        """Parte del area que usa este nivel: desde la pagina de entrada; los
        cuartos bonus compartidos entre mundos (cada mundo usa una pagina y
        se entra cayendo desde arriba) solo esa pagina."""
        start = page * 16
        shared = len({ch.world for ch in a.changes}) > 1
        # (el area de agua que comparten el 5-2 y el 6-2 se usa entera).
        one_page = shared and a.type == UNDERGROUND and a.header["entrance"] in (0, 1)
        # (17: la tuberia de salida del cuarto ocupa tambien la primera
        # columna de la pagina siguiente).
        end = start + 17 if one_page else a.width
        if one_page:
            room_pages.setdefault(a.pointer, set()).add(page)
        return start, min(end, a.width)

    def pipe_change(a, col):
        """El cambio de area que vale para una tuberia en esa columna: el
        original lee los cambios cuando entran 3 columnas mas alla del borde
        derecho de la pantalla, que con el jugador parado en la tuberia queda
        unas 12 columnas adelante."""
        options = [ch for ch in a.changes if ch.world == world and ch.col <= col + 12]
        if not options:
            return None
        return max(options, key=lambda ch: ch.col)

    crop[main_key] = (0, main.width)
    if intro_key is not None:
        # La escena de entrada: la pantalla del original (16 columnas), con
        # piso de relleno a los dos lados (pad) para que quede centrada y
        # llene tambien la pantalla ancha.
        crop[intro_key] = (0, 16)
    i = 0
    while i < len(order):
        ptr = order[i]
        a = areas[ptr]
        start, end = crop[ptr]
        for col, row, kind in a.pipes:
            if not start <= col < end or ptr == intro_key:
                continue
            ch = pipe_change(a, col)
            if ch is None or a.header["cloud"]:
                continue
            dest_key = ch.pointer & 0x7f
            if dest_key not in areas:
                dest = add(dest_key)
                crop[dest_key] = crop_for(dest, ch.page)
            elif dest_key not in (main_key, intro_key):
                s0, e0 = crop[dest_key]
                s1, e1 = crop_for(areas[dest_key], ch.page)
                crop[dest_key] = (min(s0, s1), max(e0, e1))
        for col, row in a.vines:
            if not start <= col < end:
                continue
            ch = pipe_change(a, col)
            if ch is None:
                continue
            dest_key = ch.pointer & 0x7f
            if dest_key not in areas:
                dest = add(dest_key)
                crop[dest_key] = (ch.page * 16, dest.width)
        i += 1
    # Disposicion: una al lado de la otra con una columna solida en el medio.
    # La escena de entrada lleva INTRO_PAD columnas de piso a cada lado.
    INTRO_PAD = 6
    pad = {ptr: (INTRO_PAD if ptr == intro_key else 0) for ptr in order}
    offsets = {}
    x = 0
    for ptr in order:
        offsets[ptr] = x
        x += (crop[ptr][1] - crop[ptr][0]) + 2 * pad[ptr] + 1
    width = x - 1
    grid = [['.'] * width for _ in range(ROWS)]
    out = {"blocks": [], "warps": [], "entities": [], "decorations": [], "platforms": [], "areas": []}

    def g(ptr, col):
        """Columna local de un area -> columna del nivel (None si quedo afuera)."""
        start, end = crop[ptr]
        if not start <= col < end:
            return None
        return offsets[ptr] + pad[ptr] + col - start

    for ptr in order:
        a = areas[ptr]
        start, end = crop[ptr]
        span = end - start + 2 * pad[ptr]
        for c in range(start, end):
            for r in range(ROWS):
                grid[r][g(ptr, c)] = a.cols[c][r]
        # Relleno: el piso de la ultima columna, sin nada arriba.
        for k in range(pad[ptr]):
            for side in (offsets[ptr] + k, offsets[ptr] + span - 1 - k):
                for r in range(ROWS):
                    grid[r][side] = a.cols[end - 1][r] if a.cols[end - 1][r] == '#' else '.'
        if offsets[ptr] + span < width:
            for r in range(ROWS):
                grid[r][offsets[ptr] + span] = '#'
        area_out = {"start": offsets[ptr], "end": offsets[ptr] + span - 1, "world": WORLD_NAMES[a.type]}
        # Colores (BackgroundColorCtrl): del encabezado o del primer cambio de
        # atributos del area (siempre al principio, en la columna 6).
        bg = a.header["bg_color"]
        changes = [o.data1 & 7 for o in a.objects if o.row == 0x0E and (o.data1 & 0x40) and (o.data1 & 7) >= 4]
        if not bg and changes:
            bg = changes[0]
        if bg:
            area_out["palette"] = {4: "night", 5: "snow", 6: "night_snow", 7: "gray"}[bg]
        elif a.type == GROUND and a.header["style"] == 1:
            area_out["palette"] = "mushroom"   # paleta 0 de hongos: caños naranjas
        # Nubes: caerse por abajo vuelve adonde dice su cambio de area,
        # cayendo desde arriba de la pantalla.
        if a.header["cloud"]:
            area_out["cloud"] = True
            back = [ch for ch in a.changes if ch.world == world]
            if back and (back[0].pointer & 0x7f) in crop:
                bp = back[0].pointer & 0x7f
                fc = g(bp, back[0].page * 16 + 2)
                if fc is not None:
                    area_out["fall_col"] = fc
                    area_out["fall_row"] = 2
        pages = sorted(room_pages.get(ptr, ()))
        if len(pages) > 1:
            # Una camara por cuarto: la pagina de cada uno, sin las del medio.
            for pg in pages:
                out["areas"].append(dict(area_out, start=g(ptr, pg * 16), end=g(ptr, pg * 16 + 16)))
        else:
            out["areas"].append(area_out)
        for col, row, item, hits in a.blocks:
            gc = g(ptr, col)
            if gc is not None:
                b = {"col": gc, "row": row, "item": item}
                if hits:
                    b["hits"] = hits
                if item == "vine":
                    ch = pipe_change(a, col)
                    if ch is not None and (ch.pointer & 0x7f) in crop:
                        dp = ch.pointer & 0x7f
                        dest = areas[dp]
                        vc = ch.page * 16 + 2
                        feet = None
                        for rr in range(ROW_OFFSET, ROWS):
                            if any(0 <= c < dest.width and dest.cols[c][rr] not in '.oWw' for c in range(vc, vc + 6)):
                                feet = rr
                                break
                        # La enredadera sube por el hueco del piso (si hay).
                        if feet is not None:
                            gaps = [c for c in range(vc, vc + 8) if 0 <= c < dest.width and dest.cols[c][feet] == '.']
                            if gaps:
                                vc = gaps[0]
                        if g(dp, vc) is not None:
                            b["dest_col"] = g(dp, vc)
                            b["dest_row"] = feet if feet is not None else 12
                out["blocks"].append(b)
        for d in a.decorations:
            gc = g(ptr, d["col"])
            if gc is not None:
                dec = dict(d, col=gc + d.get("dx", 0))
                dec.pop("dx", None)
                out["decorations"].append(dec)
        for p in a.platforms:
            gc = g(ptr, p["col"])
            if gc is not None:
                out["platforms"].append(dict(p, col=gc))
        # Frenesis (Cheep Cheep saltarinas, Bill Bala, llamas): zona hasta el
        # proximo "stop" o el final del area.
        stops = sorted(e["col"] for e in a.entities if e["type"] == "stop_frenzy")
        for e in a.entities:
            if e["type"] == "stop_frenzy":
                continue
            gc = g(ptr, e["col"])
            if gc is None:
                continue
            ent = dict(e, col=gc)
            if e["type"] in ("cheep_jump", "frenzy", "flame_frenzy"):
                nxt = [s for s in stops if s > e["col"]]
                endc = g(ptr, min(nxt[0], end - 1)) if nxt else offsets[ptr] + end - start - 1
                ent["end_col"] = endc
                if e["type"] == "frenzy":
                    ent["type"] = "cheep_frenzy" if a.type == WATER else "bullet_frenzy"
            if e["type"] == "lakitu":
                flag = a.flag_col if a.flag_col is not None else end
                ent["end_col"] = g(ptr, max(e["col"], min(flag, end - 1) - 8))
            out["entities"].append(ent)
    # Laberintos (objeto D-0B "loop" + tabla de chequeos por mundo y pagina):
    # el original los procesa al empezar cada pagina despues de un loop; el
    # jugador tiene que estar parado a la altura de la tabla o vuelve 4
    # paginas. Altura: Player_Y + 32 es el pie en pantalla NES, +16 en la
    # nuestra; 0xf0 no se alcanza nunca.
    table = [t for t in rom.loop_table() if t[0] == world]
    for ptr in order:
        a = areas[ptr]
        loops = sorted(o.col for o in a.objects if o.row == 0x0D and (o.data1 & 0x40) and (o.data1 & 0x3f) == 0x0B)
        pending = False
        for page in range(1, a.width // 16 + 1):
            if any(page * 16 - 16 <= c < page * 16 for c in loops):
                pending = True
            if not pending:
                continue
            entry = [t for t in table if t[1] == page]
            if not entry:
                continue
            pending = False
            y = entry[0][2]
            gc = g(ptr, page * 16)
            if gc is None:
                continue
            # Lo que se repite: los datos vuelven al objeto de la tabla de
            # offsets y se dibujan 4 paginas atras, o sea la seccion queda
            # corrida (pagina del chequeo - pagina de ese objeto) paginas. Si
            # no entra la pantalla atras, se acorta de a una pagina.
            c0 = rom.column_at_offset(ptr, entry[0][3])
            back = page * 16 - (c0 // 16) * 16
            while back > page * 16 - 24 and back > 16:
                back -= 16
            out["entities"].append({"type": "loop", "col": gc, "row": (y + 48) // 16 if y < 0xe0 else -1,
                                    "length": 3 if world == 6 else 1, "back": back})
    # Warp zones (objeto D-05): las tuberias que siguen llevan a otros mundos
    # (tabla del original: 1-2 -> 4,3,2; bajo tierra -> solo la del medio a 5;
    # en superficie -> 8,7,6).
    warp_pipes = {}
    for ptr in order:
        a = areas[ptr]
        zone = [o.col for o in a.objects if o.row == 0x0D and (o.data1 & 0x40) and (o.data1 & 0x3f) == 0x05]
        if not zone:
            continue
        # El objeto (ScrollLockObject_Warp) esta despues de las tuberias: se
        # activa cuando el borde derecho de la pantalla llega ahi, con ellas
        # en pantalla. Son las de las 24 columnas anteriores.
        near = sorted([p for p in a.pipes if p[2] == 'vertical' and zone[0] - 24 <= p[0] < zone[0]])[-3:]
        if world == 0:
            targets = [4, 3, 2]
        elif a.type == GROUND:
            targets = [8, 7, 6]
        else:
            targets = [5] if len(near) == 1 else [0, 5, 0]
        for k, p in enumerate(near):
            warp_pipes[(ptr, p[0])] = targets[k]
        gz = g(ptr, zone[0])
        if gz is not None and near:
            first = g(ptr, near[0][0])
            out["entities"].append({"type": "warp_zone", "col": gz, "row": 13,
                                    "end_col": first if first is not None else gz - 24})
    # Tuberias que llevan a otra area.
    for ptr in order:
        a = areas[ptr]
        for col, row, kind in a.pipes:
            gc = g(ptr, col)
            if gc is None:
                continue
            if (ptr, col) in warp_pipes:
                if warp_pipes[(ptr, col)]:
                    out["warps"].append({"col": gc, "row": row, "dest_col": 2, "dest_row": 12,
                                         "level": f"{warp_pipes[(ptr, col)]}-1"})
                continue
            dest_ptr, dest_page = None, 0
            if intro_ptr is not None and ptr == intro_ptr & 0x7f:
                dest_ptr, dest_page = main_ptr & 0x7f, 0
            else:
                best = pipe_change(a, col)
                if best is None:
                    continue
                dest_ptr, dest_page = best.pointer & 0x7f, best.page
            if dest_ptr not in areas:
                continue
            dest = areas[dest_ptr]
            dcol_local = dest_page * 16 + 2
            drow = (ENTRANCE_Y[dest.header["entrance"]] + 32) // 16
            # En el agua, abajo de la superficie (arriba de ella esta el
            # techo invisible que no deja salir nadando).
            if dest.type == WATER:
                drow = max(drow, ROW_OFFSET + 2)
            # Si se sale por una tuberia (vertical) cerca de la entrada, se
            # aparece arriba de ella (el juego la detecta y lo hace salir).
            pipe_tops = [(c, rr) for c in range(dest_page * 16, dest_page * 16 + 8)
                         for rr in range(ROWS) if 0 <= c < dest.width and dest.cols[c][rr] == 'T'
                         and (rr == 0 or dest.cols[c][rr - 1] != 'T') and c + 1 < dest.width
                         and dest.cols[c + 1][rr] == 'T']
            if pipe_tops and not (intro_ptr is not None and ptr == intro_ptr & 0x7f):
                pc, pr = pipe_tops[0]
                dcol_local, drow = pc, pr - 2
            dgc = g(dest_ptr, dcol_local)
            if dgc is None:
                continue
            w = {"col": gc, "row": row, "dest_col": dgc, "dest_row": drow}
            if kind == 'side':
                w["dir"] = "right"
            out["warps"].append(w)
    # Checkpoint (media pagina del original) y tiempo.
    nyb = rom.byte(HALFWAY_PAGE_NYBBLES + world * 2 + (1 if level & 2 else 0))
    half = (nyb >> 4) if (level & 1) == 0 else (nyb & 0x0f)
    checkpoints = [offsets[main_ptr & 0x7f] + half * 16] if half else []
    level_json = {
        "tile_size": 16,
        "name": f"{world + 1}-{level + 1}",
        "time": TIMER[main.header["timer"]],
        "world": WORLD_NAMES[main.type],
    }
    if intro_ptr is not None:
        level_json["intro"] = {"area": 0}
        level_json["start_col"] = INTRO_PAD + 2
    if checkpoints:
        level_json["checkpoints"] = checkpoints
    level_json.update({
        "areas": out["areas"],
        "blocks": out["blocks"],
        "warps": out["warps"],
        "entities": sorted(out["entities"], key=lambda e: e["col"]),
        "platforms": out["platforms"],
        "decorations": out["decorations"],
        "rows": [''.join(r) for r in grid],
    })
    return level_json


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        print(__doc__)
        sys.exit(1)
    rom = Rom(args[0])
    out_dir = args[1] if len(args) > 1 else "levels"
    dump = None
    if "--dump" in sys.argv:
        dump = sys.argv[sys.argv.index("--dump") + 1]
    for world in range(8):
        for level in range(4):
            name = f"{world + 1}-{level + 1}"
            if dump and dump != name:
                continue
            lv = build_level(rom, world, level)
            if dump:
                for r in lv["rows"]:
                    print(r)
                continue
            path = os.path.join(out_dir, f"world{world + 1}", f"{name}.json")
            # Los niveles retocados con el editor (marca "edited") no se pisan,
            # salvo con --force.
            if os.path.exists(path) and "--force" not in sys.argv:
                try:
                    with open(path, encoding="utf-8") as f:
                        if json.load(f).get("edited"):
                            print(f"{path}: editado a mano, no se regenera (--force para pisarlo)")
                            continue
                except (OSError, ValueError):
                    pass
            os.makedirs(os.path.dirname(path), exist_ok=True)
            text = json.dumps(lv, indent=2)
            # Objetos chicos en una sola linea, como los demas niveles.
            text = re.sub(r'\{\n\s+("(?:col|start|type|area)"[^}]*?)\n\s+\}',
                          lambda m: '{' + re.sub(r',\n\s+', ', ', m.group(1)) + '}', text)
            with open(path, "w", newline="\n") as f:
                f.write(text + "\n")
            print(f"{path}: {len(lv['rows'][0])} columnas, {len(lv['entities'])} entidades, {len(lv['warps'])} tuberias")


if __name__ == "__main__":
    main()
