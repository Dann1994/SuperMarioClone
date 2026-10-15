#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Plataforma movil (seccion 5.3, "plataformas moviles / elevadores"): se
// pisa desde arriba y se atraviesa desde abajo, como en el original. Los
// personajes que estan parados encima se mueven con ella (eso lo resuelve
// Game::updatePlatforms/resolvePlatformLanding, esta clase solo se mueve y
// avisa cuanto se movio el ultimo cuadro).
//
// Modos (campo "mode" de "platforms" en el JSON del nivel):
//  - Vertical (YMovingPlatform del original): se hamaca alrededor de un
//    centro 64 px mas abajo de donde aparece (64 px mas arriba si aparece
//    en la mitad de abajo de la pantalla), acelerando 5/256 px por cuadro^2.
//  - Horizontal (XMovingPlatform): cada 4 cuadros su velocidad sube de a
//    1/16 px hasta 14/16 y vuelve a 0; arranca hacia la izquierda.
//  Todas arrancan recien cuando el original las haria aparecer (start(),
//  lo decide Game): por eso dos plataformas vecinas no van sincronizadas.
//  - LiftUp / LiftDown: elevador que sube/baja siempre a velocidad fija y al
//    salir por un borde de la pantalla vuelve a entrar por el otro.
//  - Fall: quieta hasta que alguien la pisa, ahi se cae acelerando.
//  - Right: la de las zonas del cielo (RightPlatform del original, objeto
//    0x2A): quieta hasta que alguien la pisa; desde ahi avanza a la derecha
//    a 1 px por cuadro y no para mas.
//  - Balance: una de las dos de una balanza; la mueve Game (driveY), ver
//    Game::updateBalances.
//
// Sprite: assets/sprites/lift.png (tramo de 8x8 que se repite a lo ancho,
// tools/asset_scripts/gen_castle.py).
enum class PlatformMode {
    Vertical,
    Horizontal,
    LiftUp,
    LiftDown,
    Fall,
    Balance,
    Right,
};

class MovingPlatform {
public:
    // range: recorrido en px (Vertical/Horizontal). period: segundos de una
    // ida y vuelta completa (Vertical/Horizontal). speed: px/s (LiftUp/LiftDown).
    void spawn(float x, float y, int widthPx, PlatformMode mode, float range, float period, float speed);
    void reset() { active_ = false; }
    // Empieza a moverse (el original la hace aparecer recien ahi).
    void start() { started_ = true; }
    bool started() const { return started_; }
    // levelHeight: alto del nivel en px, para el ciclo de los elevadores.
    void update(float dt, int levelHeight);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX = 0.0f) const;

    // Alguien se paro encima: la que cae arranca a caer.
    void onStoodOn();
    // Balance: cuanto se mueve en el proximo update() (lo decide Game).
    void driveY(float dy) { pendingDy_ += dy; }

    bool isActive() const { return active_; }
    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return widthPx_; }
    // Cuanto se movio en el ultimo update(), para arrastrar a los que van
    // parados encima.
    float lastDx() const { return dx_; }
    float lastDy() const { return dy_; }

    static constexpr int kHeight = 8;

private:
    static constexpr float kFallGravity = 600.0f;   // px/s^2
    static constexpr float kMaxFallSpeed = 240.0f;  // px/s

    float x_ = 0.0f;
    float y_ = 0.0f;
    float baseX_ = 0.0f;
    float baseY_ = 0.0f;
    int widthPx_ = 48;
    PlatformMode mode_ = PlatformMode::Vertical;
    float range_ = 0.0f;
    float period_ = 4.0f;
    float speed_ = 0.0f;
    float phase_ = 0.0f;
    float fallVelocity_ = 0.0f;
    bool falling_ = false;
    bool movingRight_ = false;   // Right: ya la pisaron
    static constexpr float kRightSpeed = 60.0f;   // 0x10 = 1 px por cuadro
    float dx_ = 0.0f;
    float dy_ = 0.0f;
    float pendingDy_ = 0.0f;
    bool active_ = false;
    bool started_ = false;
    // Movimiento por cuadro del original (Vertical / Horizontal).
    float frameAccum_ = 0.0f;
    unsigned frames_ = 0;
    float centerY_ = 0.0f;
    float velY_ = 0.0f;          // px por cuadro
    int xPrimary_ = 0;           // XMovePrimaryCounter
    int xSecondary_ = 0;         // XMoveSecondaryCounter (1/16 px por cuadro)
};
