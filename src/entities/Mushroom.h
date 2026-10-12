#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Super Mushroom (seccion 5.4): camina en linea recta a velocidad
// constante, rebota al chocar una pared (invierte direccion), cae por
// gravedad y sigue caminando aunque se quede sin piso debajo (a diferencia
// de un Koopa, no le tiene miedo a los bordes).
class Mushroom {
public:
    // oneUp: variante 1-UP (suma una vida en vez de crecer, ver
    // Game::checkMushroomPickup) -- misma fisica, Game.cpp elige el sprite
    // (mushroomSprite_ u oneUpSprite_) segun isOneUp() al dibujar.
    void spawn(float x, float y, bool movingRight, bool oneUp = false);
    void update(float dt, const Tilemap& level, int screenWidth);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, en ese mismo orden).
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;
    bool isAlive() const { return alive_; }
    void collect() { alive_ = false; }
    bool isOneUp() const { return oneUp_; }
    // Golpe desde abajo al bloque donde esta apoyado (original:
    // ChkToStunEnemies/SetStun2): saltito de -3 px/cuadro y se aleja del
    // jugador a 1 px/cuadro.
    void bumpFromBelow(bool moveRight) {
        if (!alive_) return;
        y_ -= 2.0f;
        velY_ = -180.0f;
        velX_ = moveRight ? kSpeed : -kSpeed;
    }

    float x() const { return x_; }
    float y() const { return y_; }
    static constexpr int kSize = 16;

private:
    static constexpr float kSpeed = 60.0f;  // px/s: 0x10/16 px/cuadro, como el original
    static constexpr float kGravity = 858.0f;  // px/s^2: la de los enemigos del original (0x3d)
    static constexpr float kMaxFallSpeed = 180.0f;  // px/s: 3 px/cuadro
    static constexpr float kEdgeEpsilon = 0.01f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool oneUp_ = false;
};
