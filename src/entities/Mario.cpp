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

bool Mario::loadHammerSprite(SDL_Renderer* renderer, const std::string& path, const std::string& starPath) {
    if (!starPath.empty()) spriteHammerStar_.loadFromFile(renderer, starPath, kSpriteFrameWidth, kHammerHeight);
    return spriteHammer_.loadFromFile(renderer, path, kSpriteFrameWidth, kHammerHeight);
}

bool Mario::loadBoomerangSprite(SDL_Renderer* renderer, const std::string& path, const std::string& starPath) {
    if (!starPath.empty()) spriteBoomerangStar_.loadFromFile(renderer, starPath, kSpriteFrameWidth, kHammerHeight);
    return spriteBoomerang_.loadFromFile(renderer, path, kSpriteFrameWidth, kHammerHeight);
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
    if (isSuitSize(size_)) return kHammerHeight;
    return size_ == MarioSize::Small ? kSmallHeight : kBigHeight;
}

bool Mario::loadPaletteSprites(SDL_Renderer* renderer, WorldType type, const std::string& smallPath,
                               const std::string& bigPath, const std::string& firePath) {
    const int index = static_cast<int>(type);
    if (index == activePalette_) return false;
    std::array<SpriteSheet, 3>& slot = paletteSprites_[static_cast<size_t>(index)];
    if (!slot[0].loadFromFile(renderer, smallPath, kSpriteFrameWidth, kSmallHeight) ||
        !slot[1].loadFromFile(renderer, bigPath, kSpriteFrameWidth, kBigHeight) ||
        !slot[2].loadFromFile(renderer, firePath, kSpriteFrameWidth, kBigHeight)) {
        return false;
    }
    paletteLoaded_[static_cast<size_t>(index)] = true;
    return true;
}

void Mario::setWorldType(WorldType type) {
    worldType_ = type;
    // Paleta por tipo de mundo (subterraneo, castillo, agua): desactivada por
    // ahora (sWorldPalettes, ver setWorldPalettesEnabled); el tipo de mundo
    // sigue mandando en la fisica (nado) igual.
    const int wanted =
        sWorldPalettes && paletteLoaded_[static_cast<size_t>(type)] ? static_cast<int>(type) : 0;
    if (wanted == activePalette_) return;
    // Devuelve la activa a su lugar y trae la nueva.
    std::array<SpriteSheet, 3>& current = paletteSprites_[static_cast<size_t>(activePalette_)];
    spriteSmall_.swap(current[0]);
    spriteBig_.swap(current[1]);
    spriteFire_.swap(current[2]);
    std::array<SpriteSheet, 3>& next = paletteSprites_[static_cast<size_t>(wanted)];
    spriteSmall_.swap(next[0]);
    spriteBig_.swap(next[1]);
    spriteFire_.swap(next[2]);
    activePalette_ = wanted;
}

void Mario::swapAppearance(Mario& other) {
    // Las paletas por mundo tienen que estar en el mismo estado (la activa
    // en los sprites principales): se vuelve las dos a la normal primero.
    const WorldType mine = worldType_;
    const WorldType theirs = other.worldType_;
    setWorldType(WorldType::Overworld);
    other.setWorldType(WorldType::Overworld);
    spriteSmall_.swap(other.spriteSmall_);
    spriteBig_.swap(other.spriteBig_);
    spriteFire_.swap(other.spriteFire_);
    spriteHammer_.swap(other.spriteHammer_);
    spriteHammerStar_.swap(other.spriteHammerStar_);
    spriteSmallStar_.swap(other.spriteSmallStar_);
    spriteBigStar_.swap(other.spriteBigStar_);
    spriteFireStar_.swap(other.spriteFireStar_);
    for (size_t w = 0; w < paletteSprites_.size(); ++w) {
        for (size_t i = 0; i < paletteSprites_[w].size(); ++i) paletteSprites_[w][i].swap(other.paletteSprites_[w][i]);
    }
    std::swap(paletteLoaded_, other.paletteLoaded_);
    setWorldType(mine);
    other.setWorldType(theirs);
}

const SpriteSheet& Mario::currentSprite() const {
    switch (size_) {
        case MarioSize::Big: return spriteBig_;
        case MarioSize::Fire: return spriteFire_;
        case MarioSize::Hammer: return spriteHammer_;
        case MarioSize::Boomerang: return spriteBoomerang_;
        case MarioSize::Small: default: return spriteSmall_;
    }
}

