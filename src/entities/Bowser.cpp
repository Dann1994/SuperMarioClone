#include "Bowser.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "BowserFire.h"

namespace {
float randomRange(float minVal, float maxVal) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minVal + t * (maxVal - minVal);
}
}  // namespace

void Bowser::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    spawnX_ = x;
    groundY_ = y;
    velX_ = movingRight ? kWalkSpeed : -kWalkSpeed;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
    defeated_ = false;
    lastFacingRight_ = movingRight;
    jumpTimer_ = randomRange(kJumpIntervalMin, kJumpIntervalMax);
    attackTimer_ = randomRange(kAttackIntervalMin, kAttackIntervalMax);
    attackWindupTimer_ = 0.0f;
    attackRecoverTimer_ = 0.0f;
    fireRequested_ = false;
    fireballHits_ = 0;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void Bowser::reset() {
    alive_ = false;
    falling_ = false;
    fireRequested_ = false;
}

void Bowser::faceTowards(float targetX) {
    lastFacingRight_ = targetX >= x_ + static_cast<float>(kWidth) / 2.0f;
}

void Bowser::killFalling(bool flyRight) {
    if (!alive_ || falling_) return;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
}

void Bowser::killByFireball(bool flyRight) {
    if (!isDangerous()) return;
    ++fireballHits_;
    if (fireballHits_ >= kFireballsToDefeat) {
        defeated_ = true;
        killFalling(flyRight);
    }
}

void Bowser::killByAxe(bool flyRight) {
    if (!isDangerous()) return;
    defeated_ = true;
    killFalling(flyRight);
}

bool Bowser::consumeFireRequest() {
    const bool value = fireRequested_;
    fireRequested_ = false;
    return value;
}

float Bowser::fireOriginX() const {
    // Intento de acercarse a la imagen de referencia que mando el usuario
    // (fuego un poco superpuesto/saliendo de la boca, no perfectamente
    // pegado al borde del cuerpo): un pequeno solape hacia adentro sobre la
    // version anterior. Si no queda bien, la version anterior (probada y
    // confirmada por el usuario) era simplemente pegar los bordes sin
    // superposicion:
    //   lastFacingRight_ ? x_ + kWidth : x_ - BowserFire::kWidth
    static constexpr float kOverlap = 4.0f;
    return lastFacingRight_ ? x_ + static_cast<float>(kWidth) - kOverlap
                             : x_ - static_cast<float>(BowserFire::kWidth) + kOverlap;
}
float Bowser::fireOriginY() const { return y_ + kFireOriginYOffset; }
float Bowser::fireVelocityX() const { return lastFacingRight_ ? kFireSpeedX : -kFireSpeedX; }

void Bowser::update(float dt, int screenHeight) {
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
    if (animTimer_ >= kWalkFrameDuration) {
        animTimer_ -= kWalkFrameDuration;
        animFrame_ = (animFrame_ + 1) % 2;
    }

    // Salto en el lugar (ver el comentario de la clase): gravedad siempre
    // que no este cayendo derrotado, para no dejarlo "flotando" si el
    // ataque le toca mientras esta en el aire -- solo DISPARAR un salto
    // nuevo pide estar apoyado (ver mas abajo), pero la parabola de uno ya
    // en curso sigue igual este frame sea el que sea.
    velY_ = std::min(velY_ + kJumpGravity * dt, kMaxFallSpeed);
    y_ += velY_ * dt;
    if (y_ >= groundY_) {
        y_ = groundY_;
        velY_ = 0.0f;
    }
    const bool grounded = y_ >= groundY_;

    const bool attacking = attackWindupTimer_ > 0.0f || attackRecoverTimer_ > 0.0f;
    if (!attacking) {
        // Patrulla dentro de kPatrolRange alrededor del spawn (ver el
        // comentario de la clase): rebota en esos bordes, no en tiles --
        // sigue caminando aunque este en el aire de un salto.
        x_ += velX_ * dt;
        if (x_ <= spawnX_ - kPatrolRange) {
            x_ = spawnX_ - kPatrolRange;
            velX_ = kWalkSpeed;
        } else if (x_ >= spawnX_ + kPatrolRange) {
            x_ = spawnX_ + kPatrolRange;
            velX_ = -kWalkSpeed;
        }

        // Los dos timers cuentan siempre, este en el aire o no (si no, un
        // salto de por medio congelaba el del fuego y este casi nunca
        // llegaba a dispararse, bug reportado por el usuario). Atacar YA NO
        // pide estar apoyado (a pedido del usuario: tambien puede escupir
        // fuego en pleno salto) -- saltar SI sigue pidiendolo, no se puede
        // saltar de nuevo en el aire. Ataque primero si los dos estan
        // listos el mismo frame.
        jumpTimer_ -= dt;
        attackTimer_ -= dt;
        if (attackTimer_ <= 0.0f) {
            attackWindupTimer_ = kAttackWindupDuration;
        } else if (grounded && jumpTimer_ <= 0.0f) {
            velY_ = kJumpImpulse;
            jumpTimer_ = randomRange(kJumpIntervalMin, kJumpIntervalMax);
        }
    } else if (attackWindupTimer_ > 0.0f) {
        attackWindupTimer_ -= dt;
        if (attackWindupTimer_ <= 0.0f) {
            attackWindupTimer_ = 0.0f;
            fireRequested_ = true;
            attackRecoverTimer_ = kAttackRecoverDuration;
        }
    } else {
        attackRecoverTimer_ -= dt;
        if (attackRecoverTimer_ <= 0.0f) {
            attackRecoverTimer_ = 0.0f;
            attackTimer_ = randomRange(kAttackIntervalMin, kAttackIntervalMax);
        }
    }
}

void Bowser::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;

    int col;
    if (falling_) {
        col = 0;  // reusa el frame de caminata 0, boca abajo (ver mas abajo)
    } else if (attackWindupTimer_ > 0.0f) {
        // Boca cerrada (columnas 2-3, ver el comentario de la clase): solo
        // en el amague antes de escupir, ningun otro momento.
        col = 2 + animFrame_;
    } else {
        // Boca abierta (columnas 0-1): caminando, o ya disparado el fuego
        // -- la vuelve a abrir justo al escupirlo, se queda asi durante
        // kAttackRecoverDuration.
        col = animFrame_;
    }

    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // El arte fuente mira a la izquierda por defecto (mismo criterio que el
    // resto del elenco): se flipea mirando a la derecha. Boca abajo (flip
    // vertical) solo mientras cae derrotado, igual que el resto del elenco
    // sin arte de muerte propio.
    sprite.draw(renderer, worldRow, col, drawX, drawY, lastFacingRight_, falling_);
}
