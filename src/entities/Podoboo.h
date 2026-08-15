#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Podoboo (seccion 5.2, decimo enemigo): la bola de fuego que vive en un
// pozo de lava, solo en niveles de castillo (comportamiento real de SMB1,
// confirmado por el usuario contra la hoja de referencia -- fila 3 columna 6
// de "Red Enemies - All Environments" en Enemigos.png, UN SOLO frame, sin
// animacion propia).
//
// A diferencia de TODO el resto del elenco: es completamente invencible.
// Ni pisoton, ni estrella, ni fuego lo afectan -- por eso esta clase no
// tiene kill()/killFalling() ni ningun metodo de muerte, y Game.cpp no
// necesita ninguno tampoco (cualquier contacto mientras esta afuera de la
// lava hace dano sin excepcion, ver Game::checkPodobooCollision).
//
// No camina ni se mueve en X (x_ fijo, el pozo de lava no se desplaza):
// cicla sola entre escondida bajo la superficie (Submerged, invisible, fuera
// de alcance) y un unico salto vertical por gravedad (Jumping, visible,
// peligrosa) -- timer fijo, no al azar (el original tiene un patron
// predecible, no aleatorio como el Cheep Cheep Jumping).
enum class PodobooState {
    Submerged,
    Jumping,
};

class Podoboo {
public:
    // x: columna fija del pozo de lava. baseY: altura de la superficie de la
    // lava (a la que vuelve a "hundirse" al terminar cada salto).
    void spawn(float x, float baseY);
    // Lo desactiva del todo (sin animacion, no hace falta -- ver el
    // comentario de la clase): para cuando se cambia de escenario de prueba.
    void reset() { alive_ = false; }
    void update(float dt);
    // Se dibuja volteado verticalmente mientras cae (ya paso la cima de la
    // parabola) para distinguir subida de bajada con el unico frame que
    // tiene.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    PodobooState state() const { return state_; }
    // Solo se puede tocar mientras esta afuera de la lava (Jumping) --
    // Submerged esta debajo, fuera de alcance (mismo criterio que
    // PiranhaPlant::isDangerous con Hidden).
    bool isDangerous() const { return alive_ && state_ == PodobooState::Jumping; }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    static constexpr float kSubmergedDuration = 1.2f;  // s escondido antes de saltar
    static constexpr float kJumpImpulse = -300.0f;      // px/s, envion inicial
    static constexpr float kGravity = 700.0f;           // px/s^2

    float x_ = 0.0f;
    float y_ = 0.0f;
    float baseY_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    PodobooState state_ = PodobooState::Submerged;
    float timer_ = 0.0f;
};