const SpriteSheet& Mario::currentStarSprite() const {
    switch (size_) {
        case MarioSize::Big: return spriteBigStar_;
        case MarioSize::Fire: return spriteFireStar_;
        case MarioSize::Hammer: return spriteHammerStar_;
        case MarioSize::Boomerang: return spriteBoomerangStar_;
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
    if (invincible_ || hurtInvincible_ || inDamageSequence_ || isOnFlagpole()) return false;
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
    // El traje de Hammer Bro se pone desde cualquier forma.
    if (size_ == targetSize) return false;
    // Entre fuego y los trajes se puede pasar de uno a otro; de ahi no se
    // vuelve a grande ni a chico creciendo.
    const auto topForm = [](MarioSize size) { return size == MarioSize::Fire || isSuitSize(size); };
    if (topForm(size_) && !topForm(targetSize)) return false;
    const SpriteSheet& targetSprite = targetSize == MarioSize::Fire        ? spriteFire_
                                      : targetSize == MarioSize::Hammer    ? spriteHammer_
                                      : targetSize == MarioSize::Boomerang ? spriteBoomerang_
                                                                           : spriteBig_;
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
    if (isOnFlagpole()) return false;

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
    if (newSize == MarioSize::Hammer && !spriteHammer_.isLoaded()) return;
    if (newSize == MarioSize::Boomerang && !spriteBoomerang_.isLoaded()) return;

    const int oldHeight = currentHeight();
    size_ = newSize;
    const int newHeight = currentHeight();
    // Mantiene los pies en el mismo lugar al crecer/achicarse.
    y_ -= static_cast<float>(newHeight - oldHeight);
}

void Mario::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    climbing_ = false;
    climbAuto_ = false;
    vineIndex_ = -1;
    velX_ = 0.0f;
    velY_ = 0.0f;
    grounded_ = false;
    flagpolePhase_ = FlagpolePhase::None;
    // Secuencias a medio terminar del lugar de donde viene (por ejemplo,
    // cambiar de escenario mientras caia muerto): si no se cortan aca,
    // terminan en el nivel nuevo y cuentan ahi (le restaba una vida).
    inDeathSequence_ = false;
    deathFinished_ = false;
    inDamageSequence_ = false;
    inGrowSequence_ = false;
}

