"""Niveles de superficie de los mundos 2 a 8 (x-1, x-2 y x-3 que no son de
agua, puentes ni subterraneo; ver shared_levels.py para esos).

Cada nivel se describe con una lista de elementos por columna (ver
build_course): pozos, tuberias, bloques, escaleras, enemigos... Todo "a
ojo" siguiendo la idea de cada nivel del original, no tile por tile.
"""
from levelgen import (Level, ROWS, GROUND, FLOOR, LOW, HIGH, treetop, mushroom, cannon, opening, pyramid,
                      water)


def build_course(lv, items):
    """Aplica una lista de (tipo, columna, *args):
    pit c w | pipe c h | piranha c h | bricks c w row | qrow c w row |
    q c row item | dq c row item hits | stairs c h | stairs_down c h |
    pyramid c h gap | cannon c h [base] | block c w row | coins c w row |
    hidden c row item | goomba c... | koopa c [row color state] |
    buzzy c [row] | hammer c row | lakitu c end | ledge c w top (piso alto)
    """
    for item in items:
        kind, c, *a = item
        if kind == "pit":
            lv.pit(c, c + a[0] - 1)
        elif kind == "pipe":
            lv.pipe(c, a[0])
        elif kind == "piranha":
            lv.piranha(c, a[0])
        elif kind == "bricks":
            lv.row(c, c + a[0] - 1, a[1], 'B')
        elif kind == "qrow":
            lv.row(c, c + a[0] - 1, a[1], '?')
        elif kind == "q":
            lv.q(c, a[0], a[1] if len(a) > 1 else None)
        elif kind == "dq":
            lv.brick_item(c, a[0], a[1], a[2] if len(a) > 2 else None)
        elif kind == "stairs":
            lv.stairs_up(c, a[0])
        elif kind == "stairs_down":
            lv.stairs_down(c, a[0])
        elif kind == "pyramid":
            pyramid(lv, c, a[0], a[1] if len(a) > 1 else 2)
        elif kind == "cannon":
            cannon(lv, c, a[0], a[1] if len(a) > 1 else GROUND)
        elif kind == "block":
            lv.row(c, c + a[0] - 1, a[1], 'S')
        elif kind == "coins":
            lv.coins(c, c + a[0] - 1, a[1])
        elif kind == "hidden":
            lv.hidden(c, a[0], a[1] if len(a) > 1 else "one_up")
        elif kind == "goomba":
            for cc in [c] + list(a):
                lv.enemy("goomba", cc, FLOOR)
        elif kind == "goomba_at":
            lv.enemy("goomba", c, a[0])
        elif kind == "koopa":
            row = a[0] if len(a) > 0 else FLOOR
            lv.koopa(c, row, color=a[1] if len(a) > 1 else None, state=a[2] if len(a) > 2 else None)
        elif kind == "buzzy":
            lv.enemy("buzzy", c, a[0] if a else FLOOR)
        elif kind == "hammer":
            lv.enemy("hammer_bro", c, a[0] if a else FLOOR)
        elif kind == "lakitu":
            lv.enemy("lakitu", c, 3, end_col=a[0])
        elif kind == "ledge":
            lv.ground(c, c + a[0] - 1, top=a[1])
        else:
            raise ValueError(kind)


