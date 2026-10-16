#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Goomba (seccion 5.2), reescrito sobre el original (SMB-Vanilla:
// InitNormalEnemy, MoveNormalEnemy, EnemyToBGCollisionDet,
// PlayerEnemyCollision, SetStun2 y el dibujo de A_GOOMBA; sin copiar codigo,
// solo los valores y las reglas):
//
// - Camina a 8/16 px por cuadro (30 px/s; mas rapido en el modo dificil),
//   no le teme a los bordes (cae) y da la vuelta al chocar una pared.
// - Caja de choque con el jugador: InitGoomba llama a SmallBBox
//   (BoundBoxCtrl 9) = 10x6, 3 px adentro de cada costado y de 6 a 12 px
//   desde arriba del sprite (no el cuadro entero de 16x16: con el cuadro
//   entero lastimaba de costado antes de tiempo y en las esquinas).
// - Pisado (estado STOMPED): queda achatado unos instantes y desaparece.
// - Estrella o fuego (SetStun2): se da vuelta, salta 3 px por cuadro hacia
//   arriba con 1 px por cuadro hacia el lado contrario al golpe y cae sin
//   chocar con nada hasta salir de la pantalla.
// - Al lastimar al jugador se gira para mirarlo (ChkEnemyFaceRight / LInj).
// - Bamboleo: todos los Goombas espejan el sprite juntos cada 8 cuadros
//   (FrameCounter & 8), por eso el reloj es compartido (advanceWaddle).
class Goomba {
public:
    // Le cayo encima al caparazon del traje de Hammer Bro: rebota para
    // arriba. true si recien rebota (venia cayendo), para el sonido.
    bool bounceUp(float velY) {
        const bool wasFalling = velY_ >= 0.0f;
        velY_ = velY;
        return wasFalling;
    }
    // Reloj comun del bamboleo: Game lo avanza una vez por cuadro (y no en
    // pausa), como el FrameCounter del original.
    static void advanceWaddle(float dt);

    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin animacion de muerte (cambio de escenario, no muerte).
    void reset() { state_ = State::Inactive; }
    // screenHeight: alto visible (cayendo muerto desaparece al pasarlo).
    void update(float dt, const Tilemap& level, int screenHeight);
    // worldRow: fila del sheet por tipo de mundo.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return state_ != State::Inactive; }
    bool isSquished() const { return state_ == State::Stomped; }
    bool isFalling() const { return state_ == State::Defeated; }
    // Puede pisarse o lastimar solo caminando (o cayendo de un borde).
    bool isDangerous() const { return state_ == State::Walking; }

    // Pisoton: achatado y despues desaparece.
    void squish();
    // Estrella o fuego: sale despedido hacia flyRight, sin colision.
    bool killFalling(bool flyRight);
    // Choque con otro enemigo: invierte la direccion y se separa pushX.
    void bounceOffEnemy(float pushX) {
        x_ += pushX;
        velX_ = -velX_;
    }
    // Lastimo al jugador: queda mirando hacia el (centerX del jugador).
    void facePlayer(float playerCenterX);

    // Caja de choque con el jugador (ver arriba).
    SDL_FRect hitBox() const { return SDL_FRect{x_ + 3.0f, y_ + 6.0f, 10.0f, 6.0f}; }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    enum class State { Inactive, Walking, Stomped, Defeated };

    static constexpr float kSpeed = 30.0f;                 // 8/16 px por cuadro
    static constexpr float kGravity = 858.0f;              // 0x3d/256 px/cuadro^2
    static constexpr float kMaxFallSpeed = 180.0f;         // 3 px por cuadro
    static constexpr float kStompedDuration = 0.6f;        // ~2 tics de 21 cuadros
    static constexpr float kDefeatHop = -180.0f;           // -3 px por cuadro
    static constexpr float kDefeatSpeedX = 60.0f;          // 1 px por cuadro
    static constexpr float kWaddleInterval = 8.0f / 60.0f; // FrameCounter & 8
    static constexpr float kEdgeEpsilon = 0.01f;

    static inline float sWaddleClock = 0.0f;

    void moveHorizontally(float dt, const Tilemap& level);
    void moveVertically(float dt, const Tilemap& level);

    State state_ = State::Inactive;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    float stompedTimer_ = 0.0f;
};
