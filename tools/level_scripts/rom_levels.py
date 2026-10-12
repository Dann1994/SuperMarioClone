"""Lee los datos de nivel de la ROM original (solo se lee como referencia
de "como estan armados"; los graficos y el motor son los nuestros).

La ROM NO va en el repositorio (.gitignore): cada uno usa su propia copia.
El formato de los datos de area y de enemigos sale del estudio de la
descompilacion SMB-Vanilla (area.c: ProcessAreaData/decode_area_data,
common.c: ProcLoopCommand/HandleGroupEnemies); no se copio codigo.

Uso, parado en la raiz del proyecto:
  python tools/level_scripts/rom_build.py "Documentaciones/Super Mario Bros (E).nes" [salida]
(salida por defecto: levels/). Con --dump W-L imprime el nivel en texto.
"""
import json
import os
import re
import sys

# ---------------------------------------------------------------- ROM
PRG_BASE = 0x8000
WORLD_ADDR_OFFSETS = 0x9CB4      # 8 bytes
AREA_ADDR_OFFSETS = 0x9CBC       # 0x24 bytes
ENEMY_ADDR_H_OFFSETS = 0x9CE0    # 4
ENEMY_DATA_ADDR_LOW = 0x9CE4     # 34
ENEMY_DATA_ADDR_HIGH = 0x9D06    # 34
AREA_DATA_H_OFFSETS = 0x9D28     # 4
AREA_DATA_ADDR_LOW = 0x9D2C      # 34
AREA_DATA_ADDR_HIGH = 0x9D4E     # 34
HALFWAY_PAGE_NYBBLES = 0x91BD    # 16
STAIRCASE_HEIGHT = 0x9AA5        # 9
STAIRCASE_ROW = 0x9AAE           # 9

WATER, GROUND, UNDERGROUND, CASTLE = 0, 1, 2, 3
WORLD_NAMES = {WATER: "underwater", GROUND: "overworld", UNDERGROUND: "underground", CASTLE: "castle"}

# Filas de metatile del original (0..12) -> nuestras filas (0..15): la fila
# 0 del original esta en Y=32 de pantalla, que en nuestro mapa es la fila 3.
ROW_OFFSET = 3
ROWS = 16


class Rom:
    def __init__(self, path):
        data = open(path, 'rb').read()
        if data[:4] != b'NES\x1a':
            raise ValueError("no es una ROM de NES")
        self.prg = data[16:16 + 0x8000]

    def byte(self, addr):
        return self.prg[addr - PRG_BASE]

    def bytes(self, addr, n):
        return self.prg[addr - PRG_BASE:addr - PRG_BASE + n]

    def area_pointers(self, world):
        start = self.byte(WORLD_ADDR_OFFSETS + world)
        end = self.byte(WORLD_ADDR_OFFSETS + world + 1) if world < 7 else 0x24
        return [self.byte(AREA_ADDR_OFFSETS + i) for i in range(start, end)]

    def area_data_addr(self, pointer):
        area_type = (pointer & 0x60) >> 5
        offset = self.byte(AREA_DATA_H_OFFSETS + area_type) + (pointer & 0x1f)
        return self.byte(AREA_DATA_ADDR_LOW + offset) | (self.byte(AREA_DATA_ADDR_HIGH + offset) << 8)

    def loop_table(self):
        """Chequeos de los laberintos (LoopCmdWorldNumber/PageNumber/
        YPosition, 11 entradas cada una). Cambian de lugar entre versiones
        de la ROM, asi que se buscan por los mundos (4-4, 7-4 y 8-4)."""
        i = self.prg.find(bytes([3, 3, 6, 6, 6, 6, 6, 6, 7, 7, 7]))
        if i < 0:
            return []
        t = self.prg[i:i + 33]
        # AreaDataOfsLoopback: hasta donde se rebobinan los datos del area.
        j = self.prg.find(bytes([0x12, 0x36, 0x0e, 0x0e, 0x0e, 0x32, 0x32, 0x32, 0x0a, 0x26, 0x40]))
        ofs = self.prg[j:j + 11] if j >= 0 else bytes(11)
        return [(t[k], t[11 + k], t[22 + k], ofs[k]) for k in range(11)]

    def column_at_offset(self, pointer, offset):
        """Columna del objeto que esta en ese offset de los datos del area."""
        addr = self.area_data_addr(pointer)
        page, i = 0, addr + 2
        while self.byte(i) != 0xFD and i - addr <= offset:
            b0, b1 = self.byte(i), self.byte(i + 1)
            if b1 & 0x80:
                page += 1
            if (b0 & 0x0f) == 0x0D and not (b1 & 0x40):
                page = b1 & 0x1f
            if i - addr == offset:
                return page * 16 + (b0 >> 4)
            i += 2
        return 0

    def enemy_data_addr(self, pointer):
        area_type = (pointer & 0x60) >> 5
        offset = self.byte(ENEMY_ADDR_H_OFFSETS + area_type) + (pointer & 0x1f)
        return self.byte(ENEMY_DATA_ADDR_LOW + offset) | (self.byte(ENEMY_DATA_ADDR_HIGH + offset) << 8)


