#include "Koopa.h"

#include <algorithm>
#include <cmath>

void Koopa::spawn(float x, float y, bool movingRight, KoopaState initialState) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kWalkSpeed : -kWalkSpeed;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
    state_ = initialState;
    lastFacingRight_ = movingRight;
    shellIdleTimer_ = 0.0f;
    flyBaseY_ = y;
    flyPhase_ = 0.0f;
    postStompCooldown_ = 0.0f;
    stompCooldownOwner_ = nullptr;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void Koopa::reset() {
    alive_ = false;
    falling_ = false;
    state_ = KoopaState::Walking;
    postStompCooldown_ = 0.0f;
    stompCooldownOwner_ = nullptr;
}

void Koopa::stomp(const void* character) {
    if (!alive_ || falling_) return;

    if (state_ == KoopaState::Flying || state_ == KoopaState::Jumping) {
        // Deja de volar/saltar: pasa a caminar derecho. Mismo alto de
        // hitbox para los tres (kWalkHeight), asi que no hace falta ajustar
        // y_ -- simplemente le empieza a aplicar la gravedad normal de
        // Walking en vez de la fisica propia del estado anterior.
        state_ = KoopaState::Walking;
        animTimer_ = 0.0f;
        animFrame_ = 0;
    } else if (state_ == KoopaState::Walking) {
        // Se esconde: el caparazon mide menos que caminando, hay que subir
        // y_ para que los pies queden en el mismo lugar (mismo criterio que
        // Mario::setSize).
        y_ += static_cast<float>(kWalkHeight - kShellHeight);
        state_ = KoopaState::ShellIdle;
        shellIdleTimer_ = kShellIdleDuration;
        velX_ = 0.0f;
        velY_ = 0.0f;
        animTimer_ = 0.0f;
        animFrame_ = 0;
    } else if (state_ == KoopaState::ShellMoving) {
        state_ = KoopaState::ShellIdle;
        shellIdleTimer_ = kShellIdleDuration;
        velX_ = 0.0f;
        // Inmuniza a "character" un ratito (ver isImmuneTo): que tenga
        // margen para separarse antes de que este Koopa vuelva a
        // reaccionarle a un pisoton/toque suyo.
        postStompCooldown_ = kPostStompCooldown;
        stompCooldownOwner_ = character;
    }
    // ShellIdle: pisarlo de nuevo no hace nada (el llamador igual hace
    // rebotar al personaje).
}

void Koopa::kick(bool kickRight, const void* character) {
    if (!alive_ || falling_ || state_ != KoopaState::ShellIdle) return;
    state_ = KoopaState::ShellMoving;
    velX_ = kickRight ? kShellSpeed : -kShellSpeed;
    lastFacingRight_ = kickRight;
    // Mismo respiro que al frenarlo (ver stomp()): si lo pateo pisandolo
    // desde arriba, el personaje no rebota (queda parado encima) y sin esto
    // el contacto que sigue -- ahora contra un caparazon en movimiento --
    // lo terminaria danando o matando.
    postStompCooldown_ = kPostStompCooldown;
    stompCooldownOwner_ = character;
    kicker_ = character;
}

bool Koopa::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    animTimer_ = 0.0f;
    animFrame_ = 0;
    return true;
}

bool Koopa::applyVerticalPhysics(float dt, const Tilemap& level, int hitboxHeight) {
    const int ts = level.tileSize();
    velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
    float newY = y_ + velY_ * dt;
    bool landed = false;
    if (velY_ > 0.0f) {
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kHitboxWidth - kEdgeEpsilon) / ts));
        const int row = static_cast<int>(std::floor((newY + hitboxHeight - kEdgeEpsilon) / ts));
        bool blocked = false;
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row)) { blocked = true; break; }
        }
        if (blocked) {
            newY = static_cast<float>(row * ts - hitboxHeight);
            velY_ = 0.0f;
            landed = true;
        }
    }
    y_ = newY;
    return landed;
}

