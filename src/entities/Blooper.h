#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Blooper (calamar), igual que el original (InitBloober / MoveBloober /
// ProcSwimmingB de la descompilacion SMB-Vanilla, reimplementado):
//
//  - No choca con el escenario: pasa a traves del coral y los bloques.
//  - Ciclo de tres fases (BlooperMoveCounter):
//      1. Envion: cada 8 cuadros la velocidad sube 1 (hasta 2 px/cuadro).
//      2. Frena: cada 8 cuadros baja 1 hasta 0; ahi arranca una espera
//         minima de 2 intervalos (EnemyIntervalTimer, de a 21 cuadros).
//      3. Se hunde 1 px cada 2 cuadros. Pasada la espera, vuelve al envion
//         en cuanto el jugador esta a su altura o mas arriba
//         (Player_Y <= Enemy_Y + 0x10).
//    Durante el envion sube esa velocidad por cuadro (sin pasar de Y 0x20
//    de pantalla) y avanza de costado la misma velocidad.
//  - Direccion: de vez en cuando (1 de cada 64 cuadros; 1 de cada 4 en el
//    modo dificil) se re-apunta: los de lugar impar toman la direccion en
//    la que se mueve el jugador, los de lugar par van hacia el.
//  - Derrotado: cae boca abajo con la caida lenta (MoveEnemySlowVert).
//
// La logica corre por cuadro (60 por segundo), como el original.
class Blooper {
public:
    void spawn(float x, float y);
    void reset();
    // targetX: x del jugador mas cercano; targetFeetY: sus pies;
    // targetMovingRight: hacia donde se mueve; slot: lugar en el pool.
    void update(float dt, float targetX, float targetFeetY, bool targetMovingRight, int slot,
                int screenHeight);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    bool isDangerous() const { return alive_ && !falling_; }

    bool killFalling(bool flyRight);

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 24;

private:
    void runFrame(float targetX, float targetFeetY, bool targetMovingRight, int slot, int screenHeight);

    // Tope de arriba: Y 0x20 de la pantalla del original (+16 de nuestra
    // fila de mas).
    static constexpr float kTopY = 0x20 + 16.0f;
    static constexpr float kSlowGravity = 15.0f / 256.0f;  // px/cuadro^2 (0x0f), derrotado
    static constexpr float kMaxFallSpeed = 2.0f;           // px/cuadro

    float x_ = 0.0f;
    float y_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    bool movingRight_ = false;
    int moveCounter_ = 0;      // BlooperMoveCounter
    int speed_ = 0;            // Enemy_Y_MoveForce / BlooperMoveSpeed (px/cuadro)
    int intervalTimer_ = 0;    // EnemyIntervalTimer (intervalos de 21 cuadros)
    int intervalFrames_ = 0;
    unsigned frameCounter_ = 0;
    float frameAccumulator_ = 0.0f;
    float fallSpeed_ = 0.0f;
};