# ---------------------------------------------------------------- datos crudos
class AreaObject:
    """Un objeto de los datos de area, ya con su columna absoluta."""

    def __init__(self, col, row, data1):
        self.col = col      # columna absoluta en el area
        self.row = row      # nibble bajo del primer byte (0..15)
        self.data1 = data1  # segundo byte sin el bit de pagina
        self.length = data1 & 0x0f
        self.kind = (data1 >> 4) & 7


def parse_area(rom, pointer):
    """Encabezado + lista de objetos (con columna absoluta)."""
    addr = rom.area_data_addr(pointer)
    h0, h1 = rom.byte(addr), rom.byte(addr + 1)
    header = {
        "timer": h0 >> 6,
        "entrance": (h0 >> 3) & 7,
        "foreground": (h0 & 7) if (h0 & 7) < 4 else 0,
        "bg_color": (h0 & 7) if (h0 & 7) >= 4 else 0,
        "style": (h1 >> 6) if (h1 >> 6) != 3 else 0,
        "cloud": (h1 >> 6) == 3,
        "background": (h1 >> 4) & 3,
        "terrain": h1 & 0x0f,
        "type": (pointer & 0x60) >> 5,
    }
    objects = []
    page = 0
    i = addr + 2
    while True:
        b0 = rom.byte(i)
        if b0 == 0xFD:
            break
        b1 = rom.byte(i + 1)
        if b1 & 0x80:
            page += 1
        row = b0 & 0x0f
        # Salto de pagina: fila D con el bit 6 apagado.
        if row == 0x0D and (b1 & 0x40) == 0:
            page = b1 & 0x1f
        else:
            objects.append(AreaObject(page * 16 + (b0 >> 4), row, b1 & 0x7f))
        i += 2
    return header, objects


class EnemyEntry:
    def __init__(self, col, row, ident, hard_only):
        self.col = col
        self.row = row
        self.ident = ident
        self.hard_only = hard_only


class AreaChange:
    """Comando de 3 bytes: los caños de esta zona llevan a otra area."""

    def __init__(self, col, pointer, world, page):
        self.col = col
        self.pointer = pointer
        self.world = world
        self.page = page


def parse_enemies(rom, pointer):
    addr = rom.enemy_data_addr(pointer)
    enemies, changes = [], []
    page = 0
    # Igual que EnemyObjectPageSel: despues de fijar la pagina (fila F), el
    # bit de "pagina siguiente" de la entrada que sigue no vuelve a sumar.
    page_sel = False
    i = addr
    while True:
        b0 = rom.byte(i)
        if b0 == 0xFF:
            break
        b1 = rom.byte(i + 1)
        row = b0 & 0x0f
        if (b1 & 0x80) and not page_sel:
            page += 1
            page_sel = True
        if row == 0x0F and not (b1 & 0x80):
            page = b1 & 0x3f
            page_sel = True
            i += 2
            continue
        page_sel = False
        if row == 0x0E:
            b2 = rom.byte(i + 2)
            changes.append(AreaChange(page * 16 + (b0 >> 4), b1, b2 >> 5, b2 & 0x1f))
            i += 3
            continue
        enemies.append(EnemyEntry(page * 16 + (b0 >> 4), row, b1 & 0x3f, bool(b1 & 0x40)))
        i += 2
    return enemies, changes


def has_intro_pipe(objects):
    return any(o.row == 0x0D and (o.data1 & 0x40) and (o.data1 & 0x3f) == 0x00 for o in objects)
