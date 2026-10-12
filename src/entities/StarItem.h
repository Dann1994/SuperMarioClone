#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Super Star (seccion 5.4): rebota en el piso con saltitos energicos (a
// diferencia del hongo, que camina pegado al suelo) y tambien rebota contra
// paredes en vez de morir (a diferencia de la Fireball). Cicla sus 4 frames
// de color como el hongo/flor.
class StarItem {
public:
    void spawn(float x, float y, bool movingRight);
    // Golpe desde abajo al bloque donde esta apoyado (original:
    // ChkToStunEnemies/SetStun2): saltito de -3 px/cuadro y se aleja del
    // jugador a 1 px/cuadro.
    void bumpFromBelow(bool moveRight) {
        if (!alive_) return;
        y_ -= 2.0f;
        velY_ = kBounceVelocity;
        velX_ = moveRight ? kSpeed : -kSpeed;
    }
    void update(float dt, const Tilemap& level, int screenWidth);
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
    static constexpr float kSpeed = 60.0f;            // px/s horizontal, constante
    static constexpr float kGravity = 900.0f;         // px/s^2
    static constexpr float kBounceVelocity = -220.0f; // px/s, saltito al tocar el piso
    static constexpr float kFrameDuration = 0.08f;    // s por frame del ciclo de color
    static constexpr int kFrameCount = 4;
    static constexpr float kEdgeEpsilon = 0.01f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    float animTimer_ = 0.0f;
    int frame_ = 0;
};
