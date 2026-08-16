#include "Mario.h"

#include <algorithm>
#include <cmath>

bool Mario::loadSprite(SDL_Renderer* renderer, const std::string& smallPath,
                        const std::string& bigPath, const std::string& firePath) {
    if (!spriteSmall_.loadFromFile(renderer, smallPath, kSpriteFrameWidth, kSmallHeight)) {
        return false;
    }
    if (!bigPath.empty() && !spriteBig_.loadFromFile(renderer, bigPath, kSpriteFrameWidth, kBigHeight)) {
        return false;
    }
    if (!firePath.empty() && !spriteFire_.loadFromFile(renderer, firePath, kSpriteFrameWidth, kBigHeight)) {
        return false;
    }
    return true;
}

bool Mario::loadStarSprite(SDL_Renderer* renderer, const std::string& smallStarPath,
                            const std::string& bigStarPath, const std::string& fireStarPath) {
    if (!smallStarPath.empty() && !spriteSmallStar_.loadFromFile(renderer, smallStarPath, kSpriteFrameWidth, kSmallHeight)) {
        return false;
    }
    if (!bigStarPath.empty() && !spriteBigStar_.loadFromFile(renderer, bigStarPath, kSpriteFrameWidth, kBigHeight)) {
        return false;
    }
    if (!fireStarPath.empty() && !spriteFireStar_.loadFromFile(renderer, fireStarPath, kSpriteFrameWidth, kBigHeight)) {
        return false;
    }
    return true;
}

int Mario::currentHeight() const {
    if (size_ == MarioSize::Small) return kSmallHitboxHeight;
    return isDucking_ ? kCrouchHitboxHeight : kBigHitboxHeight;
}

int Mario::currentWidth() const {
    return size_ == MarioSize::Small ? kSmallHitboxWidth : kBigHitboxWidth;
}

// Alto real del frame del spritesheet (para dibujar, no para el hitbox --
// ver el comentario de kSmallHitboxWidth en Mario.h): a diferencia de
// currentHeight() no cambia al agacharse, la pose de agachado esta dibujada
// DENTRO del mismo frame de siempre.
int Mario::currentSpriteFrameHeight() const {
    return size_ == MarioSize::Small ? kSmallHeight : kBigHeight;
}

const SpriteSheet& Mario::currentSprite() const {
    switch (size_) {
        case MarioSize::Big: return spriteBig_;
        case MarioSize::Fire: return spriteFire_;
        case MarioSize::Small: default: return spriteSmall_;
    }
}

const SpriteSheet& Mario::currentStarSprite() const {
    switch (size_) {
        case MarioSize::Big: return spriteBigStar_;
        case MarioSize::Fire: return spriteFireStar_;
        case MarioSize::Small: default: return spriteSmallStar_;
    }
}

void Mario::setInvincible(bool on, float duration) {
    invincible_ = on;
    invincibleTimer_ = on ? duration : 0.0f;
    starFlickerTimer_ = 0.0f;
    starCycleIndex_ = 0;
}

bool Mario::takeDamage() {
    if (invincible_ || hurtInvincible_ || inDamageSequence_) return false;
    if (size_ == MarioSize::Small) return false;  // "morir", todavia no implementado

    inDamageSequence_ = true;
    damageSequenceTimer_ = kDamageFreezeDuration;
    damageFlickerTimer_ = 0.0f;
    damageFlickerPhase_ = 0;
    velX_ = 0.0f;
    velY_ = 0.0f;
    return true;
}

void Mario::updateDamageSequence(float dt) {
    if (!inDamageSequence_) return;

    damageSequenceTimer_ -= dt;
    damageFlickerTimer_ += dt;
    while (damageFlickerTimer_ >= kDamageFlickerInterval) {
        damageFlickerTimer_ -= kDamageFlickerInterval;
        damageFlickerPhase_ = (damageFlickerPhase_ + 1) % 4;
    }

    if (damageSequenceTimer_ <= 0.0f) {
        inDamageSequence_ = false;
        setSize(MarioSize::Small);
        grantRespawnInvincibility();
    }
}

