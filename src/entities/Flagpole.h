#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Bandera de meta (seccion 5.3): el mastil sale de los tiles 'F' del nivel
// (ver Tilemap::findFlagpole) y esta clase lo dibuja junto con la bandera,
// que arranca arriba (justo debajo de la bola del tope) y baja deslizandose
// hasta el pie del mastil cuando alguien lo agarra (startLowering). La
// secuencia del personaje (bajar por el mastil, pasarse al otro lado,
// caminar hasta el castillo) es de Mario, no de esta clase -- Game.cpp las
// coordina (ver Game::checkFlagpoleGrab/updateFlagpoleSequence).
//
// Sprites (tools/asset_scripts/gen_flagpole.py), una fila por WorldType:
// flagpole.png con 2 columnas de 16x16 (0 = bola, 1 = tramo de mastil) y
// flag.png con la bandera (16x16, cuelga a la izquierda del mastil).
class Flagpole {
public:
    void setup(int col, int topRow, int bottomRow, int tileSize);
    void reset() { active_ = false; }
    bool isActive() const { return active_; }

    // Centro en X del mastil (el palo en si mide 2px, en el medio del tile).
    float poleCenterX() const { return static_cast<float>(col_ * tileSize_ + tileSize_ / 2); }
    // Tope de la bola y pie del mastil (= tope del bloque base de abajo).
    float topY() const { return static_cast<float>(topRow_ * tileSize_); }
    float bottomY() const { return static_cast<float>((bottomRow_ + 1) * tileSize_); }

    // true si el hitbox toca el palo (de la bola para abajo).
    bool touches(float x, float y, int width, int height) const;

    void startLowering() { lowering_ = true; }
    bool isLowering() const { return lowering_; }
    bool isFlagDown() const { return flagY_ >= flagBottomY(); }
    void update(float dt);
    void render(SDL_Renderer* renderer, const SpriteSheet& poleSprite, const SpriteSheet& flagSprite, int worldRow,
                float cameraX = 0.0f) const;

    // Velocidad con la que baja la bandera, px/s -- la misma con la que baja
    // el personaje por el mastil (Mario::kFlagpoleSlideSpeed), como en el
    // original, asi llegan abajo mas o menos juntos.
    static constexpr float kLowerSpeed = 120.0f;

private:
    float flagBottomY() const { return bottomY() - static_cast<float>(tileSize_); }

    int col_ = 0;
    int topRow_ = 0;
    int bottomRow_ = 0;
    int tileSize_ = 16;
    float flagY_ = 0.0f;
    bool lowering_ = false;
    bool active_ = false;
};
