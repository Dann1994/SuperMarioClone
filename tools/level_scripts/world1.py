"""Mundo 1: 1-3 (copas de arbol) y 1-4 (castillo). 1-1 y 1-2 tienen su
propio JSON (1-2: gen_1_2.py)."""
from levelgen import Level, GROUND, FLOOR, ROWS, treetop


def gen_1_3(path, hard=False):
    """1-3 (y 5-3 con hard=True): copas de arbol, plataformas moviles."""
    name = "5-3" if hard else "1-3"
    lv = Level(name, 170, time=300)
    lv.ground(0, 15)
    lv.overworld_scenery(0, 15)
    lv.clouds(16, 150)
    trees = [  # (col, ancho, fila del tope)
        (18, 4, 12), (24, 6, 9), (32, 5, 6), (40, 3, 11), (44, 5, 8),
        (54, 5, 10), (60, 7, 7), (72, 4, 12), (78, 5, 9), (86, 3, 6),
        (100, 4, 10), (108, 6, 7), (117, 4, 11), (130, 5, 9), (138, 4, 12),
    ]
    for c, w, top in trees:
        treetop(lv, c, w, top)
        if w >= 5:
            lv.coins(c + 1, c + w - 2, top - 2)
    # Plataformas moviles entre arboles.
    lv.platform(91, 9, 3, "horizontal", range=4, period=4)
    lv.platform(123, 8, 3, "vertical", range=4, period=4)
    lv.platform(144, 10, 3, "horizontal", range=3, period=3.5)
    lv.q(62, 3, "powerup")
    # Enemigos: Koopas rojos patrullando copas, paratroopas y Goombas.
    lv.koopa(26, 8, color="red")
    lv.goombas([34, 36], r=4)
    lv.koopa(62, 6, color="red")
    lv.koopa(80, 8, state="jumping")
    lv.goombas([110, 112], r=6)
    lv.koopa(132, 8, color="red")
    if hard:
        lv.koopa(46, 6, color="red", state="flying")
        lv.koopa(102, 8, state="jumping")
        lv.koopa(118, 9, state="jumping")
    # Llegada: piso, escalera y bandera.
    lv.ground(150, 169)
    lv.flag_end(162, stairs=0)
    lv.stairs_up(152, 4, width=5)
    lv.checkpoints = [86]
    lv.save(path)


def castle_start(lv, width=16, floor=8):
    """Arranque clasico de castillo: piso alto, techo bajo."""
    lv.ground(0, width - 1, top=floor)
    lv.castle_ceiling(0, width - 1, (1, 3))


def gen_1_4(path):
    lv = Level("1-4", 160, world="castle", time=300)
    castle_start(lv, 14, floor=8)
    # Escalones de bajada hacia el primer pozo de lava.
    lv.ground(14, 15, top=9)
    lv.ground(16, 17, top=10)
    lv.castle_ceiling(14, 30, (1, 2))
    lv.fill(18, 20, 13, 15, 'L')
    lv.ground(21, 30, top=10)
    lv.enemy("firebar", 25, 10, length=6, speed=105)
    lv.put(25, 10, 'S')
    lv.q(28, 6, "powerup")
    # Pasillo con techo bajo y barras de fuego.
    lv.ground(31, 70)
    lv.castle_ceiling(31, 70, (1, 7))
    for c in (40, 52, 64):
        lv.put(c, 13, 'S')
        lv.enemy("firebar", c, 13, length=5, speed=-105 if c == 52 else 105)
    # Pozo de lava con Podoboo y plataformas de bloques.
    lv.fill(71, 74, 13, 15, 'L')
    lv.enemy("podoboo", 72, 13)
    lv.ground(75, 82, top=12)
    lv.fill(83, 86, 13, 15, 'L')
    lv.enemy("podoboo", 84, 13)
    lv.ground(87, 112, top=10)
    lv.castle_ceiling(71, 112, (1, 3))
    for c in (95, 105):
        lv.put(c, 6, 'S')
        lv.enemy("firebar", c, 6, length=6, speed=105)
    # Puente de Bowser.
    lv.castle_ceiling(113, 125, (1, 2))
    end = lv.bowser_bridge(113, 13, row=10)
    lv.ground(end, 159, top=10)
    lv.checkpoints = [77]
    lv.save(path)


if __name__ == "__main__":
    gen_1_3("levels/world1/1-3.json")
    gen_1_4("levels/world1/1-4.json")
