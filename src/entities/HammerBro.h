#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Hammer Bro (seccion 5.2): patrulla su plataforma caminando (rebota contra
// pared y le teme a los bordes -- siempre, no es un color aparte como el
// Koopa rojo, ver applyHorizontalWithWallBounce en el .cpp), saltando cada
// tanto (intervalo al azar, ver kJumpIntervalMin/Max), y tira martillos
// cada 0.8s en arco hacia donde esta mirando (ver consumeThrowRequest, el
// martillo en si es una entidad separada, Hammer.h -- Game.cpp es quien la
// instancia cuando este pide tirar). Timings calcados del original de 1985
// (ver ProcHammerBro/HammerBroJumpCode/HammerBroBGColl en smbdis.asm, la
// disassembly de la comunidad): HammerBroJumpTimer se recarga con un valor
// al azar entre 192 y 255 frames (3.2-4.25s a 60fps) para el proximo salto,
// y HammerThrowingTimer con 48 frames (0.8s) para el proximo martillo.
//
// Como sube y baja de una plataforma a otra (el original no distingue
// "plataforma" de piso solido para esto, ver el comentario de
// applyVerticalPhysics): al saltar, ignora CUALQUIER piso por un ratito fijo
// (kJumpCommitDuration, ~32 frames/0.5s en el original) -- pura parabola por
// gravedad, sin aterrizar en nada. Recien cuando esa ventana se cierra
// vuelve a buscar piso bajo sus pies, aterrizando en lo que sea que haya en
// ese momento: una plataforma mas alta si el arco la alcanzo y todavia esta
// arriba de ella, la misma de antes si no se movio lo suficiente, o mas
// abajo (otra plataforma, o el piso principal) si para entonces ya se fue
// de largo. La mayoria de los saltos usan una ventana corta (vuelve a caer
// mas o menos donde estaba, o trepa si el arco lo llevo a otra plataforma);
// pero si esta parado sobre una plataforma (no el piso de verdad) al saltar,
// a veces (al azar, ver kDescendChance) la ventana es bien mas larga a
// proposito -- lo suficiente para que el mismo salto la atraviese de
// arriba a abajo y baje, en vez de rebotar en el mismo lugar. No hay
// entonces una logica de "bajar" separada de "saltar": es el mismo salto,
// solo que a veces mas largo.
//
// Hacia donde MIRA (sprite volteado y direccion del tiro) es independiente
// de hacia donde CAMINA: siempre esta mirando al personaje mas cercano (ver
// faceTowards(), que Game.cpp llama una vez por frame con esa posicion --
// esta clase no conoce a Mario/Luigi), aunque este caminando para el otro
// lado por el rebote contra pared/borde.
//
// A diferencia del resto del elenco no hay caparazon ni frame de achatado
// propio en el arte fuente (solo poses de caminata, ver
// Documentaciones/hammer bro.png): cualquier forma de morir (pisoton,
// estrella o fuego) usa la misma parabola de "se da vuelta y sale
// despedido" que el Goomba usa cuando lo mata la estrella/fuego, en vez de
// un pisoton normal que lo deje caminando o escondido.
class HammerBro {
public:
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, const Tilemap& level, int screenHeight);
    // A donde mira (sprite y direccion del tiro, ver el comentario de la
    // clase) -- no afecta hacia donde camina. Llamar una vez por frame con
    // la posicion en X del personaje mas cercano.
    void faceTowards(float targetX);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    bool isDangerous() const { return alive_ && !falling_; }

    // Pisoton, estrella o fuego: cualquiera lo mata igual (ver el
    // comentario de la clase), se da vuelta y sale disparado en parabola
    // hacia flyRight.
    void killFalling(bool flyRight);
    // Al cruzarse con otro enemigo: mismo efecto que chocar una pared
    // (hacia donde mira no se toca, ver faceTowards()).
    void bounceOffEnemy(float pushX) {
        x_ += pushX;
        velX_ = -velX_;
    }

    // Mientras esta en el "amague" (ver kThrowWindupDuration): sostiene el
    // martillo levantado sobre la cabeza un instante antes de tirarlo de
    // verdad. Game.cpp usa esto para dibujar el sprite del martillo quieto
    // ahi arriba (reusando hammerSprite_, no hace falta un frame propio de
    // HammerBro para la pose).
    bool isWindingUp() const { return throwWindupTimer_ > 0.0f; }
    // Game.cpp llama esto una vez por frame; si devuelve true, tiene que
    // instanciar un Hammer (ver Hammer.h) con la posicion/velocidad que
    // devuelven throwOriginX/Y() y throwVelocityX/Y() ya calculadas segun
    // hacia donde esta mirando -- mismo patron que
    // Mario::consumeThrowRequest() con las bolas de fuego.
    bool consumeThrowRequest();
    float throwOriginX() const;
    float throwOriginY() const;
    float throwVelocityX() const;
    float throwVelocityY() const;

    float x() const { return x_; }
    float y() const { return y_; }
    float velocityX() const { return velX_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 24;

private:
    void applyHorizontalWithWallBounce(float dt, const Tilemap& level);
    // Devuelve true si esta llamada choco contra el piso (recien aterrizo).
    // Mientras dura jumpCommitTimer_ no choca contra nada (ver el
    // comentario de la clase), asi que nunca devuelve true en ese lapso.
    bool applyVerticalPhysics(float dt, const Tilemap& level);

    static constexpr float kWalkSpeed = 20.0f;           // px/s, mas lento que el resto: patrulla, no persigue
    static constexpr float kGravity = 1000.0f;
    static constexpr float kMaxFallSpeed = 300.0f;
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kWalkFrameDuration = 0.2f;
    static constexpr float kJumpImpulse = -420.0f;       // px/s, salto normal (puede trepar)
    // Salto mas corto/bajo para cuando le toca bajar (ver kDescendChance):
    // no hace falta tomar altura para atravesar la plataforma en la que ya
    // esta parado, un saltito chico alcanza.
    static constexpr float kDescendJumpImpulse = -180.0f;
    // s entre saltos, al azar en este rango cada vez (ver el comentario de
    // la clase -- 192-255 frames a 60fps en el original).
    static constexpr float kJumpIntervalMin = 3.2f;
    static constexpr float kJumpIntervalMax = 4.25f;
    // Cuanto dura la parabola "a ciegas" de cada salto sin poder aterrizar
    // en nada (ver el comentario de la clase). kDescendCommitDuration es la
    // version larga (a proposito, no calcada del original) para cuando le
    // toca bajar atravesando la plataforma en la que esta parado en vez de
    // solo rebotar -- tiene que durar lo suficiente para todo el arco
    // (subida + bajada, ahora con kDescendJumpImpulse en vez de
    // kJumpImpulse) mas lo justo para despegarse del todo de esa plataforma.
    static constexpr float kJumpCommitDuration = 0.5f;
    static constexpr float kDescendCommitDuration = 0.55f;
    // Si esta parado sobre una plataforma (no el piso de verdad) al saltar,
    // probabilidad de que ese salto en particular sea el largo (baja) en
    // vez del corto (rebota/trepa).
    static constexpr float kDescendChance = 0.4f;
    static constexpr float kThrowInterval = 1.1f;        // s entre martillos (un poco mas lento que el original)
    // Cuanto lo sostiene levantado sobre la cabeza antes de tirarlo de
    // verdad (ver isWindingUp()) -- no cuenta como parte de kThrowInterval,
    // se suma aparte.
    static constexpr float kThrowWindupDuration = 0.18f;
    static constexpr float kHammerSpeedX = 50.0f;        // px/s horizontal del martillo, hacia donde mira
    static constexpr float kHammerSpeedY = -270.0f;      // px/s vertical inicial del martillo (arco hacia arriba)
    static constexpr float kFallKillHopImpulse = -120.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    bool lastFacingRight_ = true;
    float jumpTimer_ = 0.0f;
    float jumpCommitTimer_ = 0.0f;
    bool grounded_ = false;
    bool onPlatform_ = false;
    float throwTimer_ = 0.0f;
    float throwWindupTimer_ = 0.0f;
    bool throwRequested_ = false;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
