#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Bola de fuego de Mario/Luigi fuego: viaja en linea recta horizontal y
// rebota contra el piso a altura constante (como en el juego original),
// hasta chocar con una pared (ahi juega una animacion corta de explosion),
// salir de la pantalla, caer en un pozo o quedarse sin tiempo de vida.
class Fireball {
public:
    void spawn(float x, float y, bool movingRight);
    void update(float dt, const Tilemap& level, int screenWidth);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& projectileSprite,
                const SpriteSheet& explosionSprite, float cameraX = 0.0f) const;
    // Sigue "viva" (ocupando uno de los 2 slots del jugador) mientras dura la
    // explosion, no solo mientras vuela.
    bool isAlive() const { return alive_; }
    bool isExploding() const { return exploding_; }
    // Hace estallar la bola ya (choco contra un enemigo, ver Game::checkFireballGoombaCollision),
    // en vez de esperar a que choque contra un bloque por su cuenta.
    void explode();

    float x() const { return x_; }
    float y() const { return y_; }

    static constexpr int kSize = 8;

private:
    static constexpr float kSpeed = 200.0f;           // px/s horizontal, constante
    static constexpr float kGravity = 600.0f;         // px/s^2
    static constexpr float kBounceVelocity = -130.0f; // px/s al rebotar (parabola baja, estilo NES)
    static constexpr float kMaxLifetime = 4.0f;        // segundos, por si nunca choca ni cae
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kExplosionFrameDuration = 0.06f; // s por frame de explosion
    static constexpr int kExplosionFrameCount = 3;
    static constexpr int kExplosionSize = 16;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    float lifetime_ = 0.0f;
    float animTimer_ = 0.0f;
    int frame_ = 0;

    bool exploding_ = false;
    float explosionTimer_ = 0.0f;
};
