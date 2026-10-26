#include <cstdlib>
#include <string>

#include "core/Game.h"

// Uso: mario.exe [--level <archivo.json> [--start-col <columna>] [--start-row <fila>]]
//   --level:     arranca directo en ese nivel en vez del selector de
//                escenarios (lo usa el editor de niveles para probar).
//   --start-col: arranca en esa columna del nivel (como un checkpoint).
//   --start-row: con --start-col, parado con los pies sobre esa fila (para
//                probar cosas altas sin tener que llegar).
//   --dump-map:  con --level, guarda el mapa entero como BMP y sale (para
//                compararlo con los mapas del original).
//   --no-update: no busca actualizaciones al abrir.
//   Prueba del modo en red con dos procesos en esta PC:
//     --net-host NOMBRE --net-code-out codigo.txt --net-frames N --net-out a.txt
//     --net-join NOMBRE --net-code-in codigo.txt --net-frames N --net-out b.txt
//   (botones automaticos; los hashes de a.txt y b.txt tienen que coincidir).
int main(int argc, char** argv) {
    Game game;
    if (!game.init()) {
        return 1;
    }
    std::string level;
    std::string netHost, netJoin, netCodeOut, netCodeIn, netOut;
    int netFrames = 0;
    int startCol = -1;
    int startRow = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--no-update") game.setNoUpdate(true);
    }
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--level") level = argv[i + 1];
        if (arg == "--start-col") startCol = std::atoi(argv[i + 1]);
        if (arg == "--start-row") startRow = std::atoi(argv[i + 1]);
        if (arg == "--dump-map") game.setDumpMap(argv[i + 1]);
        if (arg == "--net-host") netHost = argv[i + 1];
        if (arg == "--net-join") netJoin = argv[i + 1];
        if (arg == "--net-code-out") netCodeOut = argv[i + 1];
        if (arg == "--net-code-in") netCodeIn = argv[i + 1];
        if (arg == "--net-out") netOut = argv[i + 1];
        if (arg == "--net-frames") netFrames = std::atoi(argv[i + 1]);
    }
    if (!netHost.empty() || !netJoin.empty()) game.setNetAutoTest(netHost, netJoin, netCodeOut, netCodeIn, netOut, netFrames);
    // Probando un nivel (editor) o la red automatica: sin buscar actualizaciones.
    if (!level.empty() || !netHost.empty() || !netJoin.empty()) game.setNoUpdate(true);
    if (!level.empty()) game.startWithLevelFile(level, startCol, startRow);
    game.run();
    game.shutdown();
    return 0;
}
