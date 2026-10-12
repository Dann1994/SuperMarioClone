#include "BowserFire.h"

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
}  // namespace

void BowserFire::spawn(float x, float y, float velX, float maxY) {
    x_ = x;
    y_ = y;
    maxY_ = maxY;
    velX_ = velX;
    // Al azar, una sola vez por lanzamiento (ver el comentario de la
    // clase): kDownChance de bajar (mas seguido que las otras dos, a pedido
    // del usuario), el resto repartido por igual entre subir y derecho --
    // fijo para todo el vuelo, no cambia a mitad de camino. La velocidad se
    // calcula para llegar justo a kVerticalShiftPixels de desvio al cabo de
    // kVerticalShiftDuration (ver update()): de ahi en mas sigue derecho.
    static const float kTargetOffsets[4] = {32.0f, 48.0f, 64.0f, 32.0f};
    targetY_ = maxY - kTargetOffsets[static_cast<int>(randomRange(0.0f, 3.999f))];
    velY_ = 0.0f;
    verticalShiftTimer_ = 0.0f;
    alive_ = true;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void BowserFire::update(float dt, int screenWidth, int screenHeight) {
    if (!alive_) return;

    x_ += velX_ * dt;
    // Se acomoda a su altura objetivo y despues sigue derecho.
    if (y_ < targetY_) {
        y_ = std::min(targetY_, y_ + kVerticalSpeed * dt);
    } else if (y_ > targetY_) {
        y_ = std::max(targetY_, y_ - kVerticalSpeed * dt);
    }

    animTimer_ += dt;
    if (animTimer_ >= kAnimFrameDuration) {
        animTimer_ -= kAnimFrameDuration;
        animFrame_ = (animFrame_ + 1) % 2;
    }

    if (x_ + kWidth < 0.0f || x_ > static_cast<float>(screenWidth) || y_ + kHeight < 0.0f ||
        y_ > static_cast<float>(screenHeight)) {
        alive_ = false;
    }
}

void BowserFire::render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX) const {
    if (!alive_) return;
    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    sprite.draw(renderer, 0, animFrame_, drawX, drawY, velX_ >= 0.0f);
}