void Mario::grantRespawnInvincibility(float duration) {
    hurtInvincible_ = true;
    hurtInvincibleTimer_ = duration;
    hurtFlickerTimer_ = 0.0f;
    hurtFlickerVisible_ = true;
}

bool Mario::startGrow(MarioSize targetSize) {
    // Transiciones validas: chico->grande, chico->fuego (flor en chico) y
    // grande->fuego (flor en grande, sin cambio de altura). Fuego ya es el
    // maximo, y no tiene sentido "crecer" al mismo tamano que ya tiene.
    if (size_ == MarioSize::Fire || size_ == targetSize) return false;
    const SpriteSheet& targetSprite = targetSize == MarioSize::Fire ? spriteFire_ : spriteBig_;
    if (!targetSprite.isLoaded()) return false;
    if (inDamageSequence_ || inGrowSequence_) return false;

    inGrowSequence_ = true;
    growTargetSize_ = targetSize;
    growSequenceTimer_ = kGrowDuration;
    growFlickerTimer_ = 0.0f;
    growFlickerPhase_ = 0;
    velX_ = 0.0f;
    velY_ = 0.0f;
    return true;
}

void Mario::updateGrowSequence(float dt) {
    if (!inGrowSequence_) return;

    growSequenceTimer_ -= dt;
    growFlickerTimer_ += dt;
    while (growFlickerTimer_ >= kGrowFlickerInterval) {
        growFlickerTimer_ -= kGrowFlickerInterval;
        // Solo 2 fases (actual/destino) sin frames invisibles en el medio:
        // se ve como un cambio de paleta rapido, no como un parpadeo con huecos.
        growFlickerPhase_ = (growFlickerPhase_ + 1) % 2;
    }

    if (growSequenceTimer_ <= 0.0f) {
        inGrowSequence_ = false;
        setSize(growTargetSize_);
    }
}

bool Mario::startDeath() {
    if (size_ != MarioSize::Small) return false;
    if (invincible_ || hurtInvincible_ || inDamageSequence_ || inGrowSequence_ || inDeathSequence_) return false;

    inDeathSequence_ = true;
    deathSequenceTimer_ = kDeathDuration;
    velX_ = 0.0f;
    velY_ = kDeathHopImpulse;
    return true;
}

void Mario::updateDeathSequence(float dt) {
    if (!inDeathSequence_) return;

    velY_ += kDeathGravity * dt;
    y_ += velY_ * dt;

    deathSequenceTimer_ -= dt;
    if (deathSequenceTimer_ <= 0.0f) {
        inDeathSequence_ = false;
        deathFinished_ = true;
    }
}

bool Mario::consumeDeathFinished() {
    const bool value = deathFinished_;
    deathFinished_ = false;
    return value;
}

void Mario::setSize(MarioSize newSize) {
    if (newSize == size_) return;
    if (newSize == MarioSize::Big && !spriteBig_.isLoaded()) return;
    if (newSize == MarioSize::Fire && !spriteFire_.isLoaded()) return;

    const int oldHeight = currentHeight();
    size_ = newSize;
    const int newHeight = currentHeight();
    // Mantiene los pies en el mismo lugar al crecer/achicarse.
    y_ -= static_cast<float>(newHeight - oldHeight);
}

void Mario::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    velX_ = 0.0f;
    velY_ = 0.0f;
    grounded_ = false;
}

