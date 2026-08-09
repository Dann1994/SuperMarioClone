#include "ScorePopup.h"

#include "../render/DebugFont.h"

void ScorePopup::spawn(float x, float y, const std::string& text) {
    x_ = x;
    startY_ = y;
    timer_ = kDuration;
    text_ = text;
    alive_ = true;
}

void ScorePopup::update(float dt) {
    if (!alive_) return;
    timer_ -= dt;
    if (timer_ <= 0.0f) {
        alive_ = false;
    }
}

void ScorePopup::render(SDL_Renderer* renderer) const {
    if (!alive_) return;
    const float progress = 1.0f - (timer_ / kDuration);
    const int y = static_cast<int>(startY_ - kRiseDistance * progress);
    DebugFont::drawText(renderer, static_cast<int>(x_), y, text_, 1, SDL_Color{255, 255, 255, 255});
}
