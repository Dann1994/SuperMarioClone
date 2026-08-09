#include "core/Game.h"

int main(int, char**) {
    Game game;
    if (!game.init()) {
        return 1;
    }
    game.run();
    game.shutdown();
    return 0;
}
