#include "Blooper.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Difficulty.h"

void Blooper::spawn(float x, float y) {
    x_ = x;
    y_ = y;
    alive_ = true;
    falling_ = false;
    movingRight_ = false;
    moveCounter_ = 0;
    speed_ = 0;            // InitBloober: BlooperMoveSpeed = 0
    intervalTimer_ = 0;
    intervalFrames_ = 0;
    frameCounter_ = 0;
    frameAccumulator_ = 0.0f;
    fallSpeed_ = 0.0f;
}

void Blooper::reset() {
    alive_ = false;
    falling_ = false;
}

void Blooper::update(float dt, float targetX, float targetFeetY, bool targetMovingRight, int slot,
                     int screenHeight) {
    if (!alive_) return;
    // La logica del original es por cuadro: corre los cuadros enteros que
    // pasaron (con el paso fijo del juego, uno por update).
    frameAccumulator_ += dt * 60.0f;
    while (frameAccumulator_ >= 1.0f && alive_) {
        frameAccumulator_ -= 1.0f;
        runFrame(targetX, targetFeetY, targetMovingRight, slot, screenHeight);
    }
}

void Blooper::runFrame(float targetX, float targetFeetY, bool targetMovingRight, int slot, int screenHeight) {
    ++frameCounter_;
    // EnemyIntervalTimer baja de a 1 cada 21 cuadros.
    if (++intervalFrames_ >= 21) {
        intervalFrames_ = 0;
        if (intervalTimer_ > 0) --intervalTimer_;
    }

    if (falling_) {
        // MoveEnemySlowVert: cae despacio, boca abajo.
        fallSpeed_ = std::min(fallSpeed_ + kSlowGravity, kMaxFallSpeed);
        y_ += fallSpeed_;
        if (y_ > static_cast<float>(screenHeight)) alive_ = false;
        return;
    }

    // Re-apuntar de vez en cuando (PseudoRandomBitReg & 63, o & 3 en el
    // modo dificil): los de lugar impar copian la direccion del jugador,
    // los de lugar par van hacia el.
    const int mask = gSecondaryHardMode ? 3 : 63;
    if ((std::rand() & mask) == 0) {
        if (slot & 1) {
            movingRight_ = targetMovingRight;
        } else {
            movingRight_ = targetX > x_;
        }
    }

    // ProcSwimmingB.
    if ((moveCounter_ & 2) == 0) {
        if ((moveCounter_ & 1) == 0) {
            // Envion: la velocidad sube hasta 2.
            if ((frameCounter_ & 7) == 0) {
                ++speed_;
                if (speed_ == 2) ++moveCounter_;
            }
        } else if ((frameCounter_ & 7) == 0) {
            // Frena hasta 0 y arranca la espera minima.
            --speed_;
            if (speed_ == 0) {
                ++moveCounter_;
                intervalTimer_ = 2;
                intervalFrames_ = 0;
            }
        }
    } else {
        // Se hunde. Pasada la espera, vuelve al envion si el jugador esta a
        // su altura o mas arriba: Player_Y <= Enemy_Y + 0x10 (en nuestras
        // coordenadas: los pies del jugador = Player_Y + 48, el tope del
        // calamar = Enemy_Y + 16).
        const float playerY = targetFeetY - 48.0f;
        const float blooperY = y_ - 16.0f;
        if (intervalTimer_ == 0 && playerY <= blooperY + 0x10) {
            moveCounter_ = 0;
        } else if ((frameCounter_ & 1) == 0) {
            y_ += 1.0f;
        }
    }

    // Sube a la velocidad del envion, sin pasar del tope de arriba.
    const float raised = y_ - static_cast<float>(speed_);
    if (raised >= kTopY) y_ = raised;

    // De costado, la misma velocidad.
    x_ += movingRight_ ? static_cast<float>(speed_) : -static_cast<float>(speed_);

    // Fuera de la pantalla por abajo: desaparece.
    if (y_ > static_cast<float>(screenHeight)) alive_ = false;
}

void Blooper::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_) return;
    // Tentaculos recogidos mientras se hunde, estirados en el envion.
    const int col = (!falling_ && (moveCounter_ & 2) != 0) ? 1 : 0;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    sprite.draw(renderer, worldRow, col, drawX, drawY, false, falling_);
}

bool Blooper::killFalling(bool flyRight) {
    (void)flyRight;
    if (!alive_ || falling_) return false;
    falling_ = true;
    speed_ = 0;
    fallSpeed_ = 0.0f;
    return true;
}
