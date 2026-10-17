#include "Spiny.h"

#include "Difficulty.h"

#include <algorithm>
#include <cmath>

void Spiny::spawn(float x, float y, float velX, float velY) {
    x_ = x;
    y_ = y;
    velX_ = velX;
    velY_ = velY;
    alive_ = true;
    falling_ = false;
    state_ = SpinyState::Egg;
    lastFacingRight_ = velX >= 0.0f;
    walkAnimTimer_ = 0.0f;
    walkFrame_ = 0;
}

void Spiny::reset() {
    alive_ = false;
    falling_ = false;
}

bool Spiny::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    return true;
}

void Spiny::update(float dt, const Tilemap& level, int screenHeight) {
    if (!alive_) return;

    if (falling_) {
        velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
        x_ += velX_ * dt;
        y_ += velY_ * dt;
        if (y_ > screenHeight) {
            alive_ = false;
        }
        return;
    }

    walkAnimTimer_ += dt;
    if (walkAnimTimer_ >= kWalkFrameDuration) {
        walkAnimTimer_ -= kWalkFrameDuration;
        walkFrame_ = (walkFrame_ + 1) % 2;
    }

    const int ts = level.tileSize();

    if (state_ == SpinyState::Walking) {
        // Rebota contra pared, SIN miedo a los bordes -- mismo criterio que
        // el Goomba (ver el comentario de la clase, a diferencia del Koopa
        // rojo).
        const float newX = x_ + velX_ * dt;
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
            lastFacingRight_ = velX_ > 0.0f;
        } else {
            x_ = newX;
        }
    } else if (velX_ != 0.0f) {
        // Egg: solo deriva horizontal, sin rebotar contra pared (ver el
        // comentario de la clase).
        x_ += velX_ * dt;
    }

    velY_ = std::min(velY_ + (state_ == SpinyState::Egg ? kEggGravity : kGravity) * dt, kMaxFallSpeed);
    float newY = y_ + velY_ * dt;
    if (velY_ > 0.0f) {
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kSize - kEdgeEpsilon) / ts));
        const int row = static_cast<int>(std::floor((newY + kSize - kEdgeEpsilon) / ts));
        bool blocked = false;
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row) || level.isPlatformTile(col, row)) { blocked = true; break; }
        }
        if (blocked) {
            newY = static_cast<float>(row * ts - kSize);
            velY_ = 0.0f;
            if (state_ == SpinyState::Egg) {
                // Eclosiona: pasa a caminar con la direccion que ya traia
                // (o hacia donde estaba mirando, si cayo derecho sin deriva).
                state_ = SpinyState::Walking;
                // Como el resto de los que caminan (InitNormalEnemy): en la
                // segunda vuelta, mas rapido.
                const float speed = kWalkSpeed * hardWalkFactor();
                velX_ = lastFacingRight_ ? speed : -speed;
            }
        }
    }
    y_ = newY;

    if (y_ > level.heightPixels()) {
        alive_ = false;
    }
}

void Spiny::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    if (falling_) {
        // Boca abajo, frame de caminata quieto (no tiene un frame "muerto"
        // propio recortado del sheet, mismo criterio que Goomba/HammerBro).
        sprite.draw(renderer, worldRow, 2, drawX, drawY, velX_ < 0.0f, true);
        return;
    }
    // Columnas del sheet (ver assets/sprites/spiny.png): 0/1 = bola
    // enrollada girando (Egg), 2/3 = caminando con las pas afuera (Walking).
    const int col = (state_ == SpinyState::Egg ? 0 : 2) + walkFrame_;
    // El arte fuente mira a la izquierda por defecto (mismo criterio que el
    // resto del elenco): se flipea mirando a la derecha.
    sprite.draw(renderer, worldRow, col, drawX, drawY, lastFacingRight_);
}
