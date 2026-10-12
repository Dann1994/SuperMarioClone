"""Tipos de nivel que el original repite con variantes: agua (2-2/7-2),
puentes con Cheep Cheep saltarinas (2-3/7-3) y subterraneo con escena de
tuberia (4-2)."""
from levelgen import (Level, ROWS, GROUND, FLOOR, LOW, HIGH, pipe_intro, exit_area, water_ceiling, coral,
                      rope_bridge, water, pillar)


def water_level(name, path, hard=False):
    """Nivel de agua: intro caminando hasta la tuberia, el fondo del mar con
    corales, Bloopers y Cheep Cheeps, y la salida por una tuberia en L."""
    UW = 160
    EW = 28
    intro_w = 16
    U0 = intro_w + 1
    E0 = U0 + UW + 1
    lv = Level(name, E0 + EW)
    pipe_intro(lv, intro_w)
    lv.warp(10, 12, U0 + 3, 3, side=True)

    def u(x):
        return U0 + x

    # Fondo del mar con dos fosas, superficie arriba.
    lv.ground(u(0), u(UW - 1))
    for a, b in [(48, 52), (104, 109)]:
        lv.pit(u(a), u(b))
    water_ceiling(lv, u(0), u(UW - 1))
    # Corales y rocas.
    for x, top in [(10, 11), (18, 9), (26, 12), (34, 8), (40, 11), (58, 10), (66, 7), (74, 11), (82, 9),
                   (92, 12), (98, 8), (114, 10), (122, 6), (130, 11), (138, 9)]:
        coral(lv, u(x), top)
    # Plataformas de roca con monedas.
    for x0, x1, r in [(20, 25, 7), (44, 56, 9), (70, 76, 5), (86, 90, 10), (100, 112, 10), (126, 132, 7)]:
        lv.row(u(x0), u(x1), r, '#')
        lv.coins(u(x0 + 1), u(x1 - 1), r - 1)
    lv.q(u(29), 6, "powerup")
    # Bloopers y Cheep Cheeps.
    for x, r in [(30, 8), (62, 6), (95, 7), (120, 9)] + ([(45, 5), (80, 8), (110, 4), (135, 6)] if hard else []):
        lv.enemy("blooper", u(x), r)
    cheeps = [(22, 10), (38, 6), (54, 11), (68, 9), (78, 4), (88, 7), (102, 5), (116, 11), (128, 8)]
    if hard:
        cheeps += [(42, 9), (60, 4), (84, 11), (108, 8), (124, 5), (140, 7)]
    for i, (x, r) in enumerate(cheeps):
        lv.enemy("cheep", u(x), r, color="red" if i % 2 else "gray")
    # Salida: tuberia en L al final, pared atras.
    lv.ground(u(144), u(UW - 1), top=12)
    lv.l_pipe(u(150), 8)
    lv.warp(u(150), 10, E0 + 2, 10, side=True)
    lv.fill(u(154), u(UW - 1), 1, 15, '#')
    lv.area(U0, U0 + UW - 1, "underwater")
    lv.column(E0 - 1, 0, '#', ROWS)
    exit_area(lv, E0, EW)
    lv.checkpoints = [u(80)]
    lv.save(path)


def bridge_level(name, path, hard=False):
    """Puentes de soga sobre el agua, con Cheep Cheep saltando desde abajo
    durante casi todo el recorrido."""
    W = 200
    lv = Level(name, W)
    lv.ground(0, 14)
    lv.overworld_scenery(0, 14)
    lv.clouds(15, 180)
    water(lv, 15, 175)
    # Escalera de arranque hasta la altura de los puentes.
    lv.stairs_up(9, 4, width=6)
    # (columna, ancho del pilar, fila del tope) y puentes entre pilares.
    pillars = [(15, 2, 10), (34, 2, 10), (52, 2, 8), (70, 3, 10), (90, 2, 9), (108, 2, 9), (126, 3, 10),
               (146, 2, 8), (164, 3, 10)]
    for c, w, top in pillars:
        pillar(lv, c, top, w)
    rope_bridge(lv, 17, 33, 10)
    rope_bridge(lv, 36, 51, 8)
    lv.pit(43, 45)   # hueco en el puente
    rope_bridge(lv, 54, 69, 8)
    rope_bridge(lv, 73, 89, 10)
    lv.pit(80, 82)
    rope_bridge(lv, 92, 107, 9)
    rope_bridge(lv, 110, 125, 9)
    lv.pit(116, 118)
    rope_bridge(lv, 129, 145, 10)
    rope_bridge(lv, 148, 163, 8)
    lv.pit(155, 157)
    water(lv, 15, 175)
    for c, w, top in pillars:  # los pilares van por encima del agua
        pillar(lv, c, top, w)
    # Monedas arriba de los puentes, un bloque con power-up.
    for x0, x1, r in [(20, 26, 6), (58, 64, 4), (96, 102, 5), (132, 138, 6)]:
        lv.coins(x0, x1, r)
    lv.q(62, 4, "powerup")
    lv.enemy("cheep_jump", 18, FLOOR, end_col=166)
    if hard:
        for c, r in [(40, 6), (76, 8), (100, 7), (134, 8), (152, 6)]:
            lv.koopa(c, r, color="red", state="flying")
        lv.koopa(60, 7, state="jumping")
    else:
        lv.koopa(76, 9, state="jumping")
        lv.koopa(134, 9, color="red")
    # Llegada.
    lv.ground(167, W - 1)
    lv.stairs_up(169, 4, width=5)
    lv.flag_end(184, stairs=0)
    lv.checkpoints = [92]
    lv.save(path)


