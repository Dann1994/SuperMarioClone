#include "Fireball.h"

#include <cmath>

void Fireball::spawn(float x, float y, bool movingRight) {
    hammer_ = false;
    boomerang_ = false;
    movingRight_ = movingRight;
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kSpeed : -kSpeed;
    // Sin "saltito" al salir de la mano (asi estaba antes): en el original
    // arranca cayendo de una, se nota el arco recien cuando rebota.
    velY_ = kInitialFallSpeed;
    alive_ = true;
    lifetime_ = kMaxLifetime;
    animTimer_ = 0.0f;
    frame_ = 0;
    exploding_ = false;
    explosionTimer_ = 0.0f;
}

void Fireball::spawnHammer(float x, float y, bool movingRight) {
    spawn(x, y, movingRight);
    hammer_ = true;
    hitBowser_ = false;
    movingRight_ = movingRight;
    velX_ = movingRight ? kHammerSpeedX : -kHammerSpeedX;
    velY_ = kHammerSpeedY;
}

void Fireball::spawnBoomerang(float x, float y, bool movingRight) {
    spawnHammer(x, y, movingRight);
    boomerang_ = true;
    boomerangTime_ = 0.0f;
    boomerangStartX_ = x;
    boomerangStartY_ = y;
    lifetime_ = kBoomerangLifetime;
}

void Fireball::explode() {
    if (!alive_ || exploding_) return;
    if (hammer_) return;   // el martillo no explota: sigue de largo
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

    // screenWidth: pese al nombre, Game.cpp le pasa el ANCHO DEL NIVEL
    // (level.widthPixels()), no el de la pantalla visible -- en un nivel con
    // scroll la pantalla visible se mueve con la camara, pero esto solo
    // tiene que desaparecer cuando sale del nivel entero (por cualquiera de
    // los dos bordes), no cuando queda fuera de la pantalla actual. Sin
    // explosion: no choco contra nada, simplemente se fue de la vista.
    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth)) {
        alive_ = false;
        return;
    }

    animTimer_ += dt * 15.0f;
    frame_ = static_cast<int>(animTimer_) % 4;

    if (boomerang_) {
        // Ida y vuelta por el mismo camino, sin chocar con nada (ver spawnBoomerang).
        boomerangTime_ += dt;
        const float t = boomerangTime_;
        const float dir = movingRight_ ? 1.0f : -1.0f;
        x_ = boomerangStartX_ + dir * (kBoomerangSpeed * t - kBoomerangDecel * t * t / 2.0f);
        y_ = boomerangStartY_ - kBoomerangLift * std::sin(3.14159265f * std::min(t / (2.0f * kBoomerangTurnTime), 1.0f));
        return;
    }
    if (hammer_) {
        // Arco sin chocar con nada; se va al caer por debajo del nivel.
        velY_ += kHammerGravity * dt;
        x_ += velX_ * dt;
        y_ += velY_ * dt;
        if (y_ > level.heightPixels()) alive_ = false;
        return;
    }

    const int ts = level.tileSize();
    // El solido invisible ('X') no frena el fuego: lo atraviesa.
    auto solid = [&level](int col, int row) {
        return level.isSolidTile(col, row) && !level.isInvisibleSolidTile(col, row);
    };

    // --- Eje X: choca con pared y explota ---
    const float newX = x_ + velX_ * dt;
    {
        const float edgeX = velX_ > 0.0f ? newX + kSize - kEdgeEpsilon : newX;
        const int col = static_cast<int>(std::floor(edgeX / ts));
        const int row0 = static_cast<int>(std::floor(y_ / ts));
        const int row1 = static_cast<int>(std::floor((y_ + kSize - kEdgeEpsilon) / ts));
        for (int row = row0; row <= row1; ++row) {
            if (solid(col, row)) {
                explode();
                wallHitRequested_ = true;
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
            if (solid(col, row)) { blocked = true; break; }
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
                       const SpriteSheet& explosionSprite, float cameraX, const SpriteSheet* hammerSprite,
                       const SpriteSheet* boomerangSprite) const {
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

    if (boomerang_ && boomerangSprite) {
        // 16x16 centrado en la caja de 8 (como el martillo), girando.
        const int offset = (kHammerSpriteSize - kSize) / 2;
        boomerangSprite->draw(renderer, 0, frame_, static_cast<int>(std::lround(x_ - cameraX)) - offset,
                              static_cast<int>(std::lround(y_)) - offset, !movingRight_);
        return;
    }
    if (hammer_ && hammerSprite) {
        // El sprite del martillo (16x16, girando) centrado en la caja de 8x8.
        const int offset = (kHammerSpriteSize - kSize) / 2;
        hammerSprite->draw(renderer, 0, frame_, static_cast<int>(std::lround(x_ - cameraX)) - offset,
                           static_cast<int>(std::lround(y_)) - offset, !movingRight_);
        return;
    }
    projectileSprite.draw(renderer, 0, frame_, static_cast<int>(std::lround(x_ - cameraX)), static_cast<int>(std::lround(y_)), false);
}
