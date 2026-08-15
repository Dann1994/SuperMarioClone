#include "Axe.h"

#include <cmath>

void Axe::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    alive_ = true;
}

void Axe::render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    sprite.draw(renderer, 0, 0, drawX, drawY, false);
}
