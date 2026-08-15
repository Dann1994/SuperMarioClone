#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Cheep Cheep (seccion 5.2, noveno enemigo): el pez rojo (confirmado por el
// usuario -- lo que antes probamos como "variante Swimming con paleta por
// WorldType" resulto ser en realidad el Bill Bala, ver BulletBill.h, mala
// identificacion en la hoja de referencia). Una sola paleta ("Red Enemies -
// All Environments", como el Koopa rojo o el Spiny): SIEMPRE usa la fila 0
// de su sheet, sin importar worldRow -- por eso render() no lo recibe.
//
// Dos variantes bien distintas que comparten clase (CheepCheepVariant,
// mismo criterio que Koopa::setColor -- se fija una sola vez, no cambia en
// la vida de la instancia):
//   Swimming -> nada en el agua, sin gravedad ni colision con tiles (igual
//               que Blooper/Lakitu). Desaparece solo al salir del todo del
//               nivel por cualquiera de los dos bordes (no rebota, nada "de
//               un lado al otro y se va", como en el original) -- Game.cpp
//               la repone con una tecla para probarla de nuevo. Dentro de
//               Swimming hay ademas dos patrones bien distintos (ver
//               SwimPattern, mismo criterio de eje ortogonal que
//               KoopaColor/KoopaState en Koopa.h -- se fija una sola vez
//               con setSwimPattern(), solo tiene sentido si variant_ es
//               Swimming):
//     Straight    -> linea recta horizontal, sin vaiven vertical.
//     Oscillating -> mismo avance horizontal (misma velocidad que Straight,
//                    kStraightSwimSpeed == kOscillateDriftSpeed a pedido del
//                    usuario) pero con un vaiven vertical corto y lento
//                    encima (kOscillateAmplitude/kOscillateFrequency) --
//                    esto solo es lo que distingue a las dos.
//   Jumping  -> variante saltarina de tierra/castillo: NO camina ni tiene
//               piso propio (a diferencia de la primera version) -- aparece
//               ya en el aire desde abajo (el spawn point, tipicamente
//               fuera de pantalla por abajo, lo decide Game.cpp) con un
//               envion hacia arriba (kJumpImpulse) y una deriva horizontal
//               bien abierta (kJumpDriftSpeed, hacia donde mira al
//               spawnear), sube y cae solo por gravedad en una unica
//               parabola ancha (kJumpGravity, mas floja que kGravity) -- ni
//               rebota contra nada ni vuelve a saltar sola. Al volver a
//               cruzar hacia abajo la altura en la que aparecio (o salir de
//               pantalla), se desactiva sola (ver isAlive()). Game.cpp
//               maneja el pool, el timer de aparicion al azar y de que lado
//               salta (adelante o atras del personaje, moneda 50/50 -- ver
//               Game::spawnNextCheepCheepJump y kMaxCheepCheepJumps en
//               Game.h).
//
// Sin caparazon ni frame de achatado propio en el arte fuente en ninguna de
// las dos variantes: pisoton, estrella o fuego lo matan igual (misma
// parabola de "se da vuelta y sale despedido" que el resto del elenco sin
// arte de muerte propio, ver Goomba.h).
enum class CheepCheepVariant {
    Swimming,
    Jumping,
};

enum class SwimPattern {
    Straight,
    Oscillating,
};

class CheepCheep {
public:
    // Se fijan una sola vez (ver el comentario de la clase), antes del
    // primer spawn().
    void setVariant(CheepCheepVariant variant) { variant_ = variant; }
    CheepCheepVariant variant() const { return variant_; }
    void setSwimPattern(SwimPattern pattern) { swimPattern_ = pattern; }

    // x, y: punto de aparicion. Para Jumping es tambien el "piso" contra el
    // que se mide la caida (ver baseY_ en el comentario de la clase) -- normal
    // que quien la spawnee (Game.cpp) le pase una Y por debajo del borde
    // inferior de pantalla, no una posicion real del nivel.
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenWidth: para Swimming, saber cuando salio del todo por un borde
    // y desaparecer (ver el comentario de la clase). screenHeight: ver
    // Goomba::update, mismo criterio para "fuera de campo de vision" mientras
    // cae dado vuelta (las dos variantes) -- Jumping ya se desactiva sola al
    // volver a cruzar baseY_ (ver updateJumping), esto es solo una red de
    // seguridad extra igual que en el resto del elenco.
    void update(float dt, int screenWidth, int screenHeight);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de
    // dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    bool isDangerous() const { return alive_ && !falling_; }

    // Pisoton, estrella o fuego: cualquiera lo mata igual (ver el
    // comentario de la clase), se da vuelta y sale disparado en parabola
    // hacia flyRight.
    void killFalling(bool flyRight);

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    // Misma velocidad horizontal para las dos (a pedido del usuario, antes
    // Straight iba mas rapido) -- lo unico que las distingue ahora es el
    // vaiven vertical de Oscillating (ver el comentario de la clase).
    static constexpr float kStraightSwimSpeed = 10.0f;     // px/s horizontal, SwimPattern::Straight
    static constexpr float kOscillateDriftSpeed = 10.0f;   // px/s horizontal, SwimPattern::Oscillating
    static constexpr float kOscillateAmplitude = 5.0f;     // px, vaiven corto de Oscillating
    static constexpr float kOscillateFrequency = 1.2f;     // rad/s, vaiven lento de Oscillating

    // Parabola ancha y alta (a pedido del usuario, en base al comportamiento
    // real de World 2-3/7-3/8-4 de SMB1 -- salta bien alto): gravedad propia
    // mas floja que kGravity de abajo (esa es solo para la caida al morir,
    // compartida con Swimming, no se toca para no afectar esa animacion).
    static constexpr float kJumpDriftSpeed = 110.0f;     // px/s horizontal, Jumping -- deriva bien abierta
    static constexpr float kJumpImpulse = -460.0f;       // px/s, envion inicial de Jumping
    static constexpr float kJumpGravity = 700.0f;        // px/s^2, mas floja que kGravity -> parabola mas abierta
    static constexpr float kGravity = 1000.0f;           // caida post-muerte (killFalling), las dos variantes
    static constexpr float kMaxFallSpeed = 300.0f;

    static constexpr float kAnimFrameDuration = 0.15f;   // s por frame de nado/salto
    static constexpr float kFallKillHopImpulse = -120.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;

    void updateSwimming(float dt, int screenWidth);
    // Parabola pura por gravedad, sin tilemap (ver el comentario de la
    // clase): se desactiva sola al volver a cruzar baseY_ de bajada.
    void updateJumping(float dt);

    CheepCheepVariant variant_ = CheepCheepVariant::Swimming;
    SwimPattern swimPattern_ = SwimPattern::Straight;
    float x_ = 0.0f;
    float y_ = 0.0f;
    // Swimming: centro del vaiven de Oscillating (altura de spawn).
    // Jumping: altura de spawn -- al volver a cruzarla de bajada (velY_ > 0,
    // ver updateJumping) se desactiva, es lo que reemplaza al viejo piso de
    // tilemap.
    float baseY_ = 0.0f;
    float wavePhase_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    bool lastFacingRight_ = true;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
