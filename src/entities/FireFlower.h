#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Flor de fuego (seccion 5.4): a diferencia del hongo, no se mueve. Solo
// cicla sus 4 frames de color en el lugar hasta que la agarran.
class FireFlower {
public:
    void spawn(float x, float y);
    void update(float dt);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, en ese mismo orden).
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;
    bool isAlive() const { return alive_; }
    void collect() { alive_ = false; }

    float x() const { return x_; }
    float y() const { return y_; }
    static constexpr int kSize = 16;

private:
    static constexpr float kFrameDuration = 0.1f;  // s por frame del ciclo de color
    static constexpr int kFrameCount = 4;

    float x_ = 0.0f;
    float y_ = 0.0f;
    bool alive_ = false;
    float animTimer_ = 0.0f;
    int frame_ = 0;
};