void Koopa::applyHorizontalWithWallBounce(float dt, const Tilemap& level, int hitboxHeight, bool fearsLedges) {
    const int ts = level.tileSize();
    const float newX = x_ + velX_ * dt;
    const float edgeX = velX_ > 0.0f ? newX + kHitboxWidth - kEdgeEpsilon : newX;
    const int col = static_cast<int>(std::floor(edgeX / ts));
    const int col0 = static_cast<int>(std::floor(x_ / ts));
    const int col1 = static_cast<int>(std::floor((x_ + kHitboxWidth - kEdgeEpsilon) / ts));
    const int row0 = static_cast<int>(std::floor(y_ / ts));
    const int row1 = static_cast<int>(std::floor((y_ + hitboxHeight - kEdgeEpsilon) / ts));
    bool blocked = false;
    for (int row = row0; row <= row1; ++row) {
        if (level.isSolidTile(col, row)) { blocked = true; break; }
    }
    if (!blocked && fearsLedges) {
        // No hay pared, pero si le teme a los bordes (Koopa rojo), tambien
        // se da vuelta si no hay piso justo debajo del borde de avance --
        // la fila siguiente a donde terminan los pies. Ojo: esto solo vale
        // si AHORA MISMO esta parado en el piso (mismo criterio, columnas
        // propias en vez del borde de avance) -- si no, esta en el aire (el
        // saltarin pasa la mayor parte del tiempo asi) y "no hay piso a la
        // altura de mis pies" no significa nada todavia, sin este chequeo
        // se da vuelta en falso cada frame que esta saltando.
        const int floorRow = row1 + 1;
        const bool currentlyGrounded =
            level.isSolidTile(col0, floorRow) || level.isSolidTile(col1, floorRow);
        if (currentlyGrounded && !level.isSolidTile(col, floorRow)) {
            blocked = true;
        }
    }
    if (blocked) {
        velX_ = -velX_;
        lastFacingRight_ = velX_ > 0.0f;
        wallBounceRequested_ = true;
    } else {
        x_ = newX;
    }
}

void Koopa::update(float dt, const Tilemap& level, int screenHeight) {
    if (!alive_) return;

    if (postStompCooldown_ > 0.0f) {
        postStompCooldown_ = std::max(0.0f, postStompCooldown_ - dt);
    }

    if (falling_) {
        // Parabola (igual que Goomba::killFalling): se esconde en el
        // caparazon (frame shell1, quieto) y sale disparado, en vez de
        // quedar caminando/mostrando las patas afuera.
        velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
        x_ += velX_ * dt;
        y_ += velY_ * dt;
        if (y_ > screenHeight) {
            alive_ = false;
        }
        return;
    }

    if (state_ == KoopaState::Flying) {
        // Igual que Walking en X (misma velocidad, rebota contra pared),
        // pero flotando: sin gravedad ni piso, hamacandose en Y con una
        // onda senoidal alrededor de la altura de spawn (flyBaseY_).
        animTimer_ += dt;
        if (animTimer_ >= kFlyFrameDuration) {
            animTimer_ -= kFlyFrameDuration;
            animFrame_ = (animFrame_ + 1) % 2;
        }
        flyPhase_ += kFlyHoverSpeed * dt;
        float intendedY = flyBaseY_ + std::sin(flyPhase_) * kFlyHoverAmplitude;

        // Si el hamacado lo mete contra un tile solido (el borde de abajo
        // de una plataforma, o el piso), no lo atraviesa -- se clava justo
        // en el borde, como si rebotara. Aparte del chequeo de pared de mas
        // abajo a proposito: ese solo mira si hay algo en la columna de
        // avance (izquierda/derecha), y con la Y sin recortar terminaba
        // confundiendo "toque el techo" con "hay una pared" -- se daba
        // vuelta en vez de solo rebotar, sin que la direccion horizontal
        // tenga nada que ver con un choque vertical.
        const int ts = level.tileSize();
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kHitboxWidth - kEdgeEpsilon) / ts));
        const int topRow = static_cast<int>(std::floor(intendedY / ts));
        for (int c = col0; c <= col1; ++c) {
            if (level.isSolidTile(c, topRow)) {
                intendedY = static_cast<float>((topRow + 1) * ts);
                break;
            }
        }
        const int bottomRow = static_cast<int>(std::floor((intendedY + kWalkHeight - kEdgeEpsilon) / ts));
        for (int c = col0; c <= col1; ++c) {
            if (level.isSolidTile(c, bottomRow)) {
                intendedY = static_cast<float>(bottomRow * ts - kWalkHeight);
                break;
            }
        }
        y_ = intendedY;

        // Vuela por encima de todo, no le teme a los bordes aunque sea rojo
        // (no toca el piso para caminar).
        applyHorizontalWithWallBounce(dt, level, kWalkHeight, false);
        return;
    }

    if (state_ == KoopaState::Jumping) {
        // Igual que Walking en X (rebota contra pared), pero en vez de
        // quedarse apoyado al aterrizar, cada aterrizaje le vuelve a dar el
        // impulso hacia arriba -- avanza a los saltos en vez de caminando
        // liso. Aletea igual de rapido que el volador (lleva las mismas
        // alas puestas, ver render()).
        animTimer_ += dt;
        if (animTimer_ >= kFlyFrameDuration) {
            animTimer_ -= kFlyFrameDuration;
            animFrame_ = (animFrame_ + 1) % 2;
        }
        applyHorizontalWithWallBounce(dt, level, kWalkHeight, color_ == KoopaColor::Red);
        if (applyVerticalPhysics(dt, level, kWalkHeight)) {
            velY_ = kJumpHopImpulse;
        }
        if (y_ > level.heightPixels()) {
            alive_ = false;
        }
        return;
    }

    if (state_ == KoopaState::ShellIdle) {
        shellIdleTimer_ -= dt;
        // Frame quieto (shell1) todo el tiempo, salvo el ultimo tramo antes
        // de salir: ahi parpadea con shell2 como aviso.
        if (shellIdleTimer_ <= kShellWarningDuration) {
            animTimer_ += dt;
            if (animTimer_ >= kShellWarningFlashDuration) {
                animTimer_ -= kShellWarningFlashDuration;
                animFrame_ = (animFrame_ + 1) % 2;
            }
        } else {
            animFrame_ = 0;
        }
        applyVerticalPhysics(dt, level, kShellHeight);
        if (shellIdleTimer_ <= 0.0f) {
            // Sale solo: retoma la caminata en la direccion que llevaba
            // antes de esconderse, restaurando la altura (pies fijos).
            y_ -= static_cast<float>(kWalkHeight - kShellHeight);
            state_ = KoopaState::Walking;
            velX_ = lastFacingRight_ ? kWalkSpeed : -kWalkSpeed;
            animTimer_ = 0.0f;
            animFrame_ = 0;
        }
        if (y_ > level.heightPixels()) {
            alive_ = false;
        }
        return;
    }

    if (state_ == KoopaState::Walking) {
        animTimer_ += dt;
        if (animTimer_ >= kWalkFrameDuration) {
            animTimer_ -= kWalkFrameDuration;
            animFrame_ = (animFrame_ + 1) % 2;
        }
    } else {
        // ShellMoving: un solo frame quieto (shell1), no "gira".
        animFrame_ = 0;
    }

    const int hitboxHeight = state_ == KoopaState::Walking ? kWalkHeight : kShellHeight;
    // Le teme a los bordes solo caminando y solo si es rojo -- un caparazon
    // en movimiento se cae aunque sea rojo, ya no "piensa" (ver el
    // comentario de la clase).
    const bool fearsLedges = state_ == KoopaState::Walking && color_ == KoopaColor::Red;
    applyHorizontalWithWallBounce(dt, level, hitboxHeight, fearsLedges);
    applyVerticalPhysics(dt, level, hitboxHeight);

    if (y_ > level.heightPixels()) {
        alive_ = false;
    }
}