void Mario::update(float dt, Input& input, const Tilemap& level) {
    if (inDeathSequence_) {
        // Se maneja solo (no depende de que Game congele todo): mientras el
        // otro jugador sigue jugando normal, este cae con la pose de
        // "muerto" hasta que termina la secuencia.
        updateDeathSequence(dt);
        return;
    }

    updateHorizontal(dt, input);
    updateVertical(dt, input);
    moveAndCollide(dt, level);

    // El boton de correr tambien tira fuego (tocado, no mantenido) cuando
    // se es Mario/Luigi fuego, igual que en el juego original.
    if (size_ == MarioSize::Fire && input.consumePress(Action::Run)) {
        wantsThrow_ = true;
    }

    updateAnimation(dt, input);

    if (invincible_) {
        invincibleTimer_ -= dt;
        if (invincibleTimer_ <= 0.0f) {
            invincible_ = false;
            starCycleIndex_ = 0;
        } else {
            starFlickerTimer_ += dt;
            while (starFlickerTimer_ >= kStarFlickerInterval) {
                starFlickerTimer_ -= kStarFlickerInterval;
                starCycleIndex_ = (starCycleIndex_ + 1) % 4;
            }
        }
    }

    stompCooldownTimer_ = std::max(0.0f, stompCooldownTimer_ - dt);

    if (hurtInvincible_) {
        hurtInvincibleTimer_ -= dt;
        if (hurtInvincibleTimer_ <= 0.0f) {
            hurtInvincible_ = false;
            hurtFlickerVisible_ = true;
        } else {
            hurtFlickerTimer_ += dt;
            while (hurtFlickerTimer_ >= kHurtFlickerInterval) {
                hurtFlickerTimer_ -= kHurtFlickerInterval;
                hurtFlickerVisible_ = !hurtFlickerVisible_;
            }
        }
    }
}

void Mario::playThrowAnimation() {
    throwAnimTimer_ = kThrowAnimDuration;
}

bool Mario::consumeThrowRequest() {
    const bool value = wantsThrow_;
    wantsThrow_ = false;
    return value;
}

void Mario::updateHorizontal(float dt, const Input& input) {
    const bool underwater = worldType_ == WorldType::Underwater;

    // Agachado: solo grande/fuego, parado en el piso, y no bajo el agua (ahi
    // no tiene sentido). Mientras se mantiene, no camina, solo frena.
    // El hitbox se achica igual que en el original (ver kCrouchHitboxHeight
    // en Mario.h) -- mismo truco que setSize() para mantener los pies en el
    // mismo lugar al entrar/salir de la posicion.
    const int heightBeforeDuck = currentHeight();
    isDucking_ = size_ != MarioSize::Small && grounded_ && !underwater && input.down(Action::Down);
    const int heightAfterDuck = currentHeight();
    if (heightAfterDuck != heightBeforeDuck) {
        y_ -= static_cast<float>(heightAfterDuck - heightBeforeDuck);
    }
    if (isDucking_) {
        if (velX_ > 0.0f) {
            velX_ = std::max(0.0f, velX_ - kFriction * dt);
        } else if (velX_ < 0.0f) {
            velX_ = std::min(0.0f, velX_ + kFriction * dt);
        }
        return;
    }

    const bool left = input.down(Action::MoveLeft);
    const bool right = input.down(Action::MoveRight);
    // Bajo el agua no hay dash (Action::Run no acelera mas), todo es mas lento.
    const bool running = !underwater && input.down(Action::Run);
    const float accel = underwater ? kSwimAccel : (running ? kRunAccel : kWalkAccel);
    const float maxSpeed = underwater ? kSwimMaxSpeed : (running ? kMaxRunSpeed : kMaxWalkSpeed);
    const float friction = underwater ? kSwimFriction : kFriction;

    if (left && !right) {
        velX_ -= accel * dt;
        facingRight_ = false;
    } else if (right && !left) {
        velX_ += accel * dt;
        facingRight_ = true;
    } else {
        // Sin input horizontal: friccion que frena hasta cero sin pasarse.
        if (velX_ > 0.0f) {
            velX_ = std::max(0.0f, velX_ - friction * dt);
        } else if (velX_ < 0.0f) {
            velX_ = std::min(0.0f, velX_ + friction * dt);
        }
    }

    velX_ = std::clamp(velX_, -maxSpeed, maxSpeed);
}

