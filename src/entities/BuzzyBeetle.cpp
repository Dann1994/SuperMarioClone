#include "BuzzyBeetle.h"

#include <algorithm>
#include <cmath>

void BuzzyBeetle::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kWalkSpeed : -kWalkSpeed;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
    state_ = BuzzyBeetleState::Walking;
    lastFacingRight_ = movingRight;
    shellIdleTimer_ = 0.0f;
    postStompCooldown_ = 0.0f;
    stompCooldownOwner_ = nullptr;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void BuzzyBeetle::reset() {
    alive_ = false;
    falling_ = false;
    state_ = BuzzyBeetleState::Walking;
    postStompCooldown_ = 0.0f;
    stompCooldownOwner_ = nullptr;
}

void BuzzyBeetle::stomp(const void* character) {
    if (!alive_ || falling_) return;

    if (state_ == BuzzyBeetleState::Walking) {
        state_ = BuzzyBeetleState::ShellIdle;
        shellIdleTimer_ = kShellIdleDuration;
        velX_ = 0.0f;
        velY_ = 0.0f;
        animTimer_ = 0.0f;
        animFrame_ = 0;
    } else if (state_ == BuzzyBeetleState::ShellMoving) {
        state_ = BuzzyBeetleState::ShellIdle;
        shellIdleTimer_ = kShellIdleDuration;
        velX_ = 0.0f;
        postStompCooldown_ = kPostStompCooldown;
        stompCooldownOwner_ = character;
    }
    // ShellIdle: pisarlo de nuevo no hace nada (el llamador igual hace
    // rebotar al personaje).
}

void BuzzyBeetle::kick(bool kickRight, const void* character) {
    if (!alive_ || falling_ || state_ != BuzzyBeetleState::ShellIdle) return;
    state_ = BuzzyBeetleState::ShellMoving;
    velX_ = kickRight ? kShellSpeed : -kShellSpeed;
    lastFacingRight_ = kickRight;
    postStompCooldown_ = kPostStompCooldown;
    stompCooldownOwner_ = character;
}

void BuzzyBeetle::killFalling(bool flyRight) {
    if (!alive_ || falling_) return;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void BuzzyBeetle::update(float dt, const Tilemap& level, int screenHeight) {
    if (!alive_) return;

    if (postStompCooldown_ > 0.0f) {
        postStompCooldown_ = std::max(0.0f, postStompCooldown_ - dt);
    }

    if (falling_) {
        velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
        x_ += velX_ * dt;
        y_ += velY_ * dt;
        if (y_ > screenHeight) {
            alive_ = false;
        }
        return;
    }

    if (state_ == BuzzyBeetleState::ShellIdle) {
        shellIdleTimer_ -= dt;
        if (shellIdleTimer_ <= 0.0f) {
            // Sale solo: retoma la caminata en la direccion que llevaba
            // antes de esconderse. Un solo frame de caparazon en la hoja de
            // referencia, asi que a diferencia del Koopa no hay parpadeo de
            // aviso previo (ver el comentario de la clase).
            state_ = BuzzyBeetleState::Walking;
            velX_ = lastFacingRight_ ? kWalkSpeed : -kWalkSpeed;
            animTimer_ = 0.0f;
            animFrame_ = 0;
        }
        // Aunque este quieto en X, sigue cayendo si lo patean al vacio
        // (mismo criterio que Koopa::ShellIdle).
        const int ts = level.tileSize();
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
        return;
    }

    if (state_ == BuzzyBeetleState::Walking) {
        animTimer_ += dt;
        if (animTimer_ >= kWalkFrameDuration) {
            animTimer_ -= kWalkFrameDuration;
            animFrame_ = (animFrame_ + 1) % 2;
        }
    } else {
        // ShellMoving: un solo frame quieto, no "gira".
        animFrame_ = 0;
    }

    const int ts = level.tileSize();

    // --- Eje X: rebota contra paredes, se cae de los bordes (nunca les
    // tiene miedo, a diferencia del Koopa rojo -- no existe variante roja
    // de Buzzy Beetle en el original) ---
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
            if (velX_ != 0.0f) lastFacingRight_ = velX_ > 0.0f;
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

void BuzzyBeetle::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const {
    if (!alive_) return;

    int col;
    if (falling_ || state_ != BuzzyBeetleState::Walking) {
        col = 2;  // caparazon: unico frame disponible en la hoja de referencia
    } else {
        col = animFrame_;  // walk1/walk2
    }

    const int drawX = static_cast<int>(std::lround(x_));
    const int drawY = static_cast<int>(std::lround(y_));
    // El arte fuente mira a la izquierda por defecto (ver
    // Documentaciones/Enemigos.png, mismo criterio que el Koopa): se
    // flipea al reves que Mario/Goomba, mirando a la derecha SI se flipea,
    // no si no.
    sprite.draw(renderer, worldRow, col, drawX, drawY, lastFacingRight_, falling_);
}
