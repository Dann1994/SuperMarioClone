#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Moneda (seccion 5.3): no se mueve, solo cicla sus 3 frames de giro hasta
// que la agarran. El sheet trae una fila por tipo de mundo (mismo orden que
// WorldType), la fila la elige quien la crea segun el ambiente actual.
class Coin {
public:
    void spawn(float x, float y);
    void update(float dt);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const;
    bool isAlive() const { return alive_; }
    void collect() { alive_ = false; }

    float x() const { return x_; }
    float y() const { return y_; }
    static constexpr int kSize = 16;

private:
    static constexpr float kFrameDuration = 0.12f;  // s por frame del giro
    static constexpr int kFrameCount = 3;

    float x_ = 0.0f;
    float y_ = 0.0f;
    bool alive_ = false;
    float animTimer_ = 0.0f;
    int frame_ = 0;
};
