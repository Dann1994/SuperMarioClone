#include "Podoboo.h"

#include <cmath>

void Podoboo::spawn(float x, float baseY) {
    x_ = x;
    baseY_ = baseY;
    y_ = baseY;
    velY_ = 0.0f;
    alive_ = true;
    state_ = PodobooState::Submerged;
    timer_ = kSubmergedDuration;
}

void Podoboo::update(float dt) {
    if (!alive_) return;

    if (state_ == PodobooState::Submerged) {
        timer_ -= dt;
        if (timer_ <= 0.0f) {
            state_ = PodobooState::Jumping;
            velY_ = kJumpImpulse;
        }
        return;
    }

    // Jumping: parabola pura por gravedad, sin deriva horizontal (x_ fijo,
    // ver el comentario de la clase).
    velY_ += kGravity * dt;
    y_ += velY_ * dt;
    if (velY_ > 0.0f && y_ >= baseY_) {
        // Volvio a hundirse en la lava: arranca el proximo ciclo.
        y_ = baseY_;
        state_ = PodobooState::Submerged;
        timer_ = kSubmergedDuration;
    }
}

void Podoboo::render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX) const {
    if (!alive_ || state_ != PodobooState::Jumping) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    // Volteado verticalmente mientras cae (velY_ > 0, ya paso la cima):
    // distingue subida de bajada, un solo frame no tiene animacion propia.
    sprite.draw(renderer, 0, 0, drawX, drawY, false, velY_ > 0.0f);
}
