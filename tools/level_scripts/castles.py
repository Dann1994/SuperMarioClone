"""Castillos 2-4 a 8-4 armados por segmentos (1-4 esta en world1.py).

Cada castillo es una lista de segmentos que se van pegando de izquierda a
derecha (ver Castle). Los laberintos de 4-4/7-4/8-4 del original se
simplifican a un recorrido lineal; el resto (lava, Podoboos, barras de
fuego, plataformas, el puente con Bowser y el hacha) va igual que siempre.
"""
from levelgen import Level, ROWS, GROUND, LOW, HIGH


class Castle:
    def __init__(self, name, time=300):
        self.lv = Level(name, 600, world="castle", time=time)
        self.c = 0
        self.top = 8  # altura del piso actual (fila del tope)

    # Piso a la altura `top`, con techo (filas ceil[0]..ceil[1]).
    def floor(self, w, top=None, ceil=(1, 3), firebars=(), qs=(), enemies=(), blocks=()):
        lv, c = self.lv, self.c
        top = self.top if top is None else top
        lv.ground(c, c + w - 1, top=top)
        if ceil:
            lv.castle_ceiling(c, c + w - 1, ceil)
        for dx, r, length, speed in firebars:
            lv.put(c + dx, r, 'S')
            lv.enemy("firebar", c + dx, r, length=length, speed=speed)
        for dx, r, item in qs:
            lv.q(c + dx, r, item)
        for dx, r, w2 in blocks:
            lv.row(c + dx, c + dx + w2 - 1, r, 'S')
        for kind, dx, *rest in enemies:
            r = rest[0] if rest else top - 1
            lv.enemy(kind, c + dx, r)
        self.c += w
        self.top = top
        return self

    def corridor(self, w, firebars=(), qs=(), enemies=()):
        """Pasillo a nivel del piso con techo bajo (filas 1-7), con una
        zona de 3 columnas antes para bajar sin chocar el techo."""
        self.floor(3, top=GROUND, ceil=(1, 3))
        return self.floor(w, top=GROUND, ceil=(1, 7), firebars=firebars, qs=qs, enemies=enemies)

    def lava(self, w, podoboo=True, ceil=(1, 3)):
        lv, c = self.lv, self.c
        lv.fill(c, c + w - 1, 13, ROWS - 1, 'L')
        if ceil:
            lv.castle_ceiling(c, c + w - 1, ceil)
        if podoboo:
            lv.enemy("podoboo", c + w // 2, 13)
        self.c += w
        return self

    def lift(self, w, row, ceil=(1, 2), mode="horizontal", **kw):
        """Pozo de lava ancho que se cruza con una plataforma movil."""
        lv, c = self.lv, self.c
        lv.fill(c, c + w - 1, 13, ROWS - 1, 'L')
        if ceil:
            lv.castle_ceiling(c, c + w - 1, ceil)
        if mode == "horizontal":
            lv.platform(c + 1, row, 3, "horizontal", range=kw.get("range", w - 5), period=kw.get("period", 4))
        else:
            lv.platform(c + w // 2 - 1, row, 3, mode, **kw)
        lv.enemy("podoboo", c + w // 2, 13)
        self.c += w
        return self

    def bridge(self, hammer_bro=False):
        """El puente de Bowser al final, a la altura del piso actual."""
        lv = self.lv
        lv.castle_ceiling(self.c, self.c + 12, (1, 2))
        end = lv.bowser_bridge(self.c, 13, row=self.top)
        if hammer_bro:
            lv.enemy("hammer_bro", self.c - 4, self.top - 1)
        self.c = end + 1
        return self

    def save(self, path, checkpoint):
        lv = self.lv
        lv.w = self.c
        lv.g = [r[:self.c] for r in lv.g]
        lv.checkpoints = [checkpoint]
        lv.save(path)


def gen_2_4(path):
    k = Castle("2-4")
    k.floor(14, top=8)
    k.floor(4, top=10, ceil=(1, 2)).lava(3).floor(10, top=10, firebars=[(5, 10, 6, 100)], qs=[(3, 6, "powerup")])
    k.lift(12, 10).floor(6, top=10)
    k.corridor(40, firebars=[(8, 13, 5, -100), (20, 9, 5, 120), (32, 13, 5, 100)])
    k.lava(4).floor(4, top=12).lava(4).floor(4, top=12).lava(4)
    k.floor(22, top=10, firebars=[(8, 6, 6, 90), (16, 6, 6, -90)])
    k.bridge()
    k.save(path, 76)


def gen_3_4(path):
    k = Castle("3-4")
    k.floor(12, top=8)
    k.floor(4, top=10).lava(4).floor(8, top=10, firebars=[(4, 6, 6, 110)])
    k.lava(3).floor(6, top=9, qs=[(3, 5, "powerup")]).lava(3)
    k.corridor(36, firebars=[(6, 13, 5, 110), (14, 9, 5, -110), (22, 13, 5, 110), (30, 9, 5, -110)])
    k.lift(14, 9, mode="vertical", range=4, period=3).floor(6, top=10)
    k.lava(4).floor(4, top=10).lava(4)
    k.floor(24, top=10, firebars=[(6, 6, 6, 100), (14, 6, 6, 100)])
    k.bridge()
    k.save(path, 80)


def gen_4_4(path):
    # El laberinto original se simplifica: dos niveles de pasillo con
    # escaleras, sin el bucle que hace volver al principio.
    k = Castle("4-4")
    k.floor(12, top=8)
    k.floor(18, top=10, ceil=(1, 5), firebars=[(8, 10, 6, -100)])
    k.lava(3).floor(14, top=8, ceil=(1, 2))
    k.lava(4).corridor(20, firebars=[(6, 13, 5, 110), (14, 13, 5, -110)])
    k.floor(10, top=11).lava(3).floor(10, top=9, qs=[(5, 5, "powerup")])
    k.lift(12, 9).floor(4, top=10)
    k.floor(24, top=10, firebars=[(5, 6, 6, 110), (12, 10, 6, -110), (19, 6, 6, 110)])
    k.bridge()
    k.save(path, 88)


def gen_5_4(path):
    k = Castle("5-4")
    k.floor(12, top=8)
    k.floor(4, top=10).lava(4).floor(12, top=10, firebars=[(4, 10, 6, 120), (9, 6, 6, -120)])
    k.lava(4).floor(4, top=10).lava(4)
    k.corridor(30, firebars=[(8, 9, 6, 120), (20, 9, 6, -120)], qs=[(14, 10, "powerup")])
    k.lift(13, 10).floor(4, top=10).lift(13, 9)
    k.floor(10, top=10, firebars=[(5, 10, 12, 80)])
    k.lava(4).floor(20, top=10, firebars=[(10, 6, 6, 130)])
    k.bridge()
    k.save(path, 78)


def gen_6_4(path):
    k = Castle("6-4")
    k.floor(12, top=8)
    k.floor(4, top=10).lava(3).floor(4, top=10).lava(3).floor(10, top=10, firebars=[(5, 10, 6, -120)])
    k.corridor(34, firebars=[(6, 13, 6, 120), (14, 9, 6, -120), (22, 13, 6, 120)],
            qs=[(28, 10, "powerup")])
    k.lift(14, 10).floor(6, top=9, firebars=[(3, 9, 6, 130)])
    k.lava(4).floor(4, top=10).lava(4).floor(4, top=10)
    k.floor(22, top=10, firebars=[(4, 6, 6, 120), (11, 10, 6, -120), (18, 6, 6, 120)])
    k.bridge()
    k.save(path, 84)


def gen_7_4(path):
    # Laberinto simplificado (ver gen_4_4).
    k = Castle("7-4")
    k.floor(12, top=8)
    k.floor(16, top=10, ceil=(1, 4), firebars=[(7, 10, 6, 120)])
    k.floor(20, top=7, ceil=(1, 2))
    k.corridor(20, firebars=[(5, 13, 5, -120), (13, 13, 5, 120)], qs=[(10, 10, "powerup")])
    k.lava(4).floor(4, top=11).lava(4).floor(4, top=10).lava(4)
    k.lift(14, 9).floor(6, top=10)
    k.floor(24, top=10, firebars=[(4, 6, 6, 130), (12, 6, 6, -130), (20, 10, 6, 130)])
    k.bridge()
    k.save(path, 88)


def gen_8_4(path):
    # Laberinto de tuberias simplificado a un recorrido lineal; antes del
    # puente espera un Hammer Bro, como el original.
    k = Castle("8-4", time=400)
    k.floor(12, top=8)
    k.floor(6, top=10).lava(4).floor(10, top=10, firebars=[(5, 10, 6, 130)])
    k.lava(4).floor(4, top=10).lava(4)
    k.corridor(30, firebars=[(6, 13, 6, 130), (16, 9, 6, -130), (24, 13, 6, 130)],
            qs=[(11, 10, "powerup")], enemies=[("buzzy", 20), ("buzzy", 27)])
    k.lift(13, 10).floor(4, top=10).lift(13, 9)
    k.floor(6, top=10).lava(4).floor(6, top=10).lava(4)
    k.floor(24, top=10, firebars=[(4, 6, 6, 130), (12, 10, 6, -130)], enemies=[("podoboo", 18, 13)])
    k.bridge(hammer_bro=True)
    k.save(path, 90)


if __name__ == "__main__":
    gen_2_4("levels/world2/2-4.json")
    gen_3_4("levels/world3/3-4.json")
    gen_4_4("levels/world4/4-4.json")
    gen_5_4("levels/world5/5-4.json")
    gen_6_4("levels/world6/6-4.json")
    gen_7_4("levels/world7/7-4.json")
    gen_8_4("levels/world8/8-4.json")
