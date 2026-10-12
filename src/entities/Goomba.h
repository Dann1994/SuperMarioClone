#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Goomba (seccion 5.2): camina en linea recta a velocidad constante, rebota
// al chocar una pared (invierte direccion) y cae por gravedad sin miedo a
// los bordes -- misma fisica que el Mushroom (Mushroom.h), que es el modelo
// mas parecido que ya existia en el proyecto.
//
// Al ser pisado se achata (spritesheet trae un frame "squished" por tipo de
// mundo) y queda inofensivo unos instantes antes de desaparecer, en vez de
// desaparecer de golpe -- asi el pisoton se lee visualmente, como en el
// original.
//
// Muerte distinta si lo mata un Mario/Luigi con estrella o una bola de fuego
// (Game::checkGoombaCollision / checkFireballGoombaCollision): en vez de
// achatarse se da vuelta (sprite dibujado boca abajo) y sale disparado en
// parabola -- empuje horizontal constante hacia el lado contrario de donde
// vino el golpe, mas un salto inicial que la gravedad va frenando y
// revirtiendo, como una "U" invertida -- sin colisionar con nada (ni
// paredes ni piso) hasta salir de la pantalla, momento en el que recien
// desaparece.
class Goomba {
public:
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte (a diferencia de squish()/
    // killFalling()): para cuando se cambia de escenario de prueba y este no
    // tiene que seguir vivo, no porque lo hayan matado.
    void reset() { alive_ = false; squished_ = false; falling_ = false; }
    // screenHeight: alto visible en pantalla (no el alto del nivel, que
    // puede ser mas grande) -- de ahi sale cuando se considera "fuera de
    // campo de vision" mientras cae volteado, ver killFalling().
    void update(float dt, const Tilemap& level, int screenHeight);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, mismo criterio que Mushroom/Star).
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    bool isSquished() const { return squished_; }
    bool isFalling() const { return falling_; }
    // Solo puede dañar al pisarlo o de costado (o morir) mientras esta vivo
    // y no esta ya en alguna de las dos animaciones de muerte.
    bool isDangerous() const { return alive_ && !squished_ && !falling_; }
    // Pisoton: lo achata y arranca la cuenta regresiva para que desaparezca.
    void squish();
    // Al cruzarse con otro enemigo (Game::checkGoombaKoopaBounce y similares
    // para los que se agreguen despues): mismo efecto que chocar una pared,
    // invierte direccion, mas un empuje pushX para separarlos y que no
    // quede rebotando en el mismo lugar frame a frame.
    void bounceOffEnemy(float pushX) {
        x_ += pushX;
        velX_ = -velX_;
    }
    // Estrella o fuego: se da vuelta y sale disparado en parabola hacia
    // flyRight (true = derecha, false = izquierda -- el lado contrario de
    // donde vino el golpe), sin colision, hasta salir de pantalla.
    bool killFalling(bool flyRight);

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    static constexpr float kSpeed = 30.0f;              // px/s horizontal, constante
    static constexpr float kGravity = 858.0f;  // px/s^2, original: 0x3d/256 px/cuadro^2
    static constexpr float kMaxFallSpeed = 180.0f;  // px/s, original: 3 px/cuadro
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kWalkFrameDuration = 0.25f;  // s por frame de caminata
    static constexpr float kSquishDuration = 0.5f;      // s achatado antes de desaparecer
    static constexpr float kFallKillHopImpulse = -180.0f;      // px/s, saltito al darse vuelta, antes de caer
    static constexpr float kFallKillHorizontalSpeed = 60.0f;   // px/s, empuje horizontal constante de la parabola

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool squished_ = false;
    float squishTimer_ = 0.0f;
    bool falling_ = false;
    float walkAnimTimer_ = 0.0f;
    int walkFrame_ = 0;
};
