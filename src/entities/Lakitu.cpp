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

    // Sigue al personaje mas cercano en X (ver el comentario de la clase):
    // normal la mayoria del tiempo, en rafaga mientras la camara esta
    // scrolleando de verdad (o, sin scroll, si se quedo demasiado atras --
    // ver kCatchUpThreshold).
    const float center = x_ + static_cast<float>(kWidth) / 2.0f;
    const float diff = targetX - center;
    const bool scrolling = std::abs(cameraScrollSpeed) > kScrollActiveThreshold;

    if (scrolling) {
        // Con scroll activo el sentido lo da la camara, no la diferencia de
        // posiciones (ver el comentario de la clase) -- se reafirma todos
        // los cuadros, no solo al arrancar la rafaga.
        catchingUp_ = true;
        catchUpRight_ = cameraScrollSpeed > 0.0f;
    } else if (!catchingUp_ && std::abs(diff) > kCatchUpThreshold) {
        catchingUp_ = true;
        catchUpRight_ = diff > 0.0f;
    }

    if (catchingUp_) {
        velX_ = catchUpRight_ ? kCatchUpSpeed : -kCatchUpSpeed;
        lastFacingRight_ = catchUpRight_;
        x_ += velX_ * dt;
        // Termina cuando queda kAheadOffset por delante del personaje Y ya
        // no hay scroll activo (ver el comentario de la clase) -- mientras
        // la camara siga moviendose, sigue acompañandola aunque ya este
        // adelante, en vez de frenar y quedarse atras de nuevo.
        const float newDiff = targetX - (x_ + static_cast<float>(kWidth) / 2.0f);
        const bool aheadEnough = catchUpRight_ ? (newDiff <= -kAheadOffset) : (newDiff >= kAheadOffset);
        if (aheadEnough && !scrolling) {
            catchingUp_ = false;
        }
    } else if (std::abs(diff) > kFollowDeadzone) {
        velX_ = diff > 0.0f ? kFollowSpeed : -kFollowSpeed;
        lastFacingRight_ = velX_ > 0.0f;
        x_ += velX_ * dt;
    } else {
        velX_ = 0.0f;
    }
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
