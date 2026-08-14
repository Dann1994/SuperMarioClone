#include "Hammer.h"

#include <algorithm>
#include <cmath>

void Hammer::spawn(float x, float y, float velX, float velY) {
    x_ = x;
    y_ = y;
    velX_ = velX;
    velY_ = velY;
    alive_ = true;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void Hammer::update(float dt, const Tilemap& level, int screenWidth, int screenHeight) {
    if (!alive_) return;

    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth)) {
        alive_ = false;
        return;
    }

    animTimer_ += dt;
    if (animTimer_ >= kSpinFrameDuration) {
        animTimer_ -= kSpinFrameDuration;
        animFrame_ = (animFrame_ + 1) % 4;
    }

    // Sin colision con el piso ni las paredes (ver el comentario de la
    // clase): pura parabola por gravedad, atraviesa cualquier tile solido
    // como si no estuviera. Solo se lo saca de encima si sale de la
    // pantalla, ver mas abajo.
    x_ += velX_ * dt;
    velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
    y_ += velY_ * dt;

    if (y_ > static_cast<float>(screenHeight)) {
        alive_ = false;
    }
}

void Hammer::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    sprite.draw(renderer, worldRow, animFrame_, static_cast<int>(std::lround(x_ - cameraX)),
                static_cast<int>(std::lround(y_)), false);
}
