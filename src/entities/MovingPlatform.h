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
//  - Vertical / Horizontal: va y viene a lo largo de `range` px desde donde
//    arranca (para abajo / para la derecha), frenando suave en las puntas.
//  - LiftUp / LiftDown: elevador que sube/baja siempre a velocidad fija y al
//    salir por un borde de la pantalla vuelve a entrar por el otro.
//  - Fall: quieta hasta que alguien la pisa, ahi se cae acelerando.
//
// Sprite: assets/sprites/lift.png (tramo de 8x8 que se repite a lo ancho,
// tools/asset_scripts/gen_castle.py).
enum class PlatformMode {
    Vertical,
    Horizontal,
    LiftUp,
    LiftDown,
    Fall,
};

class MovingPlatform {
public:
    // range: recorrido en px (Vertical/Horizontal). period: segundos de una
    // ida y vuelta completa (Vertical/Horizontal). speed: px/s (LiftUp/LiftDown).
    void spawn(float x, float y, int widthPx, PlatformMode mode, float range, float period, float speed);
    void reset() { active_ = false; }
    // levelHeight: alto del nivel en px, para el ciclo de los elevadores.
    void update(float dt, int levelHeight);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX = 0.0f) const;

    // Alguien se paro encima: la que cae arranca a caer.
    void onStoodOn();

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
    float dx_ = 0.0f;
    float dy_ = 0.0f;
    bool active_ = false;
};
