#include "StarItem.h"

#include <cmath>

void StarItem::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kSpeed : -kSpeed;
    velY_ = kBounceVelocity;
    alive_ = true;
    animTimer_ = 0.0f;
    frame_ = 0;
}

void StarItem::update(float dt, const Tilemap& level, int screenWidth) {
    if (!alive_) return;

    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth)) {
        alive_ = false;
        return;
    }

    animTimer_ += dt;
    while (animTimer_ >= kFrameDuration) {
        animTimer_ -= kFrameDuration;
        frame_ = (frame_ + 1) % kFrameCount;
    }

    const int ts = level.tileSize();

    // --- Eje X: rebota contra paredes (invierte direccion) ---
    const float newX = x_ + velX_ * dt;
    {
        const float edgeX = velX_ > 0.0f ? newX + kSize - kEdgeEpsilon : newX;
        const int col = static_cast<int>(std::floor(edgeX / ts));
        const int row0 = static_cast<int>(std::floor(y_ / ts));
        const int row1 = static_cast<int>(std::floor((y_ + kSize - kEdgeEpsilon) / ts));
        bool blocked = false;
        for (int row = row0; row <= row1; ++row) {
            if (level.isSolidTile(col, row)) { blocked = true; break; }
        }
        if (blocked) {
            velX_ = -velX_;
        } else {
            x_ = newX;
        }
    }

    // --- Eje Y: rebota contra el piso con saltitos ---
    velY_ += kGravity * dt;
    float newY = y_ + velY_ * dt;
    if (velY_ > 0.0f) {
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kSize - kEdgeEpsilon) / ts));
        const int row = static_cast<int>(std::floor((newY + kSize - kEdgeEpsilon) / ts));
        bool blocked = false;
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row)) { blocked = true; break; }
        }
        if (blocked) {
            newY = static_cast<float>(row * ts - kSize);
            velY_ = kBounceVelocity;
        }
    }
    y_ = newY;

    if (y_ > level.heightPixels()) {
        alive_ = false;
    }
}

void StarItem::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const {
    if (!alive_) return;
    sprite.draw(renderer, worldRow, frame_, static_cast<int>(std::lround(x_)), static_cast<int>(std::lround(y_)), false);
}
