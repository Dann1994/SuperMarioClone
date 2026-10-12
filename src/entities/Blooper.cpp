#include "Blooper.h"

#include <algorithm>
#include <cmath>

void Blooper::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    velX_ = 0.0f;
    velY_ = kFloatDownSpeed;
    alive_ = true;
    falling_ = false;
    lastFacingRight_ = true;
    state_ = BlooperState::Resting;
    phaseTimer_ = kRestDuration;
}

void Blooper::reset() {
    alive_ = false;
    falling_ = false;
}

bool Blooper::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    return true;
}

void Blooper::backToResting() {
    state_ = BlooperState::Resting;
    phaseTimer_ = kRestDuration;
    velX_ = 0.0f;
    velY_ = kFloatDownSpeed;
}

void Blooper::applyHorizontalWithWallBounce(float dt, const Tilemap& level) {
    if (velX_ == 0.0f) return;
    const int ts = level.tileSize();
    const float newX = x_ + velX_ * dt;
    const float edgeX = velX_ > 0.0f ? newX + kWidth - kEdgeEpsilon : newX;
    const int col = static_cast<int>(std::floor(edgeX / ts));
    const int row0 = static_cast<int>(std::floor(y_ / ts));
    const int row1 = static_cast<int>(std::floor((y_ + kHeight - kEdgeEpsilon) / ts));
    bool blocked = false;
    for (int row = row0; row <= row1; ++row) {
        if (level.isSolidTile(col, row)) { blocked = true; break; }
    }
    if (blocked) {
        if (state_ == BlooperState::Bursting) backToResting();
    } else {
        x_ = newX;
    }
}

bool Blooper::isNearFloor(const Tilemap& level) const {
    const int ts = level.tileSize();
    const int col0 = static_cast<int>(std::floor(x_ / ts));
    const int col1 = static_cast<int>(std::floor((x_ + kWidth - kEdgeEpsilon) / ts));
    const int rowStart = static_cast<int>(std::floor((y_ + kHeight) / ts));
    const int rowEnd = static_cast<int>(std::floor((y_ + kHeight + kFloorAvoidDistance) / ts));
    for (int row = rowStart; row <= rowEnd; ++row) {
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row)) return true;
        }
    }
    return false;
}

bool Blooper::applyVerticalWithFloorCollision(float dt, const Tilemap& level) {
    const int ts = level.tileSize();
    const float newY = y_ + velY_ * dt;
    if (velY_ > 0.0f) {
        // Respaldo de colision contra el piso (ver el comentario de la
        // clase): el original no lo necesita porque no hay piso en un nivel
        // de agua real, aca si puede haber uno (test_blooper.json).
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kWidth - kEdgeEpsilon) / ts));
        const int row = static_cast<int>(std::floor((newY + kHeight - kEdgeEpsilon) / ts));
        bool blocked = false;
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row)) { blocked = true; break; }
        }
        if (blocked) {
            y_ = static_cast<float>(row * ts - kHeight);
            velY_ = 0.0f;
            return true;
        }
    }
    y_ = newY;
    return false;
}

void Blooper::update(float dt, const Tilemap& level, float targetX, float targetY, int screenHeight) {
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

    // Ver el comentario de la clase: Resting/WindingUp hunden derecho sin
    // velocidad horizontal, solo Bursting se mueve en diagonal.
    phaseTimer_ -= dt;
    switch (state_) {
        case BlooperState::Resting:
            // Si el piso esta cerca no espera el timer (ver el comentario
            // de la clase): pasa a WindingUp de una para tener tiempo de
            // frenar antes de tocarlo.
            // Como el original: se hunde hacia el jugador y recien se
            // impulsa cuando el jugador esta a su altura o mas arriba (o si
            // el piso quedo cerca).
            if ((phaseTimer_ <= 0.0f && targetY <= y_ + 16.0f) || isNearFloor(level)) {
                state_ = BlooperState::WindingUp;
                phaseTimer_ = kWindupDuration;
            }
            break;
        case BlooperState::WindingUp:
            if (phaseTimer_ <= 0.0f) {
                // Elige la diagonal del envion recien ahora, con la
                // posicion del personaje al momento de largarse -- puede
                // ser hacia arriba O hacia abajo segun donde este, SALVO
                // que el piso siga cerca: ahi fuerza para arriba sin
                // importar donde este el personaje (ver el comentario de
                // la clase, evita tocar el piso o a un Mario chico parado
                // en el).
                state_ = BlooperState::Bursting;
                phaseTimer_ = kBurstDuration;
                const bool wantRight = targetX >= x_;
                const bool wantDown = false;  // el original solo se impulsa para arriba
                velX_ = wantRight ? kBurstSpeedX : -kBurstSpeedX;
                velY_ = wantDown ? kBurstSpeedY : -kBurstSpeedY;
                lastFacingRight_ = wantRight;
            }
            break;
        case BlooperState::Bursting:
            if (phaseTimer_ <= 0.0f) {
                backToResting();
            }
            break;
    }

    applyHorizontalWithWallBounce(dt, level);
    if (applyVerticalWithFloorCollision(dt, level) && state_ == BlooperState::Bursting) {
        backToResting();
    }
    // Mas arriba de la superficie (original: Y 0x20) no sube.
    if (y_ < 48.0f) {
        y_ = 48.0f;
        if (state_ == BlooperState::Bursting) backToResting();
    }

    if (y_ > screenHeight) {
        alive_ = false;
    }
}

void Blooper::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    // Columna 1 (retraida) solo durante el aviso previo al envion
    // (WindingUp, ver el comentario de la clase), columna 0 el resto del
    // tiempo -- incluido mientras dura el envion en si.
    const int col = (!falling_ && state_ == BlooperState::WindingUp) ? 1 : 0;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // El arte fuente mira a la izquierda por defecto (mismo criterio que el
    // resto del elenco): se flipea mirando a la derecha. Boca abajo (flip
    // vertical) solo mientras cae muerto.
    sprite.draw(renderer, worldRow, col, drawX, drawY, lastFacingRight_, falling_);
}
