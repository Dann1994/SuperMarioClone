#include "FireFlower.h"

#include <cmath>

void FireFlower::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    alive_ = true;
    animTimer_ = 0.0f;
    frame_ = 0;
}

void FireFlower::update(float dt) {
    if (!alive_) return;
    animTimer_ += dt;
    while (animTimer_ >= kFrameDuration) {
        animTimer_ -= kFrameDuration;
        frame_ = (frame_ + 1) % kFrameCount;
    }
}

void FireFlower::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    sprite.draw(renderer, worldRow, frame_, static_cast<int>(std::lround(x_ - cameraX)), static_cast<int>(std::lround(y_)), false);
}
