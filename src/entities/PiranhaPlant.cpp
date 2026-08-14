#include "PiranhaPlant.h"

#include <algorithm>
#include <cmath>

void PiranhaPlant::spawn(float x, float pipeTopY) {
    x_ = x;
    pipeTopY_ = pipeTopY;
    y_ = pipeTopY_;  // escondida: el tope del sprite arranca a la altura del cano
    alive_ = true;
    state_ = PiranhaPlantState::Hidden;
    timer_ = kHiddenDuration;
    animTimer_ = 0.0f;
    animFrame_ = 0;
}

void PiranhaPlant::update(float dt, bool safeToRise) {
    if (!alive_) return;

    timer_ -= dt;

    switch (state_) {
        case PiranhaPlantState::Hidden:
            if (timer_ <= 0.0f) {
                if (safeToRise) {
                    state_ = PiranhaPlantState::Rising;
                    timer_ = kTransitionDuration;
                } else {
                    // Hay alguien cerca del cano: no sale, reintenta en un
                    // rato en vez de esperar todo kHiddenDuration de nuevo.
                    timer_ = kRetryDelay;
                }
            }
            break;
        case PiranhaPlantState::Rising: {
            const float t = std::clamp(1.0f - timer_ / kTransitionDuration, 0.0f, 1.0f);
            y_ = pipeTopY_ - t * static_cast<float>(kHeight);
            if (timer_ <= 0.0f) {
                y_ = pipeTopY_ - static_cast<float>(kHeight);
                state_ = PiranhaPlantState::Up;
                timer_ = kUpDuration;
            }
            break;
        }
        case PiranhaPlantState::Up:
            if (timer_ <= 0.0f) {
                state_ = PiranhaPlantState::Descending;
                timer_ = kTransitionDuration;
            }
            break;
        case PiranhaPlantState::Descending: {
            const float t = std::clamp(1.0f - timer_ / kTransitionDuration, 0.0f, 1.0f);
            y_ = pipeTopY_ - static_cast<float>(kHeight) + t * static_cast<float>(kHeight);
            if (timer_ <= 0.0f) {
                y_ = pipeTopY_;
                state_ = PiranhaPlantState::Hidden;
                timer_ = kHiddenDuration;
            }
            break;
        }
    }

    // Muerde (abre/cierra la boca) mientras se la ve, en cualquiera de los
    // tres estados no escondidos -- no solo en Up, para que no se vea
    // "congelada" mientras asoma/se esconde.
    if (state_ != PiranhaPlantState::Hidden) {
        animTimer_ += dt;
        if (animTimer_ >= kBiteFrameDuration) {
            animTimer_ -= kBiteFrameDuration;
            animFrame_ = (animFrame_ + 1) % 2;
        }
    }
}

void PiranhaPlant::render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX) const {
    if (!alive_ || state_ == PiranhaPlantState::Hidden) return;

    // Cuanto del sprite queda por encima del borde del cano: 0 recien
    // saliendo, kHeight bien asomada. Se dibuja solo esa franja de arriba
    // (drawSlice) para que el resto quede "adentro" del cano en vez de
    // superponerse como si el cano fuera transparente.
    const float visibleHeight = std::clamp(pipeTopY_ - y_, 0.0f, static_cast<float>(kHeight));
    if (visibleHeight <= 0.0f) return;

    const int drawX = static_cast<int>(std::lround(x_ - cameraX));
    const int drawY = static_cast<int>(std::lround(y_));
    sprite.drawSlice(renderer, worldRow, animFrame_, 0, static_cast<int>(std::lround(visibleHeight)), drawX, drawY,
                      false);
}
