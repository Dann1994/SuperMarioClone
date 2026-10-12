#include "HammerBro.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

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

bool randomChance(float probability) { return randomRange(0.0f, 1.0f) < probability; }
}  // namespace

void HammerBro::spawn(float x, float y, bool movingRight) {
    x_ = x;
    y_ = y;
    velX_ = movingRight ? kWalkSpeed : -kWalkSpeed;
    velY_ = 0.0f;
    alive_ = true;
    falling_ = false;
    lastFacingRight_ = movingRight;
    jumpTimer_ = randomRange(kJumpIntervalMin, kJumpIntervalMax);
    jumpCommitTimer_ = 0.0f;
    grounded_ = false;
    onPlatform_ = false;
    throwTimer_ = kThrowInterval;
    throwWindupTimer_ = 0.0f;
    throwRequested_ = false;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void HammerBro::reset() {
    alive_ = false;
    falling_ = false;
    throwWindupTimer_ = 0.0f;
    throwRequested_ = false;
}

bool HammerBro::killFalling(bool flyRight) {
    if (!alive_ || falling_) return false;
    falling_ = true;
    velX_ = flyRight ? kFallKillHorizontalSpeed : -kFallKillHorizontalSpeed;
    velY_ = kFallKillHopImpulse;
    animTimer_ = 0.0f;
    animFrame_ = 0;
    return true;
}

void HammerBro::faceTowards(float targetX) {
    lastFacingRight_ = targetX >= x_ + static_cast<float>(kWidth) / 2.0f;
}

bool HammerBro::consumeThrowRequest() {
    const bool value = throwRequested_;
    throwRequested_ = false;
    return value;
}

float HammerBro::throwOriginX() const { return x_ + static_cast<float>(kWidth) / 2.0f; }
float HammerBro::throwOriginY() const { return y_; }
float HammerBro::throwVelocityX() const { return lastFacingRight_ ? kHammerSpeedX : -kHammerSpeedX; }
float HammerBro::throwVelocityY() const { return kHammerSpeedY; }

bool HammerBro::applyVerticalPhysics(float dt, const Tilemap& level) {
    const int ts = level.tileSize();
    velY_ = std::min(velY_ + kGravity * dt, kMaxFallSpeed);
    float newY = y_ + velY_ * dt;
    bool landed = false;
    bool blocked = false;
    // Mientras dura el "compromiso" del salto, ni siquiera pregunta si hay
    // algo abajo -- pura parabola por gravedad (ver el comentario de la
    // clase). Recien cuando se cierra esa ventana vuelve a poder aterrizar.
    bool blockedByPlatform = false;
    if (velY_ > 0.0f && jumpCommitTimer_ <= 0.0f) {
        const int col0 = static_cast<int>(std::floor(x_ / ts));
        const int col1 = static_cast<int>(std::floor((x_ + kWidth - kEdgeEpsilon) / ts));
        const int row = static_cast<int>(std::floor((newY + kHeight - kEdgeEpsilon) / ts));
        for (int col = col0; col <= col1; ++col) {
            if (level.isSolidTile(col, row)) { blocked = true; blockedByPlatform = false; break; }
            if (level.isPlatformTile(col, row)) { blocked = true; blockedByPlatform = true; }
        }
        if (blocked) {
            newY = static_cast<float>(row * ts - kHeight);
            velY_ = 0.0f;
            landed = true;
        }
    }
    // Piso "estable" (no solo el instante de aterrizar): usado para no
    // dejar que un salto se dispare de nuevo mientras todavia esta en el
    // aire de uno anterior, y para decidir si el proximo salto puede ser
    // "de bajada" (ver update()).
    grounded_ = blocked;
    if (blocked) onPlatform_ = blockedByPlatform;
    y_ = newY;
    return landed;
}

void HammerBro::applyHorizontalWithWallBounce(float dt, const Tilemap& level) {
    const int ts = level.tileSize();
    const float newX = x_ + velX_ * dt;
    const float edgeX = velX_ > 0.0f ? newX + kWidth - kEdgeEpsilon : newX;
    const int col = static_cast<int>(std::floor(edgeX / ts));
    const int col0 = static_cast<int>(std::floor(x_ / ts));
    const int col1 = static_cast<int>(std::floor((x_ + kWidth - kEdgeEpsilon) / ts));
    const int row0 = static_cast<int>(std::floor(y_ / ts));
    const int row1 = static_cast<int>(std::floor((y_ + kHeight - kEdgeEpsilon) / ts));
    bool blocked = false;
    for (int row = row0; row <= row1; ++row) {
        if (level.isSolidTile(col, row)) { blocked = true; break; }
    }
    if (!blocked) {
        // Le teme a los bordes siempre (no es un eje de color aparte como
        // el Koopa rojo, ver el comentario de la clase) -- mismo chequeo
        // que Koopa::applyHorizontalWithWallBounce, exige estar apoyado en
        // el piso AHORA (columnas propias, no el borde de avance) para no
        // darse vuelta en falso mientras esta en el aire saltando. El piso
        // "de verdad" ('#') y una plataforma ('=') cuentan igual como piso
        // aca -- patrulla dentro de los bordes de cualquiera de las dos.
        const int floorRow = row1 + 1;
        const bool currentlyGrounded = level.isSolidTile(col0, floorRow) || level.isSolidTile(col1, floorRow) ||
                                        level.isPlatformTile(col0, floorRow) || level.isPlatformTile(col1, floorRow);
        const bool floorAhead = level.isSolidTile(col, floorRow) || level.isPlatformTile(col, floorRow);
        // Si no hay piso adelante PERO ahi habia un ladrillo que se rompio
        // (ver Tilemap::wasBrickTile), no es un borde de los que le dan
        // miedo -- es un agujero hecho a proposito, lo cruza caminando y se
        // cae, como pide el diseno del nivel de prueba.
        const bool intentionalHole = !floorAhead && level.wasBrickTile(col, floorRow);
        if (currentlyGrounded && !floorAhead && !intentionalHole) {
            blocked = true;
        }
    }
    if (blocked) {
        // Solo invierte la caminata -- hacia donde MIRA es independiente,
        // ver faceTowards() y el comentario de la clase.
        velX_ = -velX_;
    } else {
        x_ = newX;
    }
}

void HammerBro::update(float dt, const Tilemap& level, int screenHeight) {
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

    walkFlipTimer_ -= dt;
    if (walkFlipTimer_ <= 0.0f) {
        walkFlipTimer_ += kWalkFlipInterval;
        velX_ = -velX_;
    }
    applyHorizontalWithWallBounce(dt, level);

    if (jumpCommitTimer_ > 0.0f) {
        jumpCommitTimer_ = std::max(0.0f, jumpCommitTimer_ - dt);
    }
    applyVerticalPhysics(dt, level);

    jumpTimer_ -= dt;
    if (jumpTimer_ <= 0.0f && grounded_) {
        // Parado sobre una plataforma (no el piso de verdad): a veces, al
        // azar, este salto es el de bajar -- mas corto/bajo (no hace falta
        // tomar altura) pero con ventana mas larga, para atravesar la
        // plataforma de punta a punta en vez de rebotar o trepar (ver el
        // comentario de la clase).
        const bool descending = onPlatform_ && randomChance(kDescendChance);
        velY_ = descending ? kDescendJumpImpulse : kJumpImpulse;
        jumpCommitTimer_ = descending ? kDescendCommitDuration : kJumpCommitDuration;
        jumpTimer_ = randomRange(kJumpIntervalMin, kJumpIntervalMax);
    } else if (jumpTimer_ <= 0.0f) {
        // Todavia en el aire de un salto anterior cuando toco el proximo:
        // espera al aterrizaje en vez de saltar de nuevo en pleno vuelo.
        jumpTimer_ = 0.0f;
    }

    if (throwWindupTimer_ > 0.0f) {
        // Ya esta con el martillo levantado (ver isWindingUp()): cuando se
        // termina el amague recien ahi lo tira de verdad.
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

    if (y_ > level.heightPixels()) {
        alive_ = false;
    }
}

void HammerBro::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;

    const int col = falling_ ? 0 : animFrame_;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // El arte fuente mira a la izquierda por defecto (mismo criterio que
    // Koopa/Buzzy Beetle): se flipea al reves que Mario/Goomba, mirando a
    // la derecha SI se flipea, no si no. Boca abajo (flip vertical) solo
    // mientras cae muerto, igual que el resto del elenco sin arte propio de
    // muerte.
    sprite.draw(renderer, worldRow, col, drawX, drawY, lastFacingRight_, falling_);
}
