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
    // Martillo del traje de Hammer Bro: sale para arriba y adelante en arco,
    // atraviesa el escenario y a los enemigos (los mata de un golpe, tambien
    // a los inmunes al fuego; a Bowser con 2) y desaparece al salir del nivel.
    void spawnHammer(float x, float y, bool movingRight);
    // Bumeran del traje de Boomerang Bro: sale hacia adelante frenando, a
    // las ~6 baldosas se da vuelta y vuelve por el mismo camino (con una
    // curva suave hacia arriba); atraviesa el escenario. Mata igual que el
    // martillo (isHammer() tambien es true) y quien lo tiro lo atrapa al
    // volver (Game lo saca con remove()).
    void spawnBoomerang(float x, float y, bool movingRight);
    // "Proyectil de traje": atraviesa todo y mata de un golpe (martillo o bumeran).
    bool isHammer() const { return hammer_; }
    bool isBoomerang() const { return boomerang_; }
    // Ya se dio vuelta y viene de regreso (se puede atrapar).
    bool isReturning() const { return boomerang_ && boomerangTime_ > kBoomerangTurnTime; }
    // Un martillo cuenta un solo golpe a Bowser aunque lo atraviese varios cuadros.
    bool hasHitBowser() const { return hitBowser_; }
    void markHitBowser() { hitBowser_ = true; }
    void update(float dt, const Tilemap& level, int screenWidth);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& projectileSprite,
                const SpriteSheet& explosionSprite, float cameraX = 0.0f,
                const SpriteSheet* hammerSprite = nullptr, const SpriteSheet* boomerangSprite = nullptr) const;
    // Sigue "viva" (ocupando uno de los 2 slots del jugador) mientras dura la
    // explosion, no solo mientras vuela.
    bool isAlive() const { return alive_; }
    bool isExploding() const { return exploding_; }
    // Saca la bola de una (salio de la pantalla).
    void remove() {
        alive_ = false;
        exploding_ = false;
    }
    // Hace estallar la bola ya (choco contra un enemigo, ver Game::checkFireballGoombaCollision),
    // en vez de esperar a que choque contra un bloque por su cuenta.
    void explode();
    // true una sola vez el frame en que explota SOLA contra una pared (ver
    // update()), para que Game.cpp sepa cuando reproducir el sonido de
    // golpe -- no se pone en true si explode() la hizo estallar desde
    // afuera (contra un enemigo, eso no "choca con la pared"). Se resetea
    // solo, mismo criterio que Mario::consumeJumpRequest().
    bool consumeWallHitRequest() {
        const bool value = wallHitRequested_;
        wallHitRequested_ = false;
        return value;
    }

    float x() const { return x_; }
    float y() const { return y_; }

    static constexpr int kSize = 8;

private:
    // Valores reales del original (disassembly de doppelganger,
    // FireballXSpdData/FireballBGCollision, subpixeles/frame a 60fps
    // convertidos a px/s y px/s^2) -- investigado a pedido del usuario.
    static constexpr float kSpeed = 240.0f;             // px/s horizontal, constante
    static constexpr float kGravity = 1125.0f;          // px/s^2
    static constexpr float kInitialFallSpeed = 240.0f;  // px/s hacia abajo, desde el instante que sale de la mano
    static constexpr float kBounceVelocity = -180.0f;   // px/s al rebotar (parabola baja, estilo NES)
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
    bool wallHitRequested_ = false;
    bool hammer_ = false;
    bool hitBowser_ = false;
    bool movingRight_ = true;
    static constexpr float kHammerSpeedX = 120.0f;    // px/s hacia adelante
    static constexpr float kHammerSpeedY = -300.0f;   // px/s hacia arriba al salir
    static constexpr float kHammerGravity = 900.0f;   // px/s^2
    static constexpr int kHammerSpriteSize = 16;
    bool boomerang_ = false;
    float boomerangTime_ = 0.0f;
    float boomerangStartX_ = 0.0f;
    float boomerangStartY_ = 0.0f;
    // Ida y vuelta: x(t) = x0 + dir * (v0 t - a t^2 / 2); llega a lo mas lejos
    // (v0^2 / 2a = 96 px, 6 baldosas) en kBoomerangTurnTime y vuelve.
    static constexpr float kBoomerangSpeed = 240.0f;                       // px/s al salir
    static constexpr float kBoomerangTurnTime = 0.8f;                      // s hasta darse vuelta
    static constexpr float kBoomerangDecel = kBoomerangSpeed / kBoomerangTurnTime;
    static constexpr float kBoomerangLift = 14.0f;                         // px que sube en la curva
    static constexpr float kBoomerangLifetime = 3.2f;                      // s, si nadie lo atrapa
};
