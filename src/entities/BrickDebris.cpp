#include "BrickDebris.h"

#include <cmath>

void BrickDebris::spawn(float x, float y, float velX, float velY) {
    x_ = x;
    y_ = y;
    velX_ = velX;
    velY_ = velY;
    alive_ = true;
    lifetime_ = kMaxLifetime;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void BrickDebris::update(float dt, int screenWidth, int screenHeight) {
    if (!alive_) return;

    lifetime_ -= dt;
    if (lifetime_ <= 0.0f) {
        alive_ = false;
        return;
    }

    animTimer_ += dt;
    if (animTimer_ >= kSpinFrameDuration) {
        animTimer_ -= kSpinFrameDuration;
        animFrame_ = (animFrame_ + 1) % 4;
    }

    x_ += velX_ * dt;
    velY_ += kGravity * dt;
    y_ += velY_ * dt;

    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth) || y_ > static_cast<float>(screenHeight)) {
        alive_ = false;
    }
}

void BrickDebris::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const {
    if (!alive_) return;
    sprite.draw(renderer, worldRow, animFrame_, static_cast<int>(std::lround(x_)),
                static_cast<int>(std::lround(y_)), false);
}
