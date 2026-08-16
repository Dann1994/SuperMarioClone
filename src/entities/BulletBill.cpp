#include "BulletBill.h"

#include <algorithm>
#include <cmath>

void BulletBill::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kSpeed : -kSpeed;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
}

void BulletBill::reset() {
    alive_ = false;
    falling_ = false;
}

bool BulletBill::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    return true;
}

void BulletBill::update(float dt, int screenWidth, int screenHeight) {
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

    // Linea recta, sin gravedad ni colision (ver el comentario de la
    // clase): se dispara y se olvida.
    x_ += velX_ * dt;
    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth)) {
        alive_ = false;
    }
}

void BulletBill::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // El arte fuente mira a la izquierda por defecto (mismo criterio que el
    // resto del elenco): se flipea mirando a la derecha (velX_ sirve para
    // esto en los dos estados, vivo o cayendo muerto). Boca abajo (flip
    // vertical) solo mientras cae muerto. Un solo frame (no camina ni
    // nada, ver el comentario de la clase), siempre columna 0.
    sprite.draw(renderer, worldRow, 0, drawX, drawY, velX_ >= 0.0f, falling_);
}
