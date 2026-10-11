#include "MovingPlatform.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr int kSegmentWidth = 8;

}  // namespace

void MovingPlatform::spawn(float x, float y, int widthPx, PlatformMode mode, float range, float period, float speed) {
    x_ = x;
    y_ = y;
    baseX_ = x;
    baseY_ = y;
    widthPx_ = widthPx;
    mode_ = mode;
    range_ = range;
    period_ = period > 0.0f ? period : 4.0f;
    speed_ = speed;
    phase_ = 0.0f;
    fallVelocity_ = 0.0f;
    falling_ = false;
    dx_ = 0.0f;
    dy_ = 0.0f;
    active_ = true;
}

void MovingPlatform::onStoodOn() {
    if (mode_ == PlatformMode::Fall) falling_ = true;
}

void MovingPlatform::update(float dt, int levelHeight) {
    dx_ = 0.0f;
    dy_ = 0.0f;
    if (!active_) return;
    const float oldX = x_;
    const float oldY = y_;

    switch (mode_) {
        case PlatformMode::Vertical:
        case PlatformMode::Horizontal: {
            // Coseno: arranca quieta en la punta inicial, acelera hacia el
            // medio y frena llegando a la otra punta (como el original).
            phase_ = std::fmod(phase_ + dt / period_, 1.0f);
            const float offset = range_ * (1.0f - std::cos(phase_ * kTwoPi)) / 2.0f;
            if (mode_ == PlatformMode::Vertical) {
                y_ = baseY_ + offset;
            } else {
                x_ = baseX_ + offset;
            }
            break;
        }
        case PlatformMode::LiftUp:
            y_ -= speed_ * dt;
            if (y_ + kHeight < 0.0f) y_ = static_cast<float>(levelHeight);
            break;
        case PlatformMode::LiftDown:
            y_ += speed_ * dt;
            if (y_ > static_cast<float>(levelHeight)) y_ = -static_cast<float>(kHeight);
            break;
        case PlatformMode::Fall:
            if (!falling_) break;
            fallVelocity_ = std::min(fallVelocity_ + kFallGravity * dt, kMaxFallSpeed);
            y_ += fallVelocity_ * dt;
            if (y_ > static_cast<float>(levelHeight)) active_ = false;
            break;
    }

    dx_ = x_ - oldX;
    dy_ = y_ - oldY;
    // El ciclo de los elevadores no cuenta como movimiento (no arrastra a
    // nadie de un borde de la pantalla al otro).
    if (std::abs(dy_) > static_cast<float>(levelHeight) / 2.0f) dy_ = 0.0f;
}

void MovingPlatform::render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX) const {
    if (!active_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    for (int offset = 0; offset < widthPx_; offset += kSegmentWidth) {
        sprite.draw(renderer, 0, 0, drawX + offset, drawY, false);
    }
}
