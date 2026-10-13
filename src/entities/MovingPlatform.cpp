#include "MovingPlatform.h"

#include <algorithm>
#include <cmath>

namespace {

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
    pendingDy_ = 0.0f;
    dx_ = 0.0f;
    dy_ = 0.0f;
    active_ = true;
    started_ = false;
    frameAccum_ = 0.0f;
    frames_ = 0;
    velY_ = 0.0f;
    xPrimary_ = 0;
    xSecondary_ = 0;
    // InitVertPlatform: Y del original = la nuestra - 16; en la mitad de
    // abajo (Y >= 0x80) el centro queda arriba.
    centerY_ = (y - 16.0f) < 128.0f ? y + 64.0f : y - 64.0f;
}

void MovingPlatform::onStoodOn() {
    if (mode_ == PlatformMode::Fall) falling_ = true;
}

void MovingPlatform::update(float dt, int levelHeight) {
    dx_ = 0.0f;
    dy_ = 0.0f;
    if (!active_ || !started_) return;
    const float oldX = x_;
    const float oldY = y_;

    switch (mode_) {
        case PlatformMode::Vertical:
        case PlatformMode::Horizontal: {
            // Por cuadro, como el original (60 por segundo).
            frameAccum_ += dt * 60.0f;
            while (frameAccum_ >= 1.0f) {
                frameAccum_ -= 1.0f;
                ++frames_;
                if (mode_ == PlatformMode::Vertical) {
                    // MovePlatformDown/Up: ImposeGravity con 5/256, tope 3.
                    y_ += velY_;
                    velY_ += (y_ >= centerY_ ? -5.0f : 5.0f) / 256.0f;
                    velY_ = std::clamp(velY_, -3.0f, 3.0f);
                } else {
                    // XMoveCntr_Platform(0x0e) + MoveWithXMCntrs.
                    if ((frames_ & 3) == 0) {
                        if (xPrimary_ & 1) {
                            if (xSecondary_ != 0) --xSecondary_; else ++xPrimary_;
                        } else {
                            if (xSecondary_ != 0x0e) ++xSecondary_; else ++xPrimary_;
                        }
                    }
                    const bool right = (xPrimary_ & 2) != 0;
                    x_ += (right ? 1.0f : -1.0f) * static_cast<float>(xSecondary_) / 16.0f;
                }
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
        case PlatformMode::Balance:
            y_ += pendingDy_;
            pendingDy_ = 0.0f;
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
