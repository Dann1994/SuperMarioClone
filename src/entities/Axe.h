#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// El hacha al final del puente (seccion 5.2/5.3): tocarla es la UNICA forma
// de derrotar a Bowser sin ser Mario/Luigi de fuego (ver Bowser::killByAxe,
// Game::checkAxeCollision) -- en el original corta el puente y lo manda a
// la lava, ac'a no hay fisica de puente/lava todavia asi que el efecto es
// directo: toca el hacha, Bowser cae derrotado con la misma parabola que
// cualquier otra forma de matarlo -- o, si el nivel tiene puente ('b'),
// arranca el derrumbe (ver Game::startBridgeCollapse). Quieta, sin fisica
// propia. Sprite: assets/sprites/axe.png, 3 frames de 16x16 que ciclan de
// color igual que los bloques de interrogacion (en el original el hacha usa
// la misma paleta animada que las monedas/'?', "Palette 3 Tiles" de
// Documentaciones/Tiles.png, ver tools/asset_scripts/gen_castle.py).
class Axe {
public:
    void spawn(float x, float y);
    // Lo desactiva: para cuando se cambia de escenario de prueba, o al
    // tocarla (ver Game::checkAxeCollision).
    void reset() { alive_ = false; }
    // frame: 0..2, el mismo brillo compartido de los '?' (Game::questionBlockAnimFrame_).
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int frame, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    void collect() { alive_ = false; }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 16;

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool alive_ = false;
};
