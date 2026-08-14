#pragma once

#include <SDL.h>
#include <string>

// Popup de puntaje al agarrar un power-up (seccion 6: "score values"):
// sube flotando un poco y desaparece. Usa DebugFont en vez de un sprite
// recortado de la hoja de referencia (el numero ahi es demasiado chico
// para recortar limpio a mano; esto da el mismo resultado visual).
class ScorePopup {
public:
    void spawn(float x, float y, const std::string& text);
    void update(float dt);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, float cameraX = 0.0f) const;
    bool isAlive() const { return alive_; }

private:
    static constexpr float kDuration = 0.7f;      // s que dura en pantalla
    static constexpr float kRiseDistance = 14.0f; // px que sube en total

    float x_ = 0.0f;
    float startY_ = 0.0f;
    float timer_ = 0.0f;
    bool alive_ = false;
    std::string text_;
};