void Mario::update(float dt, Input& input, const Tilemap& level) {
    if (benched_) return;
    if (inDeathSequence_) {
        // Se maneja solo (no depende de que Game congele todo): mientras el
        // otro jugador sigue jugando normal, este cae con la pose de
        // "muerto" hasta que termina la secuencia.
        updateDeathSequence(dt);
        return;
    }

    if (isOnFlagpole()) {
        // Secuencia automatica de fin de nivel: ignora el input.
        updateFlagpole(dt, level);
        return;
    }

    vineGrabCooldown_ = std::max(0.0f, vineGrabCooldown_ - dt);
    if (!climbing_) tryGrabVine(input);
    if (climbing_) {
        updateClimb(dt, input, level);
    } else {
        updateHorizontal(dt, input);
        updateVertical(dt, input);
        moveAndCollide(dt, level);

        // El boton de correr tambien tira fuego (tocado, no mantenido) cuando
        // se es Mario/Luigi fuego, igual que en el juego original.
        if ((size_ == MarioSize::Fire || isSuitSize(size_)) && input.consumePress(Action::Run)) {
            wantsThrow_ = true;
        }

        updateAnimation(dt, input);
    }

    if (invincible_) {
        invincibleTimer_ -= dt;
        if (invincibleTimer_ <= 0.0f) {
            invincible_ = false;
            starCycleIndex_ = 0;
        } else {
            starFlickerTimer_ += dt;
            const float flickerInterval =
                invincibleTimer_ < kStarSlowFlickerAt ? kStarFlickerSlowInterval : kStarFlickerInterval;
            while (starFlickerTimer_ >= flickerInterval) {
                starFlickerTimer_ -= flickerInterval;
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

void Mario::stepWalkAnimation(float dt) {
    animState_ = MarioAnim::Walk;
    walkAnimTimer_ += dt;
    if (walkAnimTimer_ >= 0.1f) {
        walkAnimTimer_ = 0.0f;
        walkFrame_ = (walkFrame_ + 1) % 3;  // kWalkFrameCount (se define mas abajo)
    }
}

void Mario::playThrowAnimation() {
    throwAnimTimer_ = kThrowAnimDuration;
}

bool Mario::consumeThrowRequest() {
    // Como el original (ProcFireball_Bubble): agachado o trepando no se
    // tira fuego (el boton se ignora).
    const bool value = wantsThrow_ && !isDucking_ && !isClimbing();
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
    const int dir = (left && !right) ? -1 : (right && !left) ? 1 : 0;
    if (dir != 0 && (grounded_ || underwater)) facingRight_ = dir > 0;

    if (underwater) {
        // Nado: sin dash, todo mas lento (como estaba).
        if (dir != 0) {
            velX_ += static_cast<float>(dir) * kSwimAccel * dt;
        } else if (velX_ > 0.0f) {
            velX_ = std::max(0.0f, velX_ - kSwimFriction * dt);
        } else if (velX_ < 0.0f) {
            velX_ = std::min(0.0f, velX_ + kSwimFriction * dt);
        }
        velX_ = std::clamp(velX_, -kSwimMaxSpeed, kSwimMaxSpeed);
        return;
    }

    const float speed = std::abs(velX_);
    const float moving = velX_ > 0.0f ? 1.0f : (velX_ < 0.0f ? -1.0f : 0.0f);
    // Luigi de Lost Levels patina mas: frena con el 60% al soltar y al patinar.
    const float slide = lostLevels_ ? 0.6f : 1.0f;
    if (grounded_) {
        const bool running = input.down(Action::Run);
        const float maxSpeed = running ? kMaxRunSpeed : kMaxWalkSpeed;
        if (dir == 0) {
            // Soltar: frena hasta cero sin pasarse.
            velX_ = moving * std::max(0.0f, speed - kFriction * slide * dt);
        } else if (moving != 0.0f && static_cast<float>(dir) != moving) {
            // Patinando: frena fuerte; por debajo de kSkidTurnSpeed ya da la
            // vuelta y arranca para el otro lado.
            const float remaining = speed - kSkidDecel * slide * dt;
            velX_ = remaining > kSkidTurnSpeed ? moving * remaining : static_cast<float>(dir) * kMinWalkSpeed;
        } else {
            const float accel = running ? kRunAccel : kWalkAccel;
            float next = std::max(speed, kMinWalkSpeed) + accel * dt;
            // Ya iba mas rapido que el tope (solto correr): baja de a poco
            // con la friccion, no de golpe.
            if (next > maxSpeed) next = std::max(maxSpeed, speed - kFriction * dt);
            velX_ = static_cast<float>(dir) * next;
        }
        takeoffSpeed_ = std::abs(velX_);
        return;
    }

    // En el aire: el tope es el del despegue y soltar no frena.
    const float airMax = takeoffSpeed_ > kMaxWalkSpeed ? kMaxRunSpeed : kMaxWalkSpeed;
    if (dir == 0) return;
    const bool forward = moving == 0.0f || static_cast<float>(dir) == moving;
    float accel;
    if (forward) {
        accel = speed < kMaxWalkSpeed ? kWalkAccel : kRunAccel;
    } else if (speed >= kMaxWalkSpeed) {
        accel = kRunAccel;
    } else {
        accel = takeoffSpeed_ >= kAirBackFastTakeoff ? kFriction : kWalkAccel;
    }
    velX_ += static_cast<float>(dir) * accel * dt;
    velX_ = std::clamp(velX_, -std::max(airMax, speed), std::max(airMax, speed));
}

void Mario::updateVertical(float dt, Input& input) {
    if (worldType_ == WorldType::Underwater) {
        // Nado: cada apretada de saltar es una brazada hacia arriba, no hace
        // falta estar en el piso. Se hunde despacio si no se hace nada.
        if (input.consumePress(Action::Jump)) {
            velY_ = inWhirlpool_ ? kSwimStrokeImpulse / 2.0f : kSwimStrokeImpulse;
            swimStrokeBoostTimer_ = kSwimStrokeBoostDuration;
            swimStrokeSoundRequested_ = true;
        }
        const float gravity = kSwimGravity + (inWhirlpool_ ? kWhirlpoolPull : 0.0f);
        velY_ = std::clamp(velY_ + gravity * dt, kSwimMaxRiseSpeed, kSwimMaxSinkSpeed);
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
    // Luigi de Lost Levels: JumpMForceData/FallMForceData mas bajos (~82% y
    // ~80% de los de Mario): sube mas y cae un poco mas lento.
    const float gravityAscend = (fastArc ? kGravityAscendRun : kGravityAscend) * (lostLevels_ ? 0.82f : 1.0f);
    const float gravityDescend = (fastArc ? kGravityDescendRun : kGravityDescend) * (lostLevels_ ? 0.8f : 1.0f);
    if (!ascending || grounded_) springLaunch_ = false;
    // 0x70 por cuadro^2 = 1575 px/s^2 (kGravityDescend), corra o no.
    const float gravity = springLaunch_ ? kGravityDescend
                          : (ascending && jumpHeld) ? gravityAscend : gravityDescend;
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
    // Choque con el escenario como PlayerBGCollision del original (sin
    // copiar codigo): primero se mueve en los dos ejes y despues se prueba
    // el mapa en puntos fijos de la caja de 16x32 del jugador (la "de
    // objeto": los pies en el borde de abajo), no con la caja entera:
    // - Cabeza: UN punto en el centro (x+8; y+4 grande, y+2 grande nadando,
    //   y+18 chico o agachado). Subiendo, si ese punto esta en un bloque
    //   (al menos 4 px adentro de su fila) choca: empieza a caer a 1 px por
    //   cuadro, sin acomodar la posicion. Rozar un bloque con el costado de
    //   la cabeza no frena (se pasa de largo, como el original).
    // - Pies: dos puntos (x+3 y x+12, justo debajo de los pies). Cayendo o
    //   quieto, si alguno pisa un bloque: hundido menos de 5 px, se apoya
    //   arriba (asi corriendo se cruzan los pozos de un bloque y se sube a un
    //   escalon si apenas se hunde); 5 o mas, cuenta como pared.
    // - Costados: x+2 (izquierda) y x+13 (derecha), a 8 y 24 px de arriba
    //   (chico o agachado, solo a 24). Si toca, frena y lo corre 1 px por
    //   cuadro hacia afuera (ImpedePlayerMove).
    const int ts = level.tileSize();
    const int width = currentWidth();
    const int height = currentHeight();
    const float oldY = y_;

    x_ += velX_ * dt;
    float newY = y_ + velY_ * dt;
    grounded_ = false;
    headBumped_ = false;

    const auto cellOf = [ts](float px) { return static_cast<int>(std::floor(px / static_cast<float>(ts))); };
    const auto inCell = [ts](float py) {
        const float m = std::fmod(py, static_cast<float>(ts));
        return m < 0.0f ? m + static_cast<float>(ts) : m;
    };
    const auto solidAt = [&](float px, float py) { return level.isSolidTile(cellOf(px), cellOf(py)); };

    const bool smallBox = size_ == MarioSize::Small || isDucking_;
    const bool swimming = worldType_ == WorldType::Underwater;
    // La caja de objeto de 16 de ancho: el hitbox (10 u 12) va adentro.
    const float inset = size_ == MarioSize::Small ? 3.0f : 2.0f;
    const auto objX = [&] { return x_ - inset; };
    const auto objY = [&] { return newY + static_cast<float>(height) - 32.0f; };

    // Frena contra la pared de ese lado y queda pegado a ella. El original
    // lo corre 1 px por cuadro (ImpedePlayerMove) y empujando la pared
    // tiembla un pixel para un lado y para el otro; aca se lo deja justo al
    // ras (probeX: el punto de costado que toco), sin temblor.
    const auto impede = [&](bool right, float probeX) {
        if (right ? velX_ >= 0.0f : velX_ <= 0.0f) {
            velX_ = 0.0f;
            const float wall = static_cast<float>(cellOf(probeX) * ts);
            x_ += right ? (wall - 0.01f) - probeX : (wall + static_cast<float>(ts)) - probeX;
        }
    };

    // --- Cabeza ---
    if (velY_ < 0.0f) {
        const float hx = objX() + 8.0f;
        const float hy = objY() + (smallBox ? 18.0f : swimming ? 2.0f : 4.0f);
        const int col = cellOf(hx);
        const int row = cellOf(hy);
        const bool hit = level.isSolidTile(col, row) || level.isHiddenBlockTile(col, row);
        if (hit && inCell(hy) >= 4.0f) {
            velY_ = 60.0f;   // 1 px por cuadro hacia abajo
            headBumped_ = true;
            headBumpRow_ = row;
            headBumpCol0_ = col;
            headBumpCol1_ = col;
        }
    }

    // --- Pies ---
    if (velY_ >= 0.0f) {
        const float fy = objY() + 32.0f;
        const bool leftFoot = solidAt(objX() + 3.0f, fy);
        const bool rightFoot = solidAt(objX() + 12.0f, fy);
        if (leftFoot || rightFoot) {
            // El original suma la gravedad despues de moverse: en el cuadro
            // del contacto esta hundido un paso de caida menos que aca. Sobre
            // un tope de verdad (aire arriba) se descuenta ese paso, asi
            // corriendo se cruzan los pozos de un bloque igual que alla; en
            // una pared continua no (no se apoya adentro de la pared).
            const float footX = leftFoot ? objX() + 3.0f : objX() + 12.0f;
            const bool surface = !solidAt(footX, fy - static_cast<float>(ts));
            const float lag = surface ? std::max(0.0f, velY_ * dt) : 0.0f;
            if (inCell(fy) - lag >= 5.0f) {
                // Muy hundido: es una pared (hacia donde se mueve o mira).
                const bool right = velX_ != 0.0f ? velX_ > 0.0f : facingRight_;
                impede(right, right ? objX() + 13.0f : objX() + 2.0f);
                y_ = newY;
                return;
            }
            newY = static_cast<float>(cellOf(fy) * ts - height);
            velY_ = 0.0f;
            grounded_ = true;
        }
    }

    // --- Costados (el primero que toca, como el original) ---
    {
        const float upper = objY() + (smallBox ? 24.0f : 8.0f);
        const float lower = objY() + 24.0f;
        const float left = objX() + 2.0f;
        const float right = objX() + 13.0f;
        if (solidAt(left, upper) || solidAt(left, lower)) {
            impede(false, left);
        } else if (solidAt(right, upper) || solidAt(right, lower)) {
            impede(true, right);
        }
    }
    (void)width;

    // Plataformas moviles: solo cayendo (o quieto encima) y si los pies
    // estaban por encima del tope antes de moverse (el que va montado ya
    // viene alineado, Game lo arrastra con la plataforma antes de esto).
    platformIndex_ = -1;
    if (!grounded_ && velY_ >= 0.0f && platforms_ != nullptr) {
        constexpr float kPlatformSnapPx = 3.0f;
        const float oldBottom = oldY + static_cast<float>(height);
        const float newBottom = newY + static_cast<float>(height);
        for (size_t i = 0; i < platforms_->size(); ++i) {
            const SDL_FRect& platform = (*platforms_)[i];
            if (platform.w <= 0.0f) continue;
            if (x_ >= platform.x + platform.w || x_ + static_cast<float>(width) <= platform.x) continue;
            if (oldBottom > platform.y + kPlatformSnapPx || newBottom < platform.y) continue;
            newY = platform.y - static_cast<float>(height);
            velY_ = 0.0f;
            grounded_ = true;
            platformIndex_ = static_cast<int>(i);
            break;
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
constexpr int kAnimRowClimb = 5; // trepar/bajar por el mastil, 2 frames (todos los sheets)
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
    } else if (std::fabs(velX_) > 1.0f || (pressingLeft != pressingRight)) {
        // Apretando una direccion camina aunque no avance (contra una
        // pared): el ciclo lento y constante del original, no un temblor
        // entre parado y caminando.
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
    if (benched_) return;
    if (hasFinishedLevel()) return;  // ya entro al castillo
    // Entrando al castillo: en los ultimos px antes de la puerta se funde a
    // negro (a pedido). Se restaura al salir de render, sea por donde sea.
    struct Fade {
        const Mario& m;
        Uint8 value;
        Fade(const Mario& mario, Uint8 v) : m(mario), value(v) { apply(value); }
        ~Fade() {
            if (value != 255) apply(255);
        }
        void apply(Uint8 v) const {
            for (const SpriteSheet* sheet : {&m.spriteSmall_, &m.spriteBig_, &m.spriteFire_, &m.spriteHammer_,
                                             &m.spriteSmallStar_,
                                             &m.spriteBigStar_, &m.spriteFireStar_}) {
                sheet->setBrightness(v);
            }
        }
    };
    Uint8 brightness = 255;
    if (flagpolePhase_ == FlagpolePhase::Walk) {
        constexpr float kFadeDistance = 24.0f;
        const float t = std::clamp((x_ - (flagpoleExitX_ - kFadeDistance)) / kFadeDistance, 0.0f, 1.0f);
        brightness = static_cast<Uint8>(std::lround(255.0f * (1.0f - t)));
    }
    const Fade fade(*this, brightness);
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
        case MarioAnim::Climb: row = kAnimRowClimb; col = climbFrame_; break;
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
            const int targetFrameH = isSuitSize(growTargetSize_) ? kHammerHeight : kBigHeight;
            const int targetDrawY = static_cast<int>(std::lround(y_)) - (targetFrameH - currentHeight());
            const SpriteSheet& targetSprite = growTargetSize_ == MarioSize::Fire        ? spriteFire_
                                              : growTargetSize_ == MarioSize::Hammer    ? spriteHammer_
                                              : growTargetSize_ == MarioSize::Boomerang ? spriteBoomerang_
                                                                                        : spriteBig_;
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

    if (throwAnimTimer_ > 0.0f && isSuitSize(size_) && !isDucking_) {
        // Trajes de Hammer/Boomerang Bro: como el de fuego, la pose de tirar
        // (2 cuadros) solo pone la parte de arriba del cuerpo; las piernas
        // siguen con el movimiento que este haciendo (el 2do cuadro de la
        // hoja no las trae). Con estrella, en su paleta (rowOffset).
        const int throwFrame = throwAnimTimer_ > kThrowAnimDuration / 2.0f ? 0 : 1;
        sprite.drawSlice(renderer, kAnimRowThrow + rowOffset, throwFrame, 0, kHammerThrowTopHeight, drawX, drawY,
                         !facingRight_);
        sprite.drawSlice(renderer, row + rowOffset, col, kHammerThrowTopHeight, kHammerHeight - kHammerThrowTopHeight,
                         drawX, drawY, !facingRight_);
        return;
    }
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

void Mario::startFlagpoleGrab(float poleCenterX, float bottomY, float exitTargetX) {
    if (isOnFlagpole() || inDeathSequence_) return;
    flagpolePhase_ = FlagpolePhase::Slide;
    flagpolePoleX_ = poleCenterX;
    flagpoleBottomY_ = bottomY;
    flagpoleExitX_ = exitTargetX;
    flagpoleTimer_ = 0.0f;
    climbFrame_ = 0;
    velX_ = 0.0f;
    velY_ = 0.0f;
    throwAnimTimer_ = 0.0f;
    facingRight_ = true;
    animState_ = MarioAnim::Climb;
    // Colgado a la izquierda del palo: el borde derecho del sprite (las
    // manos de la pose de trepar) apenas lo pisa. El sprite se dibuja
    // centrado sobre el hitbox (ver render()), de ahi el ajuste.
    const int spriteMargin = (kSpriteFrameWidth - currentWidth()) / 2;
    x_ = poleCenterX + 1.0f - static_cast<float>(currentWidth() + spriteMargin);
}

void Mario::releaseFromFlagpole() {
    if (flagpolePhase_ != FlagpolePhase::Bottom) return;
    // Se pasa al otro lado del palo, mirando hacia el (sprite espejado).
    flagpolePhase_ = FlagpolePhase::Flip;
    flagpoleTimer_ = kFlagpoleFlipDuration;
    facingRight_ = false;
    const int spriteMargin = (kSpriteFrameWidth - currentWidth()) / 2;
    x_ = flagpolePoleX_ - 1.0f + static_cast<float>(spriteMargin);
}

bool Mario::tryGrabVine(const Input& input) {
    (void)input;
    if (vines_ == nullptr || vineGrabCooldown_ > 0.0f || inDamageSequence_) return false;
    const float w = static_cast<float>(currentWidth());
    const float h = static_cast<float>(currentHeight());
    for (size_t i = 0; i < vines_->size(); ++i) {
        const SDL_FRect& vine = (*vines_)[i];
        if (vine.h <= 0.0f) continue;
        const float vineCenter = vine.x + vine.w / 2.0f;
        const float tileX = vineCenter - 8.0f;
        // Tocando la columna (el tile de 16 px) de costado o desde adentro.
        if (x_ >= tileX + 16.0f || x_ + w <= tileX) continue;
        if (y_ + h <= vine.y || y_ >= vine.y + vine.h) continue;
        climbing_ = true;
        climbAuto_ = false;
        climbSideTimer_ = 0.0f;
        vineIndex_ = static_cast<int>(i);
        vineX_ = vineCenter;
        velX_ = 0.0f;
        velY_ = 0.0f;
        grounded_ = false;
        // Del lado de donde vino, mirando a la enredadera.
        facingRight_ = x_ + w / 2.0f < vineCenter;
        snapToVineSide();
        animState_ = MarioAnim::Climb;
        return true;
    }
    return false;
}

void Mario::snapToVineSide() {
    // Player_X = columna*16 -7 (mirando a la derecha) o +7 (a la
    // izquierda); el sprite de 16 se dibuja centrado en el hitbox.
    const float tileX = vineX_ - 8.0f;
    const float spriteOffset = (static_cast<float>(kSpriteFrameWidth) - static_cast<float>(currentWidth())) / 2.0f;
    x_ = (facingRight_ ? tileX - 7.0f : tileX + 7.0f) + spriteOffset;
}

void Mario::startVineEntry(int vineIndex, float vineCenterX, float feetY, bool leftSide) {
    climbing_ = true;
    climbAuto_ = false;
    climbSideTimer_ = 0.0f;
    vineIndex_ = vineIndex;
    vineX_ = vineCenterX;
    velX_ = 0.0f;
    velY_ = 0.0f;
    grounded_ = false;
    facingRight_ = leftSide;
    snapToVineSide();
    y_ = feetY - static_cast<float>(currentHeight());
    animState_ = MarioAnim::Climb;
}

void Mario::stepOffVineRight() {
    if (!climbing_) return;
    x_ = vineX_ + 8.0f;
    facingRight_ = true;
    releaseVine();
}

void Mario::releaseVine() {
    climbing_ = false;
    climbAuto_ = false;
    vineIndex_ = -1;
    vineGrabCooldown_ = 0.3f;
    velY_ = 0.0f;
    animState_ = MarioAnim::Jump;
}

void Mario::updateClimb(float dt, Input& input, const Tilemap& level) {
    // (Puede medir 0 todavia: la de llegada a otra area crece con el
    // personaje esperando colgado abajo.)
    if (vines_ == nullptr || vineIndex_ < 0 || vineIndex_ >= static_cast<int>(vines_->size())) {
        releaseVine();
        return;
    }
    const SDL_FRect& vine = (*vines_)[static_cast<size_t>(vineIndex_)];
    const float w = static_cast<float>(currentWidth());
    const float h = static_cast<float>(currentHeight());
    animState_ = MarioAnim::Climb;

    float speed = 0.0f;
    if (climbAuto_ || input.down(Action::Up)) {
        speed = -kClimbUpSpeed;
    } else if (input.down(Action::Down)) {
        speed = kClimbDownSpeed;
    }
    y_ += speed * dt;
    if (speed < 0.0f && y_ < vine.y) y_ = vine.y;   // las manos no pasan la punta
    const float feet = y_ + h;
    if (climbAuto_ && feet <= climbAutoTargetFeet_) climbAuto_ = false;
    // Bajando: al tocar el piso se suelta parado; pasada la base, se cae.
    if (speed > 0.0f) {
        const int ts = level.tileSize();
        const int col = static_cast<int>(std::floor((x_ + w / 2.0f) / static_cast<float>(ts)));
        const int row = static_cast<int>(std::floor(feet / static_cast<float>(ts)));
        if (level.isSolidTile(col, row)) {
            y_ = static_cast<float>(row * ts) - h;
            releaseVine();
            grounded_ = true;
            animState_ = MarioAnim::Idle;
            return;
        }
    }
    // Pasada la base se suelta; en la de llegada a otra area la base es la
    // altura del piso y ahi se frena (no se vuelve a bajar).
    const bool floorBase = vineFloors_ != nullptr && static_cast<size_t>(vineIndex_) < vineFloors_->size() &&
                           (*vineFloors_)[static_cast<size_t>(vineIndex_)] != 0;
    if (speed > 0.0f && feet > vine.y + vine.h) {
        if (floorBase) {
            y_ = vine.y + vine.h - h;
        } else if (feet > vine.y + vine.h + 8.0f) {
            releaseVine();
            return;
        }
    }
    // Costados: hacia la enredadera pasa al otro lado (14 px, ClimbingSub)
    // y se da vuelta; hacia afuera no hace nada. Solo se suelta con una
    // direccion + salto: salta hacia ese lado (pedido del usuario).
    const bool left = input.down(Action::MoveLeft);
    const bool right = input.down(Action::MoveRight);
    if (!climbAuto_ && (left != right)) {
        if (input.consumePress(Action::Jump)) {
            facingRight_ = right;
            x_ = right ? vineX_ + 8.0f : vineX_ - 8.0f - w;
            releaseVine();
            velX_ = right ? kMaxWalkSpeed : -kMaxWalkSpeed;
            velY_ = -kJumpImpulse;
            jumpRequested_ = true;   // suena el salto
            return;
        }
        const bool towardVine = right == facingRight_;
        if (towardVine && climbSideTimer_ <= 0.0f) {
            climbSideTimer_ = 24.0f / 60.0f;
            facingRight_ = !facingRight_;
            snapToVineSide();
        } else if (climbSideTimer_ > 0.0f) {
            climbSideTimer_ -= dt;
        }
    } else {
        climbSideTimer_ = 0.0f;
        // Salto sin direccion: se descarta (no se suelta).
        if (!climbAuto_) input.consumePress(Action::Jump);
    }
    if (speed != 0.0f) {
        // 8 cuadros por paso subiendo, 4 bajando (PlayerAnimTimerSet).
        climbAnimTimer_ += dt;
        const float interval = (speed < 0.0f ? 8.0f : 4.0f) / 60.0f;
        if (climbAnimTimer_ >= interval) {
            climbAnimTimer_ = 0.0f;
            climbFrame_ = 1 - climbFrame_;
        }
    }
}

void Mario::updateFlagpole(float dt, const Tilemap& level) {
    switch (flagpolePhase_) {
        case FlagpolePhase::Slide: {
            y_ += kFlagpoleSlideSpeed * dt;
            flagpoleTimer_ += dt;
            if (flagpoleTimer_ >= kClimbFrameInterval) {
                flagpoleTimer_ = 0.0f;
                climbFrame_ = 1 - climbFrame_;
            }
            const float bottomTop = flagpoleBottomY_ - static_cast<float>(currentHeight());
            if (y_ >= bottomTop) {
                y_ = bottomTop;
                climbFrame_ = 0;
                flagpolePhase_ = FlagpolePhase::Bottom;
            }
            break;
        }
        case FlagpolePhase::Bottom:
            break;  // espera a releaseFromFlagpole()
        case FlagpolePhase::Flip:
            flagpoleTimer_ -= dt;
            if (flagpoleTimer_ <= 0.0f) {
                flagpolePhase_ = FlagpolePhase::Walk;
                facingRight_ = true;
                animState_ = MarioAnim::Walk;
                walkFrame_ = 0;
                walkAnimTimer_ = 0.0f;
                flagpoleTimer_ = 0.0f;
            }
            break;
        case FlagpolePhase::Walk:
            // Se baja del bloque base y camina solo, con gravedad normal.
            velX_ = kMaxWalkSpeed;
            velY_ = std::min(velY_ + kGravityDescend * dt, kMaxFallSpeed);
            moveAndCollide(dt, level);
            walkAnimTimer_ += dt;
            if (walkAnimTimer_ >= kClimbFrameInterval * 1.5f) {
                walkAnimTimer_ = 0.0f;
                walkFrame_ = (walkFrame_ + 1) % kWalkFrameCount;
            }
            // El tope de tiempo es un seguro por si algo del nivel le corta
            // el paso (una pared antes de la puerta): no se queda trabado.
            flagpoleTimer_ += dt;
            if (x_ >= flagpoleExitX_ || flagpoleTimer_ >= kFlagpoleWalkMaxDuration) {
                flagpolePhase_ = FlagpolePhase::Done;
            }
            break;
        case FlagpolePhase::Done:
        case FlagpolePhase::None:
            break;
    }
}

bool Mario::killInstantly(bool hop) {
    if (inDeathSequence_ || isOnFlagpole()) return false;
    inDamageSequence_ = false;
    inGrowSequence_ = false;
    invincible_ = false;
    hurtInvincible_ = false;
    setSize(MarioSize::Small);
    if (!startDeath()) return false;
    if (!hop) velY_ = 0.0f;
    return true;
}
