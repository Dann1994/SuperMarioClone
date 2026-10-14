#include "Goomba.h"
#include "Difficulty.h"

#include <algorithm>
#include <cmath>

void Goomba::advanceWaddle(float dt) {
    sWaddleClock = std::fmod(sWaddleClock + dt, 2.0f * kWaddleInterval);
}

void Goomba::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    const float speed = kSpeed * hardWalkFactor();
    velX_ = movingRight ? speed : -speed;
    velY_ = 0.0f;
    stompedTimer_ = 0.0f;
    state_ = State::Walking;
}

void Goomba::squish() {
    if (state_ != State::Walking) return;
    state_ = State::Stomped;
    stompedTimer_ = kStompedDuration;
    velX_ = 0.0f;
    velY_ = 0.0f;
}

bool Goomba::killFalling(bool flyRight) {
    if (state_ != State::Walking) return false;
    // SetStun2: sube 2 px y arranca el salto.
    state_ = State::Defeated;
    y_ -= 2.0f;
    velX_ = flyRight ? kDefeatSpeedX : -kDefeatSpeedX;
    velY_ = kDefeatHop;
    return true;
}

void Goomba::facePlayer(float playerCenterX) {
    const bool playerRight = playerCenterX >= x_ + kSize / 2.0f;
    if ((velX_ > 0.0f) != playerRight) velX_ = -velX_;
}

void Goomba::update(float dt, const Tilemap& level, int screenHeight) {
    switch (state_) {
        case State::Inactive:
            return;
        case State::Defeated:
            // Sin colision: cae hasta salir de la pantalla.
            velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
            x_ += velX_ * dt;
            y_ += velY_ * dt;
            if (y_ > static_cast<float>(screenHeight)) state_ = State::Inactive;
            return;
        case State::Stomped:
            stompedTimer_ -= dt;
            if (stompedTimer_ <= 0.0f) state_ = State::Inactive;
            return;
        case State::Walking:
            break;
    }
    moveHorizontally(dt, level);
    moveVertically(dt, level);
    if (y_ > static_cast<float>(level.heightPixels())) state_ = State::Inactive;
}

void Goomba::moveHorizontally(float dt, const Tilemap& level) {
    // Pared adelante: da la vuelta (DoEnemySideCheck), sin avanzar.
    const int ts = level.tileSize();
    const float newX = x_ + velX_ * dt;
    const float edgeX = velX_ > 0.0f ? newX + kSize - kEdgeEpsilon : newX;
    const int col = static_cast<int>(std::floor(edgeX / ts));
    const int row0 = static_cast<int>(std::floor(y_ / ts));
    const int row1 = static_cast<int>(std::floor((y_ + kSize - kEdgeEpsilon) / ts));
    for (int row = row0; row <= row1; ++row) {
        if (level.isSolidTile(col, row)) {
            velX_ = -velX_;
            return;
        }
    }
    x_ = newX;
}

void Goomba::moveVertically(float dt, const Tilemap& level) {
    // Sin miedo a los bordes: si no hay piso, cae (estado FALLING del
    // original) manteniendo la velocidad horizontal.
    const int ts = level.tileSize();
    velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
    float newY = y_ + velY_ * dt;
    if (velY_ > 0.0f) {
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kSize - kEdgeEpsilon) / ts));
        const int row = static_cast<int>(std::floor((newY + kSize - kEdgeEpsilon) / ts));
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row)) {
                newY = static_cast<float>(row * ts - kSize);
                velY_ = 0.0f;
                break;
            }
        }
    }
    y_ = newY;
}

void Goomba::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (state_ == State::Inactive) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    switch (state_) {
        case State::Defeated:
            // Boca abajo (el original lo baja 2 px al darlo vuelta).
            sprite.draw(renderer, worldRow, 0, drawX, drawY + 2, velX_ < 0.0f, true);
            return;
        case State::Stomped:
            sprite.draw(renderer, worldRow, 2, drawX, drawY, false);
            return;
        default: {
            // Bamboleo comun a todos: alterna los dos cuadros cada 8 cuadros.
            const int frame = sWaddleClock < kWaddleInterval ? 0 : 1;
            sprite.draw(renderer, worldRow, frame, drawX, drawY, false);
            return;
        }
    }
}
