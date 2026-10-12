#include "CheepCheep.h"

#include <algorithm>
#include <cmath>

void CheepCheep::spawn(float x, float y, bool movingRight, float driftSpeed) {
    x_ = x;
    y_ = y;
    baseY_ = y;
    wavePhase_ = 0.0f;
    alive_ = true;
    falling_ = false;
    lastFacingRight_ = movingRight;
    animTimer_ = 0.0f;
    animFrame_ = 0;
    if (variant_ == CheepCheepVariant::Jumping) {
        // Arranca ya con el envion hacia arriba (ver el comentario de la
        // clase): no espera parada en ningun piso, la parabola empieza en el
        // mismo frame del spawn.
        const float drift = driftSpeed >= 0.0f ? driftSpeed : kJumpDriftSpeed;
        velX_ = movingRight ? drift : -drift;
        velY_ = kJumpImpulse;
    } else {
        const float speed = swimPattern_ == SwimPattern::Straight ? kStraightSwimSpeed : kOscillateDriftSpeed;
        velX_ = movingRight ? speed : -speed;
        velY_ = 0.0f;
    }
}

void CheepCheep::reset() {
    alive_ = false;
    falling_ = false;
}

bool CheepCheep::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    return true;
}

void CheepCheep::updateSwimming(float dt, int screenWidth) {
    x_ += velX_ * dt;
    // Ver el comentario de la clase: Straight no tiene vaiven vertical
    // (queda a la altura de spawn), Oscillating tiene uno corto y lento.
    if (swimPattern_ == SwimPattern::Oscillating) {
        wavePhase_ += kOscillateFrequency * dt;
        y_ = baseY_ + std::sin(wavePhase_) * kOscillateAmplitude;
    }

    if (x_ + kSize < 0.0f || x_ > static_cast<float>(screenWidth)) {
        alive_ = false;
    }
}

void CheepCheep::updateJumping(float dt) {
    x_ += velX_ * dt;
    velY_ = std::min(velY_ + kJumpGravity * dt, kMaxFallSpeed);
    y_ += velY_ * dt;

    // Ya paso la cima y volvio a cruzar la altura de spawn de bajada: se
    // desactiva sola (ver el comentario de la clase), sin rebotar ni volver
    // a saltar por su cuenta.
    if (velY_ > 0.0f && y_ >= baseY_) {
        alive_ = false;
    }
}

void CheepCheep::update(float dt, int screenWidth, int screenHeight) {
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

    animTimer_ += dt;
    if (animTimer_ >= kAnimFrameDuration) {
        animTimer_ -= kAnimFrameDuration;
        animFrame_ = (animFrame_ + 1) % 2;
    }

    if (variant_ == CheepCheepVariant::Swimming) {
        updateSwimming(dt, screenWidth);
        if (y_ > screenHeight) {
            alive_ = false;
        }
    } else {
        updateJumping(dt);
    }
}

void CheepCheep::render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // Una sola paleta ("All Environments", ver el comentario de la clase):
    // siempre fila 0.
    if (falling_) {
        // Boca abajo, frame quieto (no tiene un frame "muerto" propio
        // recortado del sheet, mismo criterio que Goomba/HammerBro).
        sprite.draw(renderer, 0, 0, drawX, drawY, velX_ < 0.0f, true);
        return;
    }
    // El arte fuente mira a la izquierda por defecto (mismo criterio que el
    // resto del elenco): se flipea mirando a la derecha. Alterna sus dos
    // frames en las dos variantes (nado/salto).
    sprite.draw(renderer, 0, animFrame_, drawX, drawY, lastFacingRight_);
}
