#include "Goomba.h"

#include <algorithm>
#include <cmath>

void Goomba::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kSpeed : -kSpeed;
    velY_ = 0.0f;
    alive_ = true;
    squished_ = false;
    squishTimer_ = 0.0f;
    falling_ = false;
    walkAnimTimer_ = 0.0f;
    walkFrame_ = 0;
}

void Goomba::squish() {
    if (!alive_ || squished_ || falling_) return;
    squished_ = true;
    squishTimer_ = kSquishDuration;
    velX_ = 0.0f;
    velY_ = 0.0f;
}

void Goomba::killFalling(bool flyRight) {
    if (!alive_ || squished_ || falling_) return;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
}

void Goomba::update(float dt, const Tilemap& level, int screenHeight) {
    if (!alive_) return;

    if (falling_) {
        // Parabola (empuje horizontal constante + salto inicial que la
        // gravedad frena y revierte, como una "U" invertida), sin colisionar
        // con nada (ni paredes ni piso): la idea es que se vea "muerto"
        // saliendo despedido fuera del nivel, no que siga interactuando con
        // el escenario.
        velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
        x_ += velX_ * dt;
        y_ += velY_ * dt;
        if (y_ > screenHeight) {
            alive_ = false;
        }
        return;
    }

    if (squished_) {
        squishTimer_ -= dt;
        if (squishTimer_ <= 0.0f) {
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

    // --- Eje X: rebota contra paredes (invierte direccion, no se cae ni gira en bordes) ---
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

    // --- Eje Y: gravedad y piso ---
    velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
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
            velY_ = 0.0f;
        }
    }
    y_ = newY;

    if (y_ > level.heightPixels()) {
        alive_ = false;
    }
}

void Goomba::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_));
    const int drawY = static_cast<int>(std::lround(y_));
    if (falling_) {
        // Boca abajo, frame de caminata quieto (no tiene un frame "muerto"
        // propio recortado del sheet, ver Documentaciones/Enemigos.png).
        // Flip horizontal segun hacia donde sale despedido (velX_).
        sprite.draw(renderer, worldRow, 0, drawX, drawY, velX_ < 0.0f, true);
        return;
    }
    const int col = squished_ ? 2 : walkFrame_;
    sprite.draw(renderer, worldRow, col, drawX, drawY, false);
}
