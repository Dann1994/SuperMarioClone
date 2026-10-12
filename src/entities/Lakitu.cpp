#include "Lakitu.h"

#include <algorithm>
#include <cmath>

void Lakitu::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    baseY_ = y;
    velX_ = 0.0f;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
    lastFacingRight_ = movingRight;
    catchingUp_ = false;
    hoverPhase_ = 0.0f;
    moveRight_ = movingRight;
    moveSpeed_ = 0.0f;
    throwTimer_ = kThrowInterval;
    throwWindupTimer_ = 0.0f;
    throwRequested_ = false;
}

void Lakitu::reset() {
    alive_ = false;
    falling_ = false;
    throwRequested_ = false;
}

bool Lakitu::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    return true;
}

bool Lakitu::consumeThrowRequest() {
    const bool value = throwRequested_;
    throwRequested_ = false;
    return value;
}

float Lakitu::throwOriginX() const { return x_ + static_cast<float>(kWidth) / 2.0f; }
// Arriba de la cabeza (no a la altura de los pies): sale como si lo tirara
// para arriba desde ahi, no desde abajo del personaje (ver
// kEggSpeedY/kSpawnAboveOffset y el comentario de la clase).
float Lakitu::throwOriginY() const { return y_ - kSpawnAboveOffset; }
float Lakitu::throwVelocityX() const { return lastFacingRight_ ? kEggSpeedX : -kEggSpeedX; }
float Lakitu::throwVelocityY() const { return kEggSpeedY; }

void Lakitu::update(float dt, float targetX, float cameraScrollSpeed, int levelWidthPixels, int screenHeight) {
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

    // Persecucion del original (MoveLakitu/PlayerLakituDiff): la velocidad
    // es mas alta cuanto MAS CERCA esta del personaje (base - distancia/4),
    // con una base segun como se mueve la camara (quieta 21, lento 48,
    // corriendo 64, en 1/16 px por cuadro). El sentido solo cambia cuando
    // ya se paso 60 px: ahi frena de a una unidad por cuadro y recien en 0
    // da la vuelta -- de ahi el vaiven por encima del jugador.
    const float center = x_ + static_cast<float>(kWidth) / 2.0f;
    const float diff = targetX - center;
    const bool targetRight = diff > 0.0f;
    const float distance = std::min(std::abs(diff), 60.0f);
    if (distance >= 60.0f && targetRight != moveRight_ && moveSpeed_ > 0.0f) {
        moveSpeed_ = std::max(0.0f, moveSpeed_ - 60.0f * dt);
        if (moveSpeed_ <= 0.0f) moveRight_ = targetRight;
    } else {
        if (distance >= 60.0f) moveRight_ = targetRight;
        const float scroll = std::abs(cameraScrollSpeed);
        const float base = scroll < kScrollActiveThreshold ? 21.0f : scroll <= 90.0f ? 48.0f : 64.0f;
        moveSpeed_ = base - (std::floor(distance / 4.0f) + 1.0f);
    }
    velX_ = (moveRight_ ? 1.0f : -1.0f) * moveSpeed_ * 3.75f;
    lastFacingRight_ = targetRight;
    x_ += velX_ * dt;
    x_ = std::clamp(x_, 0.0f, static_cast<float>(levelWidthPixels - kWidth));

    // Hamacado suave alrededor de la altura de spawn, no una fisica real
    // (sin gravedad ni piso -- vuela).
    hoverPhase_ += kHoverSpeed * dt;
    y_ = baseY_ + std::sin(hoverPhase_) * kHoverAmplitude;

    if (throwWindupTimer_ > 0.0f) {
        // Ya esta agachado con los brazos arriba (ver isWindingUp()): cuando
        // se termina el amague recien ahi tira el huevo de verdad.
        throwWindupTimer_ -= dt;
        if (throwWindupTimer_ <= 0.0f) {
            throwWindupTimer_ = 0.0f;
            throwRequested_ = true;
        }
    } else {
        throwTimer_ -= dt;
        if (throwTimer_ <= 0.0f) {
            throwWindupTimer_ = kThrowWindupDuration;
            throwTimer_ = kThrowInterval;
        }
    }
}

void Lakitu::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    // Columna 1 = agachado (amague antes de tirar, ver isWindingUp() y el
    // comentario de la clase), columna 0 = normal el resto del tiempo.
    const int col = (!falling_ && isWindingUp()) ? 1 : 0;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // La cara es practicamente simetrica en el arte fuente (ver
    // Documentaciones/Lakitu.png): el flip horizontal no se nota, pero se
    // deja igual por consistencia con el resto del elenco. Boca abajo (flip
    // vertical) solo mientras cae muerto.
    sprite.draw(renderer, worldRow, col, drawX, drawY, lastFacingRight_, falling_);
}
