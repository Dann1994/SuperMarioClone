#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Pirana Plant (seccion 5.2): vive dentro de una tuberia (el objeto
// "tuberia" en si todavia no existe, seccion 5.3 -- el nivel de prueba solo
// pone un tile solido angosto con esa forma para que se vea de donde sale,
// ver test_enemies.json). No camina, no tiene caparazon: cicla sola entre
// escondida y asomada, y cualquier contacto mientras esta asomada hace dano
// -- a diferencia del resto del elenco, un pisoton NO la mata (no hay forma
// de pisarla desde arriba estando metida en el caño). Solo la matan la
// estrella o el fuego, igual que a cualquier otro enemigo, pero sin
// parabola de caida (no tiene adonde volar): desaparece de una.
//
// No se asoma si hay un personaje demasiado cerca del caño (mismo criterio
// que el original: le da tiempo a alejarse antes de salir, para no golpear
// "gratis") -- pero esta clase no conoce a Mario/Luigi, asi que quien
// decide eso es Game.cpp (ve las posiciones de los personajes) y se lo pasa
// a update() ya resuelto como el parametro safeToRise.
//
// Render: mientras esta escondida o a medio asomar, solo se dibuja la
// porcion de arriba de la tuberia hacia arriba (SpriteSheet::drawSlice) --
// asi se ve que "sale" del caño en vez de superponerse encima como si el
// caño fuera transparente.
enum class PiranhaPlantState {
    Hidden,
    Rising,
    Up,
    Descending,
};

class PiranhaPlant {
public:
    // x: centro horizontal del cano. pipeTopY: altura del borde de arriba
    // del cano (donde queda la base de la planta al estar bien asomada) --
    // no la posicion de la planta en si, esa se calcula sola.
    void spawn(float x, float pipeTopY);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset() { alive_ = false; }
    void update(float dt, bool safeToRise);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, mismo criterio que el resto del
    // elenco -- Underground y Castillo comparten el mismo arte fuente, ver
    // tools/asset_scripts).
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const;

    bool isAlive() const { return alive_; }
    PiranhaPlantState state() const { return state_; }
    // Solo se puede tocar mientras se la ve asomando (Rising, Up o
    // Descending) -- escondida esta debajo del cano, fuera de alcance.
    bool isDangerous() const { return alive_ && state_ != PiranhaPlantState::Hidden; }

    // Estrella o fuego: desaparece de una (ver el comentario de la clase).
    void kill() { alive_ = false; }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 24;

private:
    static constexpr float kHiddenDuration = 2.5f;   // s escondida antes de intentar asomar
    static constexpr float kUpDuration = 2.0f;       // s bien asomada
    static constexpr float kTransitionDuration = 0.5f;  // s asomando/escondiendose
    static constexpr float kRetryDelay = 0.4f;       // s antes de reintentar si no era seguro asomar
    static constexpr float kBiteFrameDuration = 0.2f;   // s por frame de "muerde" (boca abierta/cerrada)

    float x_ = 0.0f;
    float y_ = 0.0f;
    float pipeTopY_ = 0.0f;
    bool alive_ = false;
    PiranhaPlantState state_ = PiranhaPlantState::Hidden;
    float timer_ = 0.0f;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