void Mario::updateVertical(float dt, Input& input) {
    if (worldType_ == WorldType::Underwater) {
        // Nado: cada apretada de saltar es una brazada hacia arriba, no hace
        // falta estar en el piso. Se hunde despacio si no se hace nada.
        if (input.consumePress(Action::Jump)) {
            velY_ = kSwimStrokeImpulse;
            swimStrokeBoostTimer_ = kSwimStrokeBoostDuration;
            swimStrokeSoundRequested_ = true;
        }
        velY_ = std::clamp(velY_ + kSwimGravity * dt, kSwimMaxRiseSpeed, kSwimMaxSinkSpeed);
        return;
    }

    const bool jumpHeld = input.down(Action::Jump);

    // consumePress() se llama SIEMPRE, no solo si grounded_ -- si no, el
    // "latch" (ver el comentario de Input.h) de una apretada en el aire
    // queda pendiente sin consumir y dispara un salto solo apenas aterriza,
    // aunque el jugador ya haya soltado el boton.
    const bool jumpPressed = input.consumePress(Action::Jump);
    // A partir de kRunJumpThreshold (umbral real del original, ver Mario.h),
    // salto un poco mas alto -- afecta tanto el impulso inicial como la
    // gravedad durante todo el salto (los dos juegos de valores van juntos
    // en el original). Se recalcula por velocidad ACTUAL cada frame en vez
    // de quedar fijo con la velocidad de cuando se salto (simplificacion:
    // el original la fija al saltar y no la vuelve a tocar hasta aterrizar,
    // la diferencia practica es minima).
    const bool fastArc = std::abs(velX_) >= kRunJumpThreshold;
    if (grounded_ && jumpPressed) {
        velY_ = -(fastArc ? kRunJumpImpulse : kJumpImpulse);
        grounded_ = false;
        jumpRequested_ = true;
    }

    // Salto de altura variable: mientras se sube y se mantiene el boton,
    // gravedad debil (sostiene el salto); si se suelta antes o se esta
    // cayendo, gravedad fuerte (corta el salto / caida rapida).
    const bool ascending = velY_ < 0.0f;
    const float gravityAscend = fastArc ? kGravityAscendRun : kGravityAscend;
    const float gravityDescend = fastArc ? kGravityDescendRun : kGravityDescend;
    const float gravity = (ascending && jumpHeld) ? gravityAscend : gravityDescend;
    velY_ = std::min(velY_ + gravity * dt, kMaxFallSpeed);
}

namespace {
// Margen chico para excluir el toque exacto de un borde de tile sin perder
// colisiones reales de menos de 1px (las posiciones son flotantes, no
// enteras: restar 1px entero aca hacia que un solapamiento real pero chico
// -por ejemplo el par de decimas de pixel que la gravedad mete en un frame
// antes de resolver la colision- no se detectara).
constexpr float kEdgeEpsilon = 0.01f;
}  // namespace