def underground_level(name, path):
    """Subterraneo (4-2): como el 1-2 pero con elevadores largos, Buzzy
    Beetles y Piranha Plants."""
    UW = 170
    EW = 28
    intro_w = 16
    U0 = intro_w + 1
    E0 = U0 + UW + 1
    lv = Level(name, E0 + EW)
    pipe_intro(lv, intro_w)
    lv.warp(10, 12, U0 + 2, 4, side=True)

    def u(x):
        return U0 + x

    pits = [(40, 43), (78, 82), (118, 131)]
    lv.ground(u(0), u(UW - 1))
    for a, b in pits:
        lv.pit(u(a), u(b))
    lv.column(u(0), 3, 'B')
    for x in list(range(6, 116)) + list(range(134, UW)):
        lv.put(u(x), 3, 'B')
    # Escalones de ladrillo con ? arriba.
    for x in range(8, 14):
        lv.put(u(x), LOW, 'B')
    lv.q(u(10), LOW, "powerup")
    lv.brick_item(u(12), LOW, "coin", 10)
    for x, h in [(20, 2), (22, 3), (24, 4), (26, 4)]:
        lv.column(u(x), 14 - h, 'S')
    lv.piranha(u(32), 3)
    # Plataformas de ladrillo sobre el primer pozo.
    lv.row(u(38), u(45), 9, 'B')
    lv.coins(u(39), u(44), 8)
    lv.platform(u(50), 13, 3, "vertical", range=5, period=4)
    lv.row(u(56), u(70), 7, 'B')
    lv.q(u(63), 7, "one_up")
    lv.piranha(u(72), 4)
    lv.platform(u(79), 10, 3, "horizontal", range=2, period=3)
    lv.piranha(u(86), 2)
    lv.row(u(92), u(104), LOW, 'B')
    lv.q(u(98), LOW, "star")
    lv.piranha(u(108), 3)
    for i in range(4):
        lv.column(u(112 + i), 13 - i, 'S')
    lv.column(u(116), 10, 'S')
    lv.column(u(117), 10, 'S')
    # Elevadores largos, como el 4-2.
    lv.platform(u(120), 2, 3, "lift_down", speed=50)
    lv.platform(u(120), 9, 3, "lift_down", speed=50)
    lv.platform(u(126), 6, 3, "lift_up", speed=50)
    lv.platform(u(126), 13, 3, "lift_up", speed=50)
    lv.row(u(140), u(150), LOW, 'B')
    lv.coins(u(141), u(149), LOW - 1)
    lv.piranha(u(152), 2)
    lv.l_pipe(u(160), 10)
    lv.warp(u(160), 12, E0 + 2, 10, side=True)
    for x in range(166, UW):
        lv.column(u(x), 3, 'B')
    # Enemigos.
    for x in [16, 18, 30, 48, 60, 62, 90, 94, 136, 138]:
        lv.enemy("buzzy", u(x), FLOOR)
    lv.goombas([u(x) for x in [58, 66, 100, 102, 144, 146]], r=FLOOR)
    lv.koopa(u(97), FLOOR, color="red")
    lv.area(U0, U0 + UW - 1, "underground")
    lv.column(E0 - 1, 0, '#', ROWS)
    exit_area(lv, E0, EW)
    lv.checkpoints = [u(84)]
    lv.save(path)
