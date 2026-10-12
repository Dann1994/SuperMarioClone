"""Regenera todos los niveles de la campana (menos 1-1, armado a mano).

Uso, parado en la raiz del proyecto:
  python tools/level_scripts/build_all.py
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import castles  # noqa: E402
import overworld as ow  # noqa: E402
import shared_levels as sh  # noqa: E402
import world1  # noqa: E402


def path(level):
    return f"levels/world{level[0]}/{level}.json"


def main():
    subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_1_2.py"),
                    path("1-2")], check=True)
    world1.gen_1_3(path("1-3"))
    world1.gen_1_4(path("1-4"))

    ow.gen_2_1(path("2-1"))
    sh.water_level("2-2", path("2-2"))
    sh.bridge_level("2-3", path("2-3"))
    castles.gen_2_4(path("2-4"))

    ow.gen_3_1(path("3-1"))
    ow.gen_3_2(path("3-2"))
    ow.gen_3_3(path("3-3"))
    castles.gen_3_4(path("3-4"))

    ow.gen_4_1(path("4-1"))
    sh.underground_level("4-2", path("4-2"))
    ow.gen_4_3(path("4-3"))
    castles.gen_4_4(path("4-4"))

    ow.gen_5_1(path("5-1"))
    ow.gen_5_2(path("5-2"))
    world1.gen_1_3(path("5-3"), hard=True)
    castles.gen_5_4(path("5-4"))

    ow.gen_6_1(path("6-1"))
    ow.gen_6_2(path("6-2"))
    ow.gen_6_3(path("6-3"))
    castles.gen_6_4(path("6-4"))

    ow.gen_7_1(path("7-1"))
    sh.water_level("7-2", path("7-2"), hard=True)
    sh.bridge_level("7-3", path("7-3"), hard=True)
    castles.gen_7_4(path("7-4"))

    ow.gen_8_1(path("8-1"))
    ow.gen_8_2(path("8-2"))
    ow.gen_8_3(path("8-3"))
    castles.gen_8_4(path("8-4"))


if __name__ == "__main__":
    main()
