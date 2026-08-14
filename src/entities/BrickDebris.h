#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Un pedazo del ladrillo que se rompe (seccion 5.3, ver Game::checkBrickBlockHit):
// sale despedido en diagonal por gravedad, girando, hasta salir de la
// pantalla o cumplir su tiempo de vida -- puramente decorativo, no
// colisiona con nada (ni el escenario ni los personajes). Cuatro instancias
// por bloque roto (una por esquina, ver Game::breakBrickBlock), reusando el
// mismo sprite de giro para las cuatro (Documentaciones/Objetos.png).
class BrickDebris {
public:
    void spawn(float x, float y, float velX, float velY);
    void reset() { alive_ = false; }
    void update(float dt, int screenWidth, int screenHeight);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }

    static constexpr int kSize = 8;

private:
    static constexpr float kGravity = 700.0f;
    static constexpr float kSpinFrameDuration = 0.05f;
    static constexpr float kMaxLifetime = 1.2f;  // por si nunca sale de la pantalla

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    float lifetime_ = 0.0f;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
