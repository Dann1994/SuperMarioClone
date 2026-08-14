#include "Fireball.h"

#include <cmath>

void Fireball::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kSpeed : -kSpeed;
    velY_ = kBounceVelocity * 0.5f;  // primer salto chico al salir de la mano
    alive_ = true;
    lifetime_ = kMaxLifetime;
    animTimer_ = 0.0f;
    frame_ = 0;
    exploding_ = false;
    explosionTimer_ = 0.0f;
}

void Fireball::explode() {
    if (!alive_ || exploding_) return;
    exploding_ = true;
    explosionTimer_ = kExplosionFrameDuration * kExplosionFrameCount;
}

void Fireball::update(float dt, const Tilemap& level, int screenWidth) {
    if (!alive_) return;

    if (exploding_) {
        explosionTimer_ -= dt;
        if (explosionTimer_ <= 0.0f) {
            alive_ = false;
        }
        return;
    }

    lifetime_ -= dt;
    if (lifetime_ <= 0.0f) {
        alive_ = false;
        return;
    }

    // Sin camara todavia, la pantalla visible es directamente [0, screenWidth)
    // en coordenadas de mundo. Una vez que sale de ahi, desaparece (sin
    // explosion: no choco contra nada, simplemente se fue de la vista).
    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth)) {
        alive_ = false;
        return;
    }

    animTimer_ += dt * 15.0f;
    frame_ = static_cast<int>(animTimer_) % 4;

    const int ts = level.tileSize();

    // --- Eje X: choca con pared y explota ---
    const float newX = x_ + velX_ * dt;
    {
        const float edgeX = velX_ > 0.0f ? newX + kSize - kEdgeEpsilon : newX;
        const int col = static_cast<int>(std::floor(edgeX / ts));
        const int row0 = static_cast<int>(std::floor(y_ / ts));
        const int row1 = static_cast<int>(std::floor((y_ + kSize - kEdgeEpsilon) / ts));
        for (int row = row0; row <= row1; ++row) {
            if (level.isSolidTile(col, row)) {
                explode();
                return;
            }
        }
    }
    x_ = newX;

    // --- Eje Y: rebota contra el piso ---
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

    // Se cayo por un pozo: la matamos en vez de dejarla caer para siempre.
    if (y_ > level.heightPixels()) {
        alive_ = false;
    }
}

void Fireball::render(SDL_Renderer* renderer, const SpriteSheet& projectileSprite,
                       const SpriteSheet& explosionSprite, float cameraX) const {
    if (!alive_) return;

    if (exploding_) {
        const float elapsed = kExplosionFrameDuration * kExplosionFrameCount - explosionTimer_;
        int explosionFrame = static_cast<int>(elapsed / kExplosionFrameDuration);
        if (explosionFrame >= kExplosionFrameCount) explosionFrame = kExplosionFrameCount - 1;
        const int offset = (kExplosionSize - kSize) / 2;
        explosionSprite.draw(renderer, 0, explosionFrame,
                              static_cast<int>(std::lround(x_ - cameraX)) - offset,
                              static_cast<int>(std::lround(y_)) - offset, false);
        return;
    }

    projectileSprite.draw(renderer, 0, frame_, static_cast<int>(std::lround(x_ - cameraX)), static_cast<int>(std::lround(y_)), false);
}
