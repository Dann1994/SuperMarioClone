#include "Flagpole.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int kBallFrame = 0;
constexpr int kPoleFrame = 1;
// Ancho del palo dentro de su tile de 16px (ver flagpole.png): 2px en el
// medio. El agarre usa un margen extra para que no haga falta tocar
// exactamente esos 2px -- igual que en el original, alcanza con llegar.
constexpr float kGrabHalfWidth = 2.0f;

}  // namespace

void Flagpole::setup(int col, int topRow, int bottomRow, int tileSize) {
    col_ = col;
    topRow_ = topRow;
    bottomRow_ = bottomRow;
    tileSize_ = tileSize;
    // Arranca colgando justo debajo de la bola.
    flagY_ = static_cast<float>((topRow_ + 1) * tileSize_);
    lowering_ = false;
    active_ = true;
}

bool Flagpole::touches(float x, float y, int width, int height) const {
    if (!active_) return false;
    const float poleLeft = poleCenterX() - kGrabHalfWidth;
    const float poleRight = poleCenterX() + kGrabHalfWidth;
    return x < poleRight && x + width > poleLeft && y < bottomY() && y + height > topY();
}

void Flagpole::update(float dt) {
    if (!active_ || !lowering_) return;
    flagY_ = std::min(flagY_ + kLowerSpeed * dt, flagBottomY());
}

void Flagpole::render(SDL_Renderer* renderer, const SpriteSheet& poleSprite, const SpriteSheet& flagSprite,
                      int worldRow, float cameraX) const {
    if (!active_) return;
    const int drawX = col_ * tileSize_ - static_cast<int>(std::lround(cameraX));
    poleSprite.draw(renderer, worldRow, kBallFrame, drawX, topRow_ * tileSize_, false);
    for (int row = topRow_ + 1; row <= bottomRow_; ++row) {
        poleSprite.draw(renderer, worldRow, kPoleFrame, drawX, row * tileSize_, false);
    }
    // La bandera cuelga a la izquierda: su borde derecho toca el palo (que
    // esta en el medio del tile del mastil).
    flagSprite.draw(renderer, worldRow, 0, drawX - tileSize_ / 2, static_cast<int>(std::lround(flagY_)), false);
}
