#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Bill Bala (enemigo nuevo, no estaba en la lista original de la seccion
// 5.2 -- surgio de una confusion: el sprite que se habia identificado como
// "Cheep Cheep version nado" en realidad era este, ver CheepCheep.h). Sin
// comportamiento extra a proposito (a diferencia de todo el resto del
// elenco): linea recta horizontal a velocidad constante, sin gravedad ni
// colision con tiles (no rebota contra pared, no se hunde/flota) -- se
// dispara y se olvida, como en el original (sale de un Bill Blaster que
// todavia no existe como objeto, por eso el spawn es manual en el
// escenario de prueba). Desaparece solo al salir del todo del nivel por
// cualquiera de los dos bordes.
//
// A diferencia de Blooper/Spiny (sin pisoton) o de Piraña Plant, el Bill
// Bala SI se puede pisar como el resto del elenco clasico (Goomba-like):
// pisoton lo mata (se achata/desaparece via killFalling, no tiene frame de
// achatado propio), de costado hace dano. Estrella o fuego lo matan igual
// que un pisoton.
//
// Tiene una fila por WorldType en la hoja de referencia (misma hoja de
// donde salio la confusion con Cheep Cheep -- ver el comentario de esa
// clase): render() recibe worldRow.
class BulletBill {
public:
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenWidth: para saber cuando salio del todo por un borde y
    // desaparecer. screenHeight: ver Goomba::update, mismo criterio para
    // "fuera de campo de vision" mientras cae dado vuelta.
    void update(float dt, int screenWidth, int screenHeight);
    // worldRow selecciona la variante de color segun el tipo de mundo (ver
    // el comentario de la clase). cameraX: ver Game::cameraX_, se resta a
    // la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    bool isDangerous() const { return alive_ && !falling_; }

    // Pisoton, estrella o fuego: cualquiera lo mata igual (ver el
    // comentario de la clase), se da vuelta y sale disparado en parabola
    // hacia flyRight.
    bool killFalling(bool flyRight);

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    static constexpr float kSpeed = 60.0f;  // px/s, linea recta constante
    static constexpr float kGravity = 1000.0f;
    static constexpr float kMaxFallSpeed = 300.0f;
    static constexpr float kFallKillHopImpulse = -180.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
};
