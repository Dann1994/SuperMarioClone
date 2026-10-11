#include <cstdlib>
#include <string>

#include "core/Game.h"

// Uso: mario.exe [--level <archivo.json> [--start-col <columna>]]
//   --level:     arranca directo en ese nivel en vez del selector de
//                escenarios (lo usa el editor de niveles para probar).
//   --start-col: arranca en esa columna del nivel (como un checkpoint).
int main(int argc, char** argv) {
    Game game;
    if (!game.init()) {
        return 1;
    }
    std::string level;
    int startCol = -1;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--level") level = argv[i + 1];
        if (arg == "--start-col") startCol = std::atoi(argv[i + 1]);
    }
    if (!level.empty()) game.startWithLevelFile(level, startCol);
    game.run();
    game.shutdown();
    return 0;
}