void Koopa::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;

    int col;
    int hitboxHeight;
    if (falling_) {
        col = 4;  // shell1: se esconde en el caparazon al morir por estrella/fuego
        hitboxHeight = kShellHeight;
    } else if (state_ == KoopaState::Walking) {
        col = animFrame_;  // walk1/walk2
        hitboxHeight = kWalkHeight;
    } else if (state_ == KoopaState::Flying || state_ == KoopaState::Jumping) {
        // fly1/fly2 (con el ala): las dos variantes de Koopa Alado la
        // llevan puesta, se la sacan recien al perder el estado (pisoton).
        col = 2 + animFrame_;
        hitboxHeight = kWalkHeight;
    } else {
        // shell1 quieto siempre, salvo el aviso de "por salir" (animFrame_
        // solo llega a 1 durante kShellWarningDuration, ver update()).
        col = 4 + animFrame_;
        hitboxHeight = kShellHeight;
    }

    const int drawX = static_cast<int>(std::lround(x_ - cameraX)) - (kFrameWidth - kHitboxWidth) / 2;
    const int drawY = static_cast<int>(std::lround(y_)) - (kFrameHeight - hitboxHeight);
    // El rojo tiene una unica paleta ("All Environments"), ignora worldRow.
    const int row = color_ == KoopaColor::Red ? 0 : worldRow;
    // El arte fuente mira a la izquierda por defecto (ver Documentaciones/Enemigos.png),
    // asi que se flipea al reves que Mario/Goomba: mirando a la derecha SI
    // se flipea, no si no. Boca abajo (flip vertical) solo mientras cae
    // muerto, igual que el Goomba.
    sprite.draw(renderer, row, col, drawX, drawY, lastFacingRight_, falling_);
}
