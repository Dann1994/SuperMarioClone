#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// La moneda que salta de un bloque de interrogacion (seccion 5.3/6), NO la
// misma entidad que Coin.h (esa es la moneda quieta que se deja quieta en
// el piso del nivel, se agarra caminando -- ver el comentario de
// Game::spawnItemFromQuestionBlock). Esta aparece pegada al bloque, salta
// derecho hacia arriba (sin arco, solo sube y frena por gravedad) girando
// sus 4 frames (Documentaciones/Objetos.png, fila de "moneda de bloque" --
// una sola paleta, no cambia de color por WorldType como Coin.h) y
// desaparece sola apenas llega arriba, sin caer de vuelta -- calcado del
// original. Se suma al contador de monedas en el momento en que Game.cpp la
// hace aparecer (golpear el bloque), no cuando termina de saltar: esta
// clase es puramente visual, no hace falta consultarla para saber si "ya
// sumo".
class BlockCoin {
public:
    void spawn(float x, float y);
    void reset() { alive_ = false; }
    void update(float dt);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }

    float x() const { return x_; }
    float y() const { return y_; }
    static constexpr int kWidth = 8;
    static constexpr int kHeight = 16;

private:
    static constexpr float kJumpImpulse = -260.0f;  // px/s
    static constexpr float kGravity = 1100.0f;       // px/s^2
    static constexpr float kFrameDuration = 0.05f;   // s por frame del giro, rapido

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    float animTimer_ = 0.0f;
    int frame_ = 0;
};
