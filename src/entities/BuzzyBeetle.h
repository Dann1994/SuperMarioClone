#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Buzzy Beetle (seccion 5.2): muy parecido al Koopa Troopa verde caminando
// (misma fisica de pared/gravedad, se cae de los bordes, mismo mecanismo de
// caparazon con pisoton/patada/despertar solo, ver Koopa.h) pero con tres
// diferencias puntuales del original de 1985:
//   - No existe variante roja (siempre se cae de los bordes) ni Koopa Alado
//     equivalente (no vuela ni salta, solo camina) -- por eso es una clase
//     aparte en vez de otra combinacion de KoopaColor/KoopaState.
//   - Inmune al fuego: una bola de fuego explota igual al tocarlo (mismo
//     feedback visual que contra cualquier otro enemigo, ver
//     Game::checkFireballBuzzyBeetleCollision) pero no le hace nada -- sigue
//     caminando o en el estado en que estaba. Estrella si lo mata, igual
//     que a cualquier otro enemigo.
//   - El sprite (Documentaciones/Enemigos.png) ya es del tamano del
//     caparazon en las tres poses (camina "metido" en su caparazon todo el
//     tiempo, a diferencia del Koopa que muestra piernas largas al
//     caminar), asi que el hitbox es 16x16 fijo en cualquier estado, sin el
//     ajuste de "y" que hace falta en Koopa al esconderse.
//
// Maquina de estados (la logica de que gatilla cada transicion vive en
// Game.cpp, junto con la de Koopa):
//   Walking    -> pisoton lo esconde (ShellIdle); de costado hace dano.
//   ShellIdle  -> quieto en el piso. Pisarlo de nuevo lo patea igual que
//                 tocarlo de costado (ShellMoving) -- de costado, hacia el
//                 lado contrario a quien lo toco; de arriba, hacia donde
//                 esta mirando el personaje. Si nadie lo toca despues de
//                 kShellIdleDuration, sale solo y retoma la caminata en la
//                 direccion en la que iba antes de esconderse. Solo hay un
//                 frame de caparazon en la hoja de referencia, asi que a
//                 diferencia del Koopa no hay parpadeo de aviso antes de
//                 salir (se queda quieto todo el tiempo escondido).
//   ShellMoving-> desliza rapido, rebota contra paredes, cae por gravedad.
//                 Mata a cualquier otro enemigo que toque (Game::checkShellVsGoomba/
//                 checkShellVsKoopa/checkShellVsBuzzyBeetle) y sigue de
//                 largo. Pisarlo lo frena (vuelve a ShellIdle, timer de
//                 nuevo desde cero); tocarlo de costado hace dano.
// En cualquier estado no-caido, la estrella lo elimina igual que a
// cualquier otro enemigo: se da vuelta (frame de caparazon, flip vertical
// -- no tiene arte propio de "muerto") y sale despedido en parabola hacia
// el lado contrario de donde vino el golpe.
enum class BuzzyBeetleState {
    Walking,
    ShellIdle,
    ShellMoving,
};

class BuzzyBeetle {
public:
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, const Tilemap& level, int screenHeight);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, mismo criterio que Goomba/Koopa).
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    BuzzyBeetleState state() const { return state_; }
    bool isMovingShell() const { return alive_ && !falling_ && state_ == BuzzyBeetleState::ShellMoving; }
    bool isDangerous() const { return alive_ && !falling_; }
    // El personaje que lo piso/pateo no vuelve a interactuar con este
    // Buzzy Beetle durante kPostStompCooldown -- mismo motivo que
    // Koopa::isImmuneTo, ver ese comentario.
    bool isImmuneTo(const void* character) const {
        return postStompCooldown_ > 0.0f && character == stompCooldownOwner_;
    }

    // Pisoton: Walking -> se esconde (ShellIdle). ShellMoving -> se frena
    // (ShellIdle, timer de nuevo desde cero) y character queda inmune un
    // rato (ver isImmuneTo). ShellIdle -> no hace nada (el llamador igual
    // hace rebotar al personaje).
    void stomp(const void* character = nullptr);
    // Patada: solo tiene efecto en ShellIdle -> ShellMoving hacia kickRight.
    void kick(bool kickRight, const void* character = nullptr);
    // Estrella: se da vuelta y sale disparado en parabola hacia flyRight,
    // sin colision, hasta salir de pantalla (igual que Goomba/Koopa). El
    // fuego NO llega a llamar esto (ver el comentario de la clase).
    void killFalling(bool flyRight);
    // Al cruzarse con otro enemigo (Game::checkGoombaBuzzyBeetleBounce/
    // checkKoopaBuzzyBeetleBounce): mismo efecto que chocar una pared.
    void bounceOffEnemy(float pushX) {
        x_ += pushX;
        velX_ = -velX_;
        if (velX_ != 0.0f) lastFacingRight_ = velX_ > 0.0f;
    }

    float x() const { return x_; }
    float y() const { return y_; }
    float velocityX() const { return velX_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    static constexpr float kWalkSpeed = 30.0f;
    static constexpr float kShellSpeed = 210.0f;
    static constexpr float kGravity = 1000.0f;
    static constexpr float kMaxFallSpeed = 300.0f;
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kWalkFrameDuration = 0.25f;
    static constexpr float kShellIdleDuration = 8.0f;
    static constexpr float kFallKillHopImpulse = -120.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;
    static constexpr float kPostStompCooldown = 0.4f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    BuzzyBeetleState state_ = BuzzyBeetleState::Walking;
    bool lastFacingRight_ = true;
    float shellIdleTimer_ = 0.0f;
    float postStompCooldown_ = 0.0f;
    const void* stompCooldownOwner_ = nullptr;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
