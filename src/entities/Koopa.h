#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Koopa Troopa verde (seccion 5.2): a diferencia del rojo (no programado
// todavia), no le tiene miedo a los bordes -- misma fisica de caminata que
// el Goomba (Goomba.h).
//
// Maquina de estados (toda la logica de que gatilla cada transicion vive en
// Game.cpp, junto con la de Goomba):
//   Walking    -> pisada normal: se mete en el caparazon (ShellIdle).
//   ShellIdle  -> quieto en el piso. Pisarlo de nuevo lo patea igual que
//                 tocarlo de costado (ShellMoving) -- de costado, hacia el
//                 lado contrario a quien lo toco; de arriba, hacia donde
//                 esta mirando el personaje (un pisoton no tiene un "lado"
//                 natural). Si nadie lo toca despues de kShellIdleDuration,
//                 sale solo y retoma la caminata en la direccion en la que
//                 iba antes de esconderse.
//   ShellMoving-> desliza rapido, rebota contra paredes, cae por gravedad.
//                 Mata a cualquier otro enemigo que toque (Game::checkShellVsGoomba/
//                 checkShellVsKoopa) y sigue de largo -- salvo que choque
//                 con OTRO caparazon en movimiento, ahi mueren los dos.
//                 Pisarlo lo frena (vuelve a ShellIdle, temporizador de
//                 nuevo desde cero); tocarlo de costado hace daño (como
//                 cualquier enemigo), salvo estrella.
// En cualquier estado no-caido, estrella o fuego lo eliminan igual que al
// Goomba: se esconde en el caparazon (frame shell1, quieto -- no el flip
// vertical que usa el Goomba por no tener arte propio) y sale despedido en
// parabola hacia el lado contrario de donde vino el golpe.
enum class KoopaState {
    Walking,
    ShellIdle,
    ShellMoving,
};

class Koopa {
public:
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, const Tilemap& level, int screenHeight);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, mismo criterio que Goomba/Mushroom).
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    KoopaState state() const { return state_; }
    bool isMovingShell() const { return alive_ && !falling_ && state_ == KoopaState::ShellMoving; }
    // Puede interactuar (pisarse/patearse/danar/matar) mientras este vivo y
    // no este ya cayendo muerto -- caminando, escondido o deslizandose son
    // todos "dangerous" (cada uno reacciona distinto, ver Game.cpp). Ojo:
    // esto NO incluye el cooldown post-pisoton (ver isImmuneTo) porque ese
    // es especifico de UN personaje, no general -- el caparazon tiene que
    // poder seguir matando a otros enemigos (o danar al OTRO jugador, en 2P)
    // aunque este en cooldown contra quien lo piso/pateo.
    bool isDangerous() const { return alive_ && !falling_; }
    // El personaje que lo piso/pateo (identificado por puntero, no le
    // importa el tipo) no vuelve a interactuar con este Koopa durante
    // kPostStompCooldown -- sin este respiro, si el personaje no rebota
    // (pisar un caparazon quieto) o rebota parado sin moverse, el contacto
    // que sigue lo terminaria danando o re-pateando sin parar. El resto (el
    // otro jugador, otros enemigos) no se ve afectado.
    bool isImmuneTo(const void* character) const {
        return postStompCooldown_ > 0.0f && character == stompCooldownOwner_;
    }

    // Pisoton: Walking -> se esconde (ShellIdle). ShellMoving -> se frena
    // (ShellIdle, timer de nuevo desde cero) y character queda inmune un
    // rato (ver isImmuneTo) -- sin eso, si el personaje vuelve a caer en el
    // mismo lugar (rebota parado sin moverse) lo pisa nuevamente ya quieto,
    // y pisar-quieto lo patea -- se veia como si "nunca se frenara".
    // ShellIdle -> no hace nada (el llamador igual hace rebotar al personaje).
    void stomp(const void* character = nullptr);
    // Patada: solo tiene efecto en ShellIdle -> ShellMoving hacia kickRight.
    // Mismo respiro que stomp() (ver isImmuneTo): si la patada vino de un
    // pisoton (sin rebote, el personaje queda encima), hace falta igual
    // para que el contacto que sigue no lo dane.
    void kick(bool kickRight, const void* character = nullptr);
    // Estrella o fuego: se da vuelta y sale disparado en parabola hacia
    // flyRight, sin colision, hasta salir de pantalla (igual que Goomba).
    void killFalling(bool flyRight);
    // Al cruzarse con otro enemigo (Game::checkGoombaKoopaBounce/checkKoopaKoopaBounce):
    // mismo efecto que chocar una pared -- invierte direccion (si estaba
    // quieto, como ShellIdle, sigue quieto) mas un empuje pushX para
    // separarlos y que no quede rebotando en el mismo lugar frame a frame.
    void bounceOffEnemy(float pushX) {
        x_ += pushX;
        velX_ = -velX_;
        if (velX_ != 0.0f) lastFacingRight_ = velX_ > 0.0f;
    }

    float x() const { return x_; }
    float y() const { return y_; }
    float velocityX() const { return velX_; }
    int width() const { return kHitboxWidth; }
    int height() const { return state_ == KoopaState::Walking ? kWalkHeight : kShellHeight; }

    static constexpr int kHitboxWidth = 16;
    static constexpr int kWalkHeight = 24;
    static constexpr int kShellHeight = 16;
    // Tamano del frame en el spritesheet (igual para las 6 columnas: walk1,
    // walk2, dead1, dead2, shell1, shell2 -- ver tools/asset_scripts, koopa.png).
    static constexpr int kFrameWidth = 16;
    static constexpr int kFrameHeight = 24;

private:
    void applyVerticalPhysics(float dt, const Tilemap& level, int hitboxHeight);
    void applyHorizontalWithWallBounce(float dt, const Tilemap& level, int hitboxHeight);

    static constexpr float kWalkSpeed = 30.0f;                 // px/s, igual que el Goomba
    static constexpr float kShellSpeed = 210.0f;                // px/s, caparazon deslizando
    static constexpr float kGravity = 1000.0f;
    static constexpr float kMaxFallSpeed = 300.0f;
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kWalkFrameDuration = 0.25f;          // s por frame de caminata
    static constexpr float kShellIdleDuration = 8.0f;           // s escondido antes de salir solo
    // Ultimo tramo escondido: parpadea entre shell1/shell2 como aviso de que
    // esta por salir (shell2 NO es una animacion de bamboleo continua, es
    // especificamente este aviso).
    static constexpr float kShellWarningDuration = 1.5f;
    static constexpr float kShellWarningFlashDuration = 0.15f;  // s por frame durante el aviso
    static constexpr float kFallKillHopImpulse = -120.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;
    // Inmunidad de un personaje puntual al recien frenar/patear un
    // caparazon, ver isImmuneTo().
    static constexpr float kPostStompCooldown = 0.4f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    KoopaState state_ = KoopaState::Walking;
    bool lastFacingRight_ = true;
    float shellIdleTimer_ = 0.0f;
    float postStompCooldown_ = 0.0f;
    const void* stompCooldownOwner_ = nullptr;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
