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
    patrolRange_ = randomPatrolRange();
    flameTimerIndex_ = 0;
    groundY_ = y;
    velX_ = movingRight ? kWalkSpeed : -kWalkSpeed;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
    defeated_ = false;
    lastFacingRight_ = movingRight;
    jumpTimer_ = randomRange(kJumpIntervalMin, kJumpIntervalMax);
    attackTimer_ = nextAttackDelay();
    attackWindupTimer_ = 0.0f;
    attackRecoverTimer_ = 0.0f;
    // Con Both arranca con fuego (da igual cual, se alterna de ahi en mas);
    // con los otros dos modos siempre es el mismo (ver el comentario de la
    // clase).
    nextAttack_ = attackMode_ == BowserAttackMode::HammerOnly ? AttackType::Hammer : AttackType::Fire;
    currentAttack_ = nextAttack_;
    fireRequested_ = false;
    hammerThrowRequested_ = false;
    hammerBurstRemaining_ = 0;
    hammerBurstTimer_ = 0.0f;
    fireballHits_ = 0;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void Bowser::reset() {
    alive_ = false;
    falling_ = false;
    fireRequested_ = false;
    hammerThrowRequested_ = false;
    hammerBurstRemaining_ = 0;
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

bool Bowser::consumeHammerThrowRequest() {
    const bool value = hammerThrowRequested_;
    hammerThrowRequested_ = false;
    return value;
}

float Bowser::hammerOriginX() const { return x_ + static_cast<float>(kWidth) / 2.0f; }
float Bowser::hammerOriginY() const { return y_; }
float Bowser::hammerVelocityX() const { return lastFacingRight_ ? kHammerSpeedX : -kHammerSpeedX; }
float Bowser::hammerVelocityY() const { return kHammerSpeedY; }

float Bowser::randomPatrolRange() {
    static const float kRanges[4] = {17.0f, 33.0f, 49.0f, 65.0f};
    return kRanges[static_cast<int>(randomRange(0.0f, 3.999f))];
}

float Bowser::nextAttackDelay() {
    // Original: una pausa de 0x20 cuadros y despues la espera de la tabla
    // (0xbf o 0x40 cuadros) en un orden fijo que se repite.
    static const float kTable[8] = {3.18f, 1.07f, 3.18f, 3.18f, 3.18f, 1.07f, 1.07f, 3.18f};
    const float delay = 0.53f + kTable[flameTimerIndex_];
    flameTimerIndex_ = (flameTimerIndex_ + 1) % 8;
    return delay;
}

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

    const bool attacking = attackWindupTimer_ > 0.0f || hammerBurstRemaining_ > 0 || attackRecoverTimer_ > 0.0f;
    if (!attacking) {
        // Patrulla dentro de kPatrolRange alrededor del spawn (ver el
        // comentario de la clase): rebota en esos bordes, no en tiles --
        // sigue caminando aunque este en el aire de un salto.
        x_ += velX_ * dt;
        if (x_ <= spawnX_ - patrolRange_) {
            x_ = spawnX_ - patrolRange_;
            velX_ = kWalkSpeed;
            patrolRange_ = randomPatrolRange();
        } else if (x_ >= spawnX_ + patrolRange_) {
            x_ = spawnX_ + patrolRange_;
            velX_ = -kWalkSpeed;
            patrolRange_ = randomPatrolRange();
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
            // Se fija recien ahora (no cambia hasta el proximo ciclo, ver
            // el comentario de la clase) para que render() sepa durante
            // todo el amague si le toca cerrar la boca o no.
            currentAttack_ = nextAttack_;
        } else if (grounded && jumpTimer_ <= 0.0f) {
            velY_ = kJumpImpulse;
            jumpTimer_ = randomRange(kJumpIntervalMin, kJumpIntervalMax);
        }
    } else if (attackWindupTimer_ > 0.0f) {
        attackWindupTimer_ -= dt;
        if (attackWindupTimer_ <= 0.0f) {
            attackWindupTimer_ = 0.0f;
            if (currentAttack_ == AttackType::Fire) {
                fireRequested_ = true;
                attackRecoverTimer_ = kAttackRecoverDuration;
            } else {
                // Rafaga (ver el comentario de la clase): el primero sale
                // ya, los otros kHammerBurstCount-1 con intervalo (ver la
                // rama de hammerBurstRemaining_ mas abajo) -- el descanso
                // post-ataque recien arranca cuando termina toda la rafaga,
                // no despues de este primero.
                hammerThrowRequested_ = true;
                hammerBurstRemaining_ = kHammerBurstCount - 1;
                hammerBurstTimer_ = kHammerBurstInterval;
            }
            // Decide el que le toca al PROXIMO ciclo (ver AttackMode en el
            // comentario de la clase): con Both alterna, con los otros dos
            // modos siempre es el mismo.
            switch (attackMode_) {
                case BowserAttackMode::FireOnly:
                    nextAttack_ = AttackType::Fire;
                    break;
                case BowserAttackMode::HammerOnly:
                    nextAttack_ = AttackType::Hammer;
                    break;
                case BowserAttackMode::Both:
                    nextAttack_ = currentAttack_ == AttackType::Fire ? AttackType::Hammer : AttackType::Fire;
                    break;
            }
        }
    } else if (hammerBurstRemaining_ > 0) {
        hammerBurstTimer_ -= dt;
        if (hammerBurstTimer_ <= 0.0f) {
            hammerThrowRequested_ = true;
            --hammerBurstRemaining_;
            hammerBurstTimer_ = kHammerBurstInterval;
            if (hammerBurstRemaining_ <= 0) {
                attackRecoverTimer_ = kAttackRecoverDuration;
            }
        }
    } else {
        attackRecoverTimer_ -= dt;
        if (attackRecoverTimer_ <= 0.0f) {
            attackRecoverTimer_ = 0.0f;
            attackTimer_ = nextAttackDelay();
        }
    }
}

void Bowser::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;

    int col;
    if (falling_) {
        col = 0;  // reusa el frame de caminata 0, boca abajo (ver mas abajo)
    } else if (attackWindupTimer_ > 0.0f && currentAttack_ == AttackType::Fire) {
        // Boca cerrada (columnas 2-3, ver el comentario de la clase): solo
        // en el amague antes de escupir fuego, ningun otro momento (el
        // amague del martillo usa boca abierta, sin pose propia).
        col = 2 + animFrame_;
    } else {
        // Boca abierta (columnas 0-1): caminando, en el amague/tiro del
        // martillo, o ya disparado el fuego -- la vuelve a abrir justo al
        // escupirlo, se queda asi durante kAttackRecoverDuration.
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
