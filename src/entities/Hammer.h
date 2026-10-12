#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// El martillo que tira el Hammer Bro (seccion 5.2): sale despedido en un
// arco y gira todo el tiempo mientras vuela (ver Documentaciones/Hammer.png
// -- "los martillos tirados por los Bros. y Bowser rotan"). No choca contra
// nada del escenario (atraviesa piso, paredes y plataformas por igual, como
// si no estuvieran) ni desaparece al tocar a Mario/Luigi (les hace dano,
// pero sigue de largo). Solo se lo saca de en medio si sale de la pantalla
// (ver update()) o si cambia el escenario de prueba (reset()). Entidad
// separada de HammerBro (que es quien decide cuando y hacia donde tirarlo,
// ver HammerBro::consumeThrowRequest) para poder animar el giro de forma
// independiente del estado del que lo tiro.
class Hammer {
public:
    void spawn(float x, float y, float velX, float velY);
    void reset() { alive_ = false; }
    // screenWidth/screenHeight: ver Fireball::update, mismo criterio para
    // "fuera de campo de vision" (sale de la pantalla o cae por un pozo).
    void update(float dt, const Tilemap& level, int screenWidth, int screenHeight);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, mismo criterio que el resto del
    // elenco).
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    static constexpr float kGravity = 225.0f;  // px/s^2: 0x10/256 px/cuadro^2, como el original
    static constexpr float kMaxFallSpeed = 240.0f;  // px/s: 4 px/cuadro
    static constexpr float kSpinFrameDuration = 0.06f;  // s por frame de giro

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
