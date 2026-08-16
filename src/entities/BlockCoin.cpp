#include "BlockCoin.h"

#include <algorithm>
#include <cmath>

void BlockCoin::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    velY_ = kJumpImpulse;
    alive_ = true;
    animTimer_ = 0.0f;
    frame_ = 0;
}

void BlockCoin::update(float dt) {
    if (!alive_) return;

    velY_ += kGravity * dt;
    y_ += velY_ * dt;
    // Ya paso la cima (empezo a bajar): desaparece ahi, no cae de vuelta
    // (ver el comentario de la clase).
    if (velY_ >= 0.0f) {
        alive_ = false;
        return;
    }

    animTimer_ += dt;
    if (animTimer_ >= kFrameDuration) {
        animTimer_ -= kFrameDuration;
        frame_ = (frame_ + 1) % 4;
    }
}

void BlockCoin::render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    sprite.draw(renderer, 0, frame_, drawX, drawY, false);
}