def overworld(name, width, flag, items, time=400, checkpoint=None, stairs=8, scenery=True):
    lv = Level(name, width, time=time)
    lv.ground(0, width - 1)
    build_course(lv, items)
    lv.flag_end(flag, stairs=stairs)
    if scenery:
        lv.overworld_scenery(0, flag - stairs - 3)
    lv.clouds(flag - stairs - 3, width)
    lv.checkpoints = [checkpoint or flag // 2]
    return lv


# ------------------------------------------------------------------ mundo 2
def gen_2_1(path):
    lv = overworld("2-1", 215, 204, [
        ("stairs", 8, 3), ("stairs_down", 11, 3),
        ("q", 18, LOW, "powerup"), ("bricks", 22, 5, LOW), ("q", 24, LOW), ("q", 24, HIGH),
        ("goomba", 26, 28),
        ("bricks", 32, 8, HIGH), ("dq", 35, HIGH, "star"), ("koopa", 36, HIGH - 1),
        ("piranha", 46, 3), ("goomba", 50, 52), ("piranha", 56, 4),
        ("pit", 62, 3),
        ("bricks", 68, 6, LOW), ("dq", 70, LOW, "coin", 10), ("q", 72, LOW, "powerup"),
        ("koopa", 76, FLOOR, None, "jumping"), ("koopa", 82, FLOOR, None, "jumping"),
        ("pit", 88, 3), ("goomba", 94, 96, 98),
        ("qrow", 102, 3, LOW), ("bricks", 106, 6, HIGH), ("hidden", 104, HIGH),
        ("pit", 114, 2), ("pyramid", 120, 4), ("goomba", 132, 134),
        ("piranha", 140, 2), ("koopa", 146), ("piranha", 152, 3),
        ("bricks", 158, 4, LOW), ("q", 160, LOW, "powerup"), ("goomba", 166, 168, 170),
        ("pit", 176, 2), ("koopa", 186),
    ], checkpoint=100)
    lv.save(path)


# ------------------------------------------------------------------ mundo 3
def gen_3_1(path):
    lv = overworld("3-1", 215, 204, [
        ("q", 14, LOW, "powerup"), ("bricks", 18, 4, LOW), ("goomba", 22),
        ("pit", 26, 3),
        ("piranha", 34, 3), ("koopa", 40, FLOOR, None, "jumping"), ("piranha", 46, 4),
        ("pit", 54, 2), ("pit", 60, 3),
        ("stairs", 66, 4), ("pit", 70, 3), ("stairs_down", 73, 4),
        ("bricks", 82, 8, LOW), ("dq", 85, LOW, "coin", 10), ("bricks", 82, 8, HIGH), ("q", 88, HIGH, "powerup"),
        ("goomba", 84, 86), ("koopa", 94, FLOOR, None, "jumping"),
        ("pit", 100, 3), ("piranha", 108, 2),
        ("koopa", 114), ("koopa", 118, FLOOR, None, "jumping"),
        # Los dos Hammer Bros en sus ladrillos.
        ("bricks", 128, 8, LOW), ("bricks", 130, 6, HIGH), ("hammer", 131, LOW - 1), ("hammer", 134, HIGH - 1),
        ("q", 140, LOW, "powerup"),
        ("pit", 146, 3), ("goomba", 152, 154),
        ("piranha", 160, 3), ("pit", 168, 2), ("koopa", 174, FLOOR, None, "jumping"),
        ("goomba", 186, 188),
    ], checkpoint=104)
    lv.save(path)


def gen_3_2(path):
    lv = overworld("3-2", 225, 214, [
        ("goomba", 18, 20, 22), ("q", 24, LOW, "powerup"),
        ("koopa", 30), ("goomba", 36, 38, 40),
        ("bricks", 44, 3, LOW), ("koopa", 52, FLOOR, None, "jumping"),
        ("goomba", 58, 60, 62), ("pit", 66, 2),
        ("koopa", 72), ("koopa", 75), ("goomba", 80, 82),
        ("q", 88, LOW, "star"), ("goomba", 92, 94, 96),
        ("piranha", 102, 2), ("koopa", 108, FLOOR, None, "jumping"), ("koopa", 114, FLOOR, None, "jumping"),
        ("goomba", 120, 122, 124), ("pit", 128, 3),
        ("bricks", 136, 5, LOW), ("q", 138, LOW, "powerup"), ("koopa", 144),
        ("goomba", 150, 152, 154), ("pit", 160, 2),
        ("koopa", 166, FLOOR, None, "jumping"), ("goomba", 172, 174), ("koopa", 182), ("koopa", 186),
        ("goomba", 192, 194),
    ], checkpoint=110)
    lv.save(path)


def treetop_course(name, path, trees, platforms, enemies, extras=(), time=300, width=175, end=150):
    lv = Level(name, width, time=time)
    lv.ground(0, 15)
    lv.overworld_scenery(0, 15)
    lv.clouds(16, end)
    for c, w, top in trees:
        treetop(lv, c, w, top)
        if w >= 5:
            lv.coins(c + 1, c + w - 2, top - 2)
    for p in platforms:
        lv.platform(*p[:4], **p[4])
    build_course(lv, enemies)
    build_course(lv, extras)
    lv.ground(end, width - 1)
    lv.stairs_up(end + 2, 4, width=5)
    lv.flag_end(end + 12, stairs=0)
    lv.checkpoints = [end // 2]
    lv.save(path)


def gen_3_3(path):
    treetop_course("3-3", path, [
        (18, 5, 11), (26, 4, 8), (34, 6, 10), (44, 3, 7), (50, 5, 10), (60, 4, 12),
        (78, 6, 9), (88, 3, 6), (96, 5, 10), (112, 4, 8), (120, 6, 11), (136, 5, 9),
    ], [
        (66, 10, 3, "vertical", {"range": 4, "period": 4}),
        (71, 7, 3, "vertical", {"range": 4, "period": 4}),
        (102, 9, 3, "horizontal", {"range": 5, "period": 4}),
        (128, 10, 3, "fall", {}),
        (143, 9, 3, "horizontal", {"range": 3, "period": 3}),
    ], [
        ("koopa", 28, 7, "red"), ("goomba_at", 36, 9), ("goomba_at", 38, 9),
        ("koopa", 80, 8, "red"), ("koopa", 98, 9, None, "jumping"), ("koopa", 122, 10, "red"),
        ("q", 46, 3, "powerup"),
    ])


# ------------------------------------------------------------------ mundo 4
def gen_4_1(path):
    lv = overworld("4-1", 230, 219, [
        ("lakitu", 16, 196),
        ("q", 20, LOW, "powerup"), ("bricks", 24, 4, LOW),
        ("pit", 30, 3), ("piranha", 38, 3),
        ("qrow", 46, 2, LOW), ("pit", 52, 2),
        ("piranha", 60, 2), ("pit", 66, 3),
        ("bricks", 74, 6, LOW), ("q", 76, LOW, "one_up"), ("piranha", 84, 4),
        ("pit", 92, 3), ("pit", 100, 2),
        ("stairs", 106, 3), ("pit", 109, 2), ("stairs_down", 111, 3),
        ("q", 120, LOW, "powerup"), ("piranha", 126, 3),
        ("pit", 134, 3), ("bricks", 142, 5, LOW),
        ("pit", 150, 2), ("piranha", 156, 2), ("pit", 164, 3),
        ("qrow", 172, 3, LOW), ("pit", 180, 2), ("piranha", 188, 3),
    ], checkpoint=112, time=400)
    lv.save(path)


def gen_4_3(path):
    lv = Level("4-3", 175, time=300)
    lv.ground(0, 15)
    lv.overworld_scenery(0, 15)
    lv.clouds(16, 150)
    for c, w, top in [(18, 5, 12), (26, 3, 9), (32, 7, 6), (44, 5, 11), (52, 3, 8), (60, 7, 10),
                      (80, 5, 7), (90, 3, 11), (98, 5, 9), (114, 7, 10), (126, 3, 7), (134, 5, 11)]:
        mushroom(lv, c, w, top)
        if w >= 5:
            lv.coins(c + 1, c + w - 2, top - 2)
    # Plataformas que suben y bajan (en el original son balanzas).
    lv.platform(70, 12, 3, "vertical", range=5, period=4)
    lv.platform(75, 7, 3, "vertical", range=5, period=4)
    lv.platform(106, 10, 3, "horizontal", range=4, period=4)
    lv.platform(142, 10, 3, "lift_up", speed=45)
    lv.q(34, 2, "powerup")
    build_course(lv, [
        ("koopa", 34, 5, "red"), ("koopa", 62, 9, "red"), ("koopa", 82, 6, "red"),
        ("koopa", 100, 8, None, "jumping"), ("koopa", 116, 9, "red"),
    ])
    lv.ground(148, 174)
    lv.stairs_up(150, 4, width=5)
    lv.flag_end(162, stairs=0)
    lv.checkpoints = [80]
    lv.save(path)


# ------------------------------------------------------------------ mundo 5
def gen_5_1(path):
    lv = overworld("5-1", 220, 209, [
        ("goomba", 16, 18), ("cannon", 26, 2), ("koopa", 32),
        ("goomba", 38, 40, 42), ("cannon", 48, 3),
        ("q", 54, LOW, "powerup"), ("koopa", 60, FLOOR, None, "jumping"), ("koopa", 64, FLOOR, None, "jumping"),
        ("piranha", 72, 3), ("goomba", 78, 80), ("cannon", 86, 2), ("goomba", 92, 94),
        ("pit", 100, 3), ("cannon", 108, 2), ("cannon", 108, 2, 12),
        ("koopa", 116), ("goomba", 122, 124, 126),
        ("piranha", 132, 4), ("koopa", 140, FLOOR, None, "jumping"),
        ("bricks", 146, 4, LOW), ("q", 148, LOW, "star"),
        ("pit", 156, 3), ("cannon", 166, 3), ("goomba", 172, 174), ("koopa", 186),
    ], checkpoint=104)
    lv.save(path)


def gen_5_2(path):
    lv = overworld("5-2", 225, 214, [
        ("stairs", 14, 4), ("stairs_down", 18, 4),
        ("goomba", 26, 28), ("q", 32, LOW, "powerup"),
        ("bricks", 40, 8, LOW), ("bricks", 42, 6, HIGH), ("hammer", 43, LOW - 1), ("hammer", 45, HIGH - 1),
        ("cannon", 56, 2), ("koopa", 62, FLOOR, None, "jumping"),
        ("pit", 68, 3), ("piranha", 76, 3), ("buzzy", 84), ("buzzy", 87),
        ("cannon", 94, 3), ("pit", 100, 2),
        ("bricks", 108, 10, LOW), ("dq", 112, LOW, "coin", 10), ("q", 114, LOW, "powerup"),
        ("koopa", 122), ("goomba", 128, 130),
        ("bricks", 136, 8, LOW), ("bricks", 138, 6, HIGH), ("hammer", 139, LOW - 1), ("hammer", 141, HIGH - 1),
        ("pit", 150, 3), ("cannon", 158, 2), ("piranha", 166, 2), ("koopa", 174, FLOOR, None, "jumping"),
        ("goomba", 182, 184, 186),
    ], checkpoint=104)
    lv.save(path)


# ------------------------------------------------------------------ mundo 6
def gen_6_1(path):
    lv = overworld("6-1", 225, 214, [
        ("lakitu", 14, 190),
        ("q", 18, LOW, "powerup"), ("stairs", 24, 3), ("pit", 27, 3), ("stairs_down", 30, 3),
        ("bricks", 40, 6, LOW), ("pit", 50, 3),
        ("stairs", 58, 4), ("pit", 62, 2), ("stairs_down", 64, 4),
        ("q", 74, LOW, "powerup"), ("pit", 80, 3), ("bricks", 88, 4, LOW),
        ("pit", 96, 2), ("pit", 102, 3),
        ("stairs", 110, 2), ("pit", 112, 3), ("stairs_down", 115, 2),
        ("qrow", 124, 3, LOW), ("pit", 132, 3),
        ("stairs", 142, 4), ("pit", 146, 3), ("stairs_down", 149, 4),
        ("pit", 160, 2), ("bricks", 168, 5, LOW), ("pit", 178, 3),
    ], checkpoint=106)
    lv.save(path)


def gen_6_2(path):
    lv = overworld("6-2", 225, 214, [
        ("piranha", 14, 2), ("piranha", 22, 3), ("goomba", 28), ("piranha", 32, 4),
        ("bricks", 38, 6, LOW), ("q", 40, LOW, "powerup"), ("piranha", 48, 2),
        ("pit", 54, 2), ("piranha", 60, 3), ("piranha", 68, 2), ("goomba", 72, 74),
        ("piranha", 78, 4), ("bricks", 84, 8, HIGH), ("goomba_at", 86, HIGH - 1),
        ("piranha", 96, 2), ("pit", 102, 3), ("piranha", 108, 3), ("koopa", 114),
        ("piranha", 120, 2), ("piranha", 126, 4), ("q", 132, LOW, "powerup"),
        ("pit", 138, 2), ("piranha", 144, 3), ("goomba", 150, 152), ("piranha", 158, 2),
        ("piranha", 166, 3), ("koopa", 172, FLOOR, None, "jumping"), ("piranha", 180, 2),
    ], checkpoint=106)
    lv.save(path)


def gen_6_3(path):
    treetop_course("6-3", path, [
        (18, 4, 11), (26, 5, 8), (36, 3, 10), (42, 6, 7), (52, 3, 11),
        (70, 5, 9), (80, 3, 6), (88, 5, 10), (104, 6, 8), (116, 3, 11), (124, 5, 9), (136, 4, 12),
    ], [
        (56, 9, 3, "horizontal", {"range": 6, "period": 4}),
        (66, 11, 3, "fall", {}),
        (94, 9, 3, "fall", {}),
        (98, 7, 3, "fall", {}),
        (130, 10, 3, "vertical", {"range": 4, "period": 3}),
        (141, 10, 3, "horizontal", {"range": 4, "period": 3}),
    ], [
        ("koopa", 44, 6, "red"), ("koopa", 106, 7, "red"),
    ], extras=[
        ("cannon", 28, 1, 8), ("cannon", 90, 1, 10), ("cannon", 126, 1, 9),
        ("q", 82, 3, "powerup"),
    ])


# ------------------------------------------------------------------ mundo 7
def gen_7_1(path):
    lv = overworld("7-1", 220, 209, [
        ("cannon", 16, 2), ("goomba", 22), ("cannon", 28, 3), ("koopa", 34, FLOOR, None, "jumping"),
        ("q", 40, LOW, "powerup"), ("cannon", 46, 2), ("cannon", 46, 1, 12),
        ("buzzy", 52), ("buzzy", 55), ("pit", 60, 3),
        ("cannon", 68, 3), ("bricks", 74, 8, LOW), ("bricks", 76, 6, HIGH),
        ("hammer", 77, LOW - 1), ("hammer", 79, HIGH - 1),
        ("cannon", 90, 2), ("koopa", 96, FLOOR, None, "jumping"), ("pit", 102, 2),
        ("buzzy", 110), ("cannon", 116, 3), ("piranha", 124, 3),
        ("q", 130, LOW, "powerup"), ("cannon", 136, 2), ("koopa", 142),
        ("pit", 148, 3), ("cannon", 156, 4), ("buzzy", 162), ("buzzy", 166),
        ("hammer", 176), ("cannon", 184, 2),
    ], checkpoint=104)
    lv.save(path)


# ------------------------------------------------------------------ mundo 8
def gen_8_1(path):
    lv = overworld("8-1", 260, 249, [
        ("goomba", 16, 18), ("pit", 24, 3), ("koopa", 32, FLOOR, None, "jumping"),
        ("piranha", 38, 3), ("pit", 44, 3), ("goomba", 50, 52, 54),
        ("q", 58, LOW, "powerup"), ("pit", 64, 4), ("koopa", 74, FLOOR, None, "jumping"),
        ("piranha", 80, 4), ("pit", 86, 3), ("buzzy", 94), ("buzzy", 97),
        ("pit", 104, 2), ("pit", 110, 3), ("koopa", 118), ("goomba", 124, 126),
        ("piranha", 132, 2), ("pit", 138, 4), ("koopa", 148, FLOOR, None, "jumping"),
        ("bricks", 154, 6, LOW), ("q", 156, LOW, "powerup"), ("pit", 164, 3),
        ("goomba", 172, 174, 176), ("piranha", 182, 3), ("pit", 188, 3),
        ("koopa", 196, FLOOR, None, "jumping"), ("koopa", 202, FLOOR, None, "jumping"),
        ("pit", 208, 2), ("goomba", 216, 218, 220), ("koopa", 230),
    ], time=300, checkpoint=128)
    lv.save(path)


def gen_8_2(path):
    lv = overworld("8-2", 235, 224, [
        ("lakitu", 18, 150),
        ("stairs", 12, 4), ("pit", 16, 3), ("stairs_down", 19, 4),
        ("q", 28, LOW, "powerup"), ("cannon", 34, 2), ("pit", 40, 3),
        ("bricks", 48, 6, LOW), ("buzzy", 56), ("pit", 62, 3), ("cannon", 70, 3),
        ("pit", 78, 2), ("koopa", 84, FLOOR, None, "jumping"), ("pit", 90, 3),
        ("bricks", 98, 8, LOW), ("q", 101, LOW, "powerup"), ("pit", 110, 2),
        ("cannon", 118, 2), ("pit", 124, 3), ("buzzy", 132), ("pit", 140, 3),
        ("piranha", 150, 3), ("cannon", 158, 3), ("hammer", 168), ("pit", 176, 3),
        ("koopa", 186, FLOOR, None, "jumping"), ("goomba", 192, 194),
    ], time=300, checkpoint=112)
    lv.save(path)


def gen_8_3(path):
    lv = overworld("8-3", 225, 214, [
        ("q", 16, LOW, "powerup"), ("koopa", 22),
        ("ledge", 28, 6, 10), ("hammer", 30, 9), ("cannon", 38, 2),
        ("pit", 44, 3), ("ledge", 52, 8, 10), ("hammer", 54, 9), ("hammer", 57, 9),
        ("piranha", 66, 3), ("pit", 72, 2), ("buzzy", 78), ("cannon", 84, 3),
        ("ledge", 92, 6, 11), ("hammer", 94, 10), ("pit", 102, 3),
        ("q", 110, LOW, "powerup"), ("cannon", 116, 2), ("koopa", 122, FLOOR, None, "jumping"),
        ("ledge", 130, 8, 10), ("hammer", 132, 9), ("hammer", 135, 9), ("pit", 142, 3),
        ("cannon", 150, 3), ("buzzy", 158), ("ledge", 166, 6, 11), ("hammer", 168, 10),
        ("pit", 176, 2), ("goomba", 184, 186),
    ], time=300, checkpoint=112)
    # Murallas de castillo de fondo (decorado).
    for c in (34, 88, 146):
        lv.deco("castle_small", c, 9)
    lv.save(path)