void Mario::moveAndCollide(float dt, const Tilemap& level) {
    const int ts = level.tileSize();

    const int width = currentWidth();

    // --- Eje X ---
    float newX = x_ + velX_ * dt;
    {
        float left = newX;
        float right = newX + width;
        int row0 = static_cast<int>(std::floor(y_ / ts));
        int row1 = static_cast<int>(std::floor((y_ + currentHeight() - kEdgeEpsilon) / ts));
        int col0 = static_cast<int>(std::floor(left / ts));
        int col1 = static_cast<int>(std::floor((right - kEdgeEpsilon) / ts));

        if (velX_ > 0.0f) {
            for (int col = col0; col <= col1; ++col) {
                bool blocked = false;
                for (int row = row0; row <= row1; ++row) {
                    if (level.isSolidTile(col, row)) { blocked = true; break; }
                }
                if (blocked) {
                    newX = static_cast<float>(col * ts - width);
                    velX_ = 0.0f;
                    break;
                }
            }
        } else if (velX_ < 0.0f) {
            for (int col = col1; col >= col0; --col) {
                bool blocked = false;
                for (int row = row0; row <= row1; ++row) {
                    if (level.isSolidTile(col, row)) { blocked = true; break; }
                }
                if (blocked) {
                    newX = static_cast<float>((col + 1) * ts);
                    velX_ = 0.0f;
                    break;
                }
            }
        }
    }
    x_ = newX;

    // --- Eje Y ---
    const int height = currentHeight();
    float newY = y_ + velY_ * dt;
    grounded_ = false;
    headBumped_ = false;
    {
        float top = newY;
        float bottom = newY + height;
        int col0 = static_cast<int>(std::floor(x_ / ts));
        int col1 = static_cast<int>(std::floor((x_ + width - kEdgeEpsilon) / ts));
        int row0 = static_cast<int>(std::floor(top / ts));
        int row1 = static_cast<int>(std::floor((bottom - kEdgeEpsilon) / ts));

        if (velY_ > 0.0f) {
            for (int row = row0; row <= row1; ++row) {
                bool blocked = false;
                for (int col = col0; col <= col1; ++col) {
                    if (level.isSolidTile(col, row)) { blocked = true; break; }
                }
                if (blocked) {
                    newY = static_cast<float>(row * ts - height);
                    velY_ = 0.0f;
                    grounded_ = true;
                    break;
                }
            }
        } else if (velY_ < 0.0f) {
            for (int row = row1; row >= row0; --row) {
                bool blocked = false;
                for (int col = col0; col <= col1; ++col) {
                    if (level.isSolidTile(col, row)) { blocked = true; break; }
                }
                if (blocked) {
                    newY = static_cast<float>((row + 1) * ts);
                    velY_ = 0.0f;
                    // Choco de cabeza saltando: Game.cpp chequea si alguna
                    // de estas columnas era un ladrillo rompible (ver
                    // headBumpedThisFrame() y Game::checkBrickBlockHit).
                    headBumped_ = true;
                    headBumpRow_ = row;
                    headBumpCol0_ = col0;
                    headBumpCol1_ = col1;
                    break;
                }
            }
        }
    }
    y_ = newY;
}

namespace {
constexpr int kAnimRowIdle = 0;
constexpr int kAnimRowWalk = 1;
constexpr int kAnimRowSkid = 2;
constexpr int kAnimRowJump = 3;
constexpr int kAnimRowThrow = 7;
constexpr int kAnimRowSwim = 6;
constexpr int kAnimRowDie = 4;  // solo existe en el sheet chico (grande/fuego tiene "duck" en ese indice)
constexpr int kAnimRowDuck = 4; // solo existe en los sheets grande/fuego (el chico tiene "die" en ese indice)
constexpr int kWalkFrameCount = 3;
constexpr int kSwimFrameCount = 5;
// Segun la hoja de referencia: la pose de tirar fuego solo reemplaza los 24px
// de arriba del sprite; los 8px de abajo (piernas) quedan con el frame de
// movimiento que este jugando en ese momento.
constexpr int kThrowTopHeight = 24;
}  // namespace

