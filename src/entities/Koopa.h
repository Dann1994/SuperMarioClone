#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Koopa Troopa, verde o rojo (seccion 5.2, ver KoopaColor): misma fisica de
// caminata que el Goomba (Goomba.h), salvo que el rojo le tiene miedo a los
// bordes -- da la vuelta antes de caerse en vez de seguir de largo (en
// Walking y Jumping; una vez que es caparazon en movimiento, cualquiera de
// los dos colores se cae si lo patean hacia un borde, el caparazon ya no
// "piensa"). El rojo ademas usa un sprite propio con una sola paleta ("All
// Environments" en la hoja de referencia, no una fila por WorldType como el
// verde), ver setColor()/render().
//
// Maquina de estados (toda la logica de que gatilla cada transicion vive en
// Game.cpp, junto con la de Goomba). KoopaState y KoopaColor son ejes
// independientes en esta clase (nada le impide combinarlos distinto), pero
// en el juego original de 1985 el color fija cual de los dos "Koopa Alado"
// le toca -- el VERDE va a los saltos (Jumping) y el ROJO se hamaca arriba y
// abajo en un punto fijo (Flying); no existe un verde volador ni un rojo
// saltarin, ver los comentarios de koopaJumping_/koopaRedFlying_ en Game.h:
//   Flying     -> variante alada (Koopa Alado rojo): igual que Walking en X
//                 (misma velocidad, rebota contra pared) pero flotando --
//                 sin gravedad ni piso, hamacandose en Y con una onda
//                 senoidal alrededor de la altura de spawn. Un pisoton le
//                 saca las alas: pasa a Walking (mismo alto de hitbox que
//                 Flying, no hace falta ajustar y_) y a partir de ahi cae
//                 solo por la gravedad normal de Walking, como si se
//                 desplomara al perder las alas. De costado hace daño,
//                 igual que Walking.
//   Jumping    -> variante saltarina (Koopa Alado verde): igual que Walking
//                 en X, pero en vez de quedarse parado al tocar el piso,
//                 cada aterrizaje le vuelve a dar impulso hacia arriba -- va
//                 "rebotando" en arcos en vez de caminar liso. Lleva las
//                 mismas alas que Flying (mismo frame, ver render()); un
//                 pisoton se las saca igual, dejandolo caminando derecho
//                 (Walking). De costado hace daño.
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
    Flying,
    Jumping,
    Walking,
    ShellIdle,
    ShellMoving,
};

enum class KoopaColor {
    Green,
    Red,
};

class Koopa {
public:
    // Especie/color: se fija una sola vez (no cambia en la vida del Koopa,
    // a diferencia de KoopaState). Verde por defecto -- solo hace falta
    // llamar esto para instancias rojas.
    void setColor(KoopaColor color) { color_ = color; }
    KoopaColor color() const { return color_; }

    // initialState: Walking (por defecto) para un Koopa de piso normal, o
    // Flying/Jumping para arrancar como Koopa Alado (ver KoopaState). En
    // Flying, "y" es la altura central del hamacado, no una posicion en el
    // piso (ver kFlyHoverAmplitude); en Jumping es la altura inicial nomas,
    // la gravedad se encarga del resto.
    void spawn(float x, float y, bool movingRight, KoopaState initialState = KoopaState::Walking);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, const Tilemap& level, int screenHeight);
    // worldRow selecciona la variante de color segun el tipo de mundo (el
    // sheet trae una fila por WorldType, mismo criterio que Goomba/Mushroom)
    // -- pero solo para el verde: el rojo ignora worldRow y siempre usa la
    // fila 0 de SU sheet (una unica paleta, "All Environments" en la hoja
    // de referencia). "sprite" tiene que ser la SpriteSheet que corresponda
    // al color() de este Koopa (koopaSprite_ o koopaRedSprite_ en Game.h).
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

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

    // Pisoton: Flying/Jumping -> deja de volar/saltar (Walking). Walking ->
    // se esconde (ShellIdle). ShellMoving -> se frena (ShellIdle, timer de
    // nuevo desde cero) y character queda inmune un rato (ver isImmuneTo)
    // -- sin eso, si el personaje vuelve a caer en el mismo lugar (rebota
    // parado sin moverse) lo pisa nuevamente ya quieto, y pisar-quieto lo
    // patea -- se veia como si "nunca se frenara". ShellIdle -> no hace nada
    // (el llamador igual hace rebotar al personaje).
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
    int height() const {
        const bool tall = state_ == KoopaState::Walking || state_ == KoopaState::Flying ||
                           state_ == KoopaState::Jumping;
        return tall ? kWalkHeight : kShellHeight;
    }

    static constexpr int kHitboxWidth = 16;
    static constexpr int kWalkHeight = 24;
    static constexpr int kShellHeight = 16;
    // Tamano del frame en el spritesheet (igual para las 6 columnas: walk1,
    // walk2, fly1, fly2, shell1, shell2 -- ver tools/asset_scripts, koopa.png).
    static constexpr int kFrameWidth = 16;
    static constexpr int kFrameHeight = 24;

private:
    // Devuelve true si esta llamada choco contra el piso (recien aterrizo) --
    // Jumping lo usa para volver a impulsarse hacia arriba en vez de quedarse
    // parado, ver update().
    bool applyVerticalPhysics(float dt, const Tilemap& level, int hitboxHeight);
    // fearsLedges: ademas de rebotar contra una pared, tambien da la vuelta
    // si no hay piso debajo del borde de avance (el rojo caminando/saltando,
    // ver KoopaColor). Un caparazon en movimiento nunca le teme a los
    // bordes sin importar el color, asi que siempre pasa false.
    void applyHorizontalWithWallBounce(float dt, const Tilemap& level, int hitboxHeight, bool fearsLedges);

    static constexpr float kWalkSpeed = 30.0f;                 // px/s, igual que el Goomba
    static constexpr float kShellSpeed = 210.0f;                // px/s, caparazon deslizando
    static constexpr float kGravity = 1000.0f;
    static constexpr float kMaxFallSpeed = 300.0f;
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kWalkFrameDuration = 0.25f;          // s por frame de caminata
    static constexpr float kFlyFrameDuration = 0.15f;           // s por frame de aleteo (mas rapido que caminar)
    static constexpr float kFlyHoverAmplitude = 12.0f;          // px, cuanto sube/baja desde la altura de spawn
    static constexpr float kFlyHoverSpeed = 2.5f;               // rad/s, velocidad del hamacado
    static constexpr float kJumpHopImpulse = -220.0f;           // px/s, impulso de cada salto al aterrizar
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
    KoopaColor color_ = KoopaColor::Green;
    bool lastFacingRight_ = true;
    float shellIdleTimer_ = 0.0f;
    float flyBaseY_ = 0.0f;
    float flyPhase_ = 0.0f;
    float postStompCooldown_ = 0.0f;
    const void* stompCooldownOwner_ = nullptr;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