void Mario::updateAnimation(float dt, const Input& input) {
    throwAnimTimer_ = std::max(0.0f, throwAnimTimer_ - dt);

    if (bumpAnimTimer_ > 0.0f) {
        bumpAnimTimer_ = std::max(0.0f, bumpAnimTimer_ - dt);
        animState_ = MarioAnim::Skid;
        walkAnimTimer_ = 0.0f;
        walkFrame_ = 0;
        swimAnimTimer_ = 0.0f;
        swimFrame_ = 0;
        return;
    }

    if (isDucking_) {
        animState_ = MarioAnim::Duck;
        walkAnimTimer_ = 0.0f;
        walkFrame_ = 0;
        return;
    }

    if (worldType_ == WorldType::Underwater) {
        animState_ = MarioAnim::Swim;
        walkAnimTimer_ = 0.0f;
        walkFrame_ = 0;
        // El ciclo de nado avanza solo, sostenido (a diferencia de caminar,
        // nadar se ve igual floteando o moviendose), pero se acelera un
        // instante cada vez que se da una brazada (tecla de saltar).
        swimStrokeBoostTimer_ = std::max(0.0f, swimStrokeBoostTimer_ - dt);
        const float fps = swimStrokeBoostTimer_ > 0.0f ? kSwimStrokeFps : kSwimBaseFps;
        swimAnimTimer_ += dt * fps;
        while (swimAnimTimer_ >= 1.0f) {
            swimAnimTimer_ -= 1.0f;
            swimFrame_ = (swimFrame_ + 1) % kSwimFrameCount;
        }
        return;
    }
    swimAnimTimer_ = 0.0f;
    swimFrame_ = 0;

    const bool pressingLeft = input.down(Action::MoveLeft);
    const bool pressingRight = input.down(Action::MoveRight);
    const bool skidding = grounded_ &&
        ((velX_ > kSkidVelocityThreshold && pressingLeft) ||
         (velX_ < -kSkidVelocityThreshold && pressingRight));
    if (!grounded_) {
        animState_ = MarioAnim::Jump;
    } else if (skidding) {
        animState_ = MarioAnim::Skid;
    } else if (std::fabs(velX_) > 1.0f) {
        animState_ = MarioAnim::Walk;
    } else {
        animState_ = MarioAnim::Idle;
    }

    if (animState_ != MarioAnim::Walk) {
        walkAnimTimer_ = 0.0f;
        walkFrame_ = 0;
        return;
    }

    // La velocidad del ciclo de caminata: 3 escalones fijos por velocidad
    // (PlayerAnimTmrData del original), no interpolacion continua.
    const float absSpeed = std::fabs(velX_);
    const float framesPerSecond = absSpeed >= kAnimSpeedFastThreshold     ? kAnimFpsFast
                                   : absSpeed >= kAnimSpeedMediumThreshold ? kAnimFpsMedium
                                                                            : kAnimFpsSlow;
    walkAnimTimer_ += dt * framesPerSecond;
    while (walkAnimTimer_ >= 1.0f) {
        walkAnimTimer_ -= 1.0f;
        walkFrame_ = (walkFrame_ + 1) % kWalkFrameCount;
    }
}

void Mario::render(SDL_Renderer* renderer, float cameraX) const {
    const int drawXBase = static_cast<int>(std::lround(x_ - cameraX)) - (kSpriteFrameWidth - currentWidth()) / 2;
    // El frame del sprite es mas alto que el hitbox real (ver el comentario
    // de kSmallHitboxWidth en Mario.h): se dibuja mas arriba de y_ por esta
    // diferencia para que la base (pies) coincida con el piso del hitbox en
    // vez de con el fondo del frame completo.
    const int spriteYOffset = currentSpriteFrameHeight() - currentHeight();

    // Se calcula antes de los casos especiales: la secuencia de crecer lo
    // reusa para animar "sobre" la pose que estaba jugando (caminar, salto,
    // etc.) en vez de forzar quieto.
    int row = kAnimRowIdle;
    int col = 0;
    switch (animState_) {
        case MarioAnim::Idle: row = kAnimRowIdle; col = 0; break;
        case MarioAnim::Walk: row = kAnimRowWalk; col = walkFrame_; break;
        case MarioAnim::Skid: row = kAnimRowSkid; col = 0; break;
        case MarioAnim::Jump: row = kAnimRowJump; col = 0; break;
        case MarioAnim::Swim: row = kAnimRowSwim; col = swimFrame_; break;
        case MarioAnim::Duck: row = kAnimRowDuck; col = 0; break;
        case MarioAnim::Throw: break;  // no se asigna como estado real, ver mas abajo
    }

    if (inDeathSequence_) {
        // Pose fija de "muerto" mientras salta/cae, siempre chico.
        const int deadDrawY = static_cast<int>(std::lround(y_)) - (kSmallHeight - kSmallHitboxHeight);
        spriteSmall_.draw(renderer, kAnimRowDie, 0, drawXBase, deadDrawY, !facingRight_);
        return;
    }

    if (inDamageSequence_) {
        // Congelado, parpadeando: grande -> invisible -> chico -> invisible
        // (pies siempre en el mismo lugar).
        if (damageFlickerPhase_ == 0) {
            const int bigDrawY = static_cast<int>(std::lround(y_)) - spriteYOffset;
            currentSprite().draw(renderer, kAnimRowIdle, 0, drawXBase, bigDrawY, !facingRight_);
        } else if (damageFlickerPhase_ == 2) {
            const int smallDrawY = static_cast<int>(std::lround(y_)) + (currentHeight() - kSmallHeight);
            spriteSmall_.draw(renderer, kAnimRowIdle, 0, drawXBase, smallDrawY, !facingRight_);
        }
        // fases 1 y 3: no se dibuja nada (parpadeo).
        return;
    }

    if (inGrowSequence_) {
        // Congelado, cambiando de paleta rapido entre la forma actual y la
        // destino (como el parpadeo de la estrella, pero entre 2 formas en
        // vez de 4), sobre el mismo frame/pose que estaba jugando. size_
        // todavia no cambio, asi que currentSprite()/currentHeight()
        // reflejan la forma actual sea cual sea (chico->grande/fuego, o
        // grande->fuego sin cambio de altura).
        if (growFlickerPhase_ == 0) {
            const int currentDrawY = static_cast<int>(std::lround(y_)) - spriteYOffset;
            currentSprite().draw(renderer, row, col, drawXBase, currentDrawY, !facingRight_);
        } else {
            const int targetDrawY = static_cast<int>(std::lround(y_)) - (kBigHeight - currentHeight());
            const SpriteSheet& targetSprite = growTargetSize_ == MarioSize::Fire ? spriteFire_ : spriteBig_;
            targetSprite.draw(renderer, row, col, drawXBase, targetDrawY, !facingRight_);
        }
        return;
    }

    if (hurtInvincible_ && !hurtFlickerVisible_) {
        return;  // frame "invisible" del parpadeo post-danio
    }

    const int drawX = drawXBase;
    const int drawY = static_cast<int>(std::lround(y_)) - spriteYOffset;

    // Estrella: mientras dura, se usa el sheet de paletas alternativas en vez
    // del normal, con un offset de filas segun el tipo de mundo (cada uno
    // tiene sus 3 paletas propias) y el paso del ciclo (0 = paleta normal,
    // se queda con currentSprite()).
    const bool useStarPalette = invincible_ && starCycleIndex_ > 0 && currentStarSprite().isLoaded();
    const SpriteSheet& sprite = useStarPalette ? currentStarSprite() : currentSprite();
    const int rowOffset = useStarPalette
        ? (static_cast<int>(worldType_) * 3 + (starCycleIndex_ - 1)) * currentSprite().rowCount()
        : 0;

    if (throwAnimTimer_ > 0.0f && size_ == MarioSize::Fire) {
        // Tecnica del juego original: se componen dos capas en vez de un frame
        // unico (ver comentario de kThrowTopHeight).
        sprite.drawSlice(renderer, kAnimRowThrow + rowOffset, 0, 0, kThrowTopHeight, drawX, drawY, !facingRight_);
        sprite.drawSlice(renderer, row + rowOffset, col, kThrowTopHeight, currentSpriteFrameHeight() - kThrowTopHeight,
                          drawX, drawY, !facingRight_);
        return;
    }

    sprite.draw(renderer, row + rowOffset, col, drawX, drawY, !facingRight_);
}
