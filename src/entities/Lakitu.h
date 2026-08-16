#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Lakitu (seccion 5.2): flota arriba en su nube, sin gravedad ni piso --
// seguis al personaje mas cercano en X mientras se hamaca suave en Y, y
// cada tanto tira un huevo de Spiny (ver consumeThrowRequest -- Spiny.h es
// quien existe como entidad separada y arranca en SpinyState::Egg con el
// impulso que le pasa throwOriginX/Y/throwVelocityX/Y): aparece por encima
// de la cabeza del Lakitu con un empujon hacia arriba, y de ahi la gravedad
// de Spiny se encarga de que caiga, como una tirada real en vez de solo
// soltarlo. Esta clase no conoce a Mario/Luigi: igual que
// HammerBro::faceTowards, Game.cpp le pasa la posicion en X del personaje
// mas cercano una vez por frame.
//
// El seguimiento tiene dos velocidades (ver update()): normal (kFollowSpeed,
// lento) o una rafaga mas rapida (kCatchUpSpeed) que lo pasa de largo hasta
// quedar ADELANTE del personaje (kAheadOffset) en vez de solo emparejarlo.
// Que dispara la rafaga: en el original de 1985 (ver PlayerLakituDiff en la
// disassembly de la comunidad, smbdis.asm) la velocidad del Lakitu esta
// enganchada a ScrollAmount -- cuanto se movio la CAMARA ese cuadro, no la
// posicion cruda del personaje. Mientras el personaje esta parado en el
// medio de la pantalla (todavia no empujo la camara, ver el "dead zone" de
// scroll del original) el Lakitu se mueve lento aunque el personaje este
// lejos; en cuanto la camara arranca a scrollear de verdad, el Lakitu
// acompaña ese mismo movimiento a toda velocidad -- por eso el personaje
// nunca lo puede dejar atras mientras avanza: no esta compitiendo contra la
// velocidad del personaje, esta compitiendo contra la de la camara, que es
// la misma. Aca se replica con Game::cameraScrollSpeed_ (ver el parametro
// cameraScrollSpeed de update()): mientras haya scroll activo, rafaga
// (sentido = signo de cameraScrollSpeed, no hace falta que esta clase
// conozca facingRight de nadie); sin scroll, cae al criterio viejo de
// distancia (kCatchUpThreshold) como respaldo -- por ejemplo si el
// personaje retrocede dentro de la pantalla sin llegar a mover la camara.
//
// Sin caparazon ni frame de achatado propio en el arte fuente (solo dos
// poses de la cara, ver Documentaciones/Lakitu.png): cualquier forma de
// morir (pisoton, estrella o fuego) usa la misma parabola de "se da vuelta
// y sale despedido" que el resto del elenco sin arte de muerte propio
// (Goomba/HammerBro) -- a diferencia de la Pirana Plant, ACA el pisoton si
// lo mata (no hay razon para que un salto encima no lo tire de la nube).
//
// Las dos poses de la cara no son una animacion de hamacado: la columna 1
// (con los brazos por encima de la cabeza, agachandose hacia el huevo) es
// la pose de "esconderse" justo antes de tirar (ver kThrowWindupDuration/
// isWindingUp), columna 0 es la normal el resto del tiempo -- mismo criterio
// que HammerBro::isWindingUp, salvo que aca es la propia cara la que cambia
// en vez de superponer un sprite aparte.
//
// A diferencia del Spiny (Spiny.h), SI tiene una fila por WorldType en la
// hoja de referencia (Overworld/Underwater/Underground/Castle, esta ultima
// compartiendo el mismo arte que Underground -- mismo criterio que
// PiranhaPlant): render() recibe worldRow igual que el resto del elenco.
class Lakitu {
public:
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // targetX: centro en X del personaje mas cercano (ver el comentario de
    // la clase) -- decide hacia donde vuela y hacia donde mira.
    // cameraScrollSpeed: Game::cameraScrollSpeed_ de este mismo cuadro (ver
    // el comentario de la clase) -- que tan rapido y hacia donde esta
    // scrolleando la camara ahora mismo.
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, float targetX, float cameraScrollSpeed, int levelWidthPixels, int screenHeight);
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

    // Mientras esta en el "amague" (ver kThrowWindupDuration): se agacha
    // hacia el huevo un instante antes de tirarlo de verdad (columna 0 del
    // sheet, ver render()).
    bool isWindingUp() const { return throwWindupTimer_ > 0.0f; }
    // Game.cpp llama esto una vez por frame; si devuelve true, tiene que
    // instanciar un Spiny (ver Spiny.h) en SpinyState::Egg con la posicion/
    // velocidad que devuelven throwOriginX/Y() y throwVelocityX/Y() -- mismo
    // patron que HammerBro::consumeThrowRequest() con los martillos.
    bool consumeThrowRequest();
    float throwOriginX() const;
    float throwOriginY() const;
    float throwVelocityX() const;
    float throwVelocityY() const;

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 24;

private:
    static constexpr float kFollowSpeed = 40.0f;         // px/s, velocidad normal (sin scroll activo, ver el comentario de la clase)
    static constexpr float kFollowDeadzone = 8.0f;        // px, no corrige diferencias mas chicas que esto (evita tembleque)
    // Por debajo de esto, cameraScrollSpeed se considera "sin scroll" (ruido
    // de punto flotante, no un movimiento real de camara).
    static constexpr float kScrollActiveThreshold = 5.0f;
    // Respaldo sin scroll activo (ver el comentario de la clase): cuanto se
    // puede alejar el personaje (en X) antes de arrancar la rafaga igual.
    static constexpr float kCatchUpThreshold = 60.0f;
    static constexpr float kCatchUpSpeed = 110.0f;       // px/s durante la rafaga, mas rapido que kFollowSpeed
    // Cuanto se pasa de largo la rafaga antes de frenar: termina cuando
    // queda esta distancia POR DELANTE del personaje (no al llegar a su
    // misma X), para terminar realmente adelante y no al lado -- pero solo
    // si para entonces el scroll tambien ya se corto (ver update()).
    static constexpr float kAheadOffset = 24.0f;
    static constexpr float kHoverAmplitude = 6.0f;        // px, cuanto sube/baja desde la altura de spawn
    static constexpr float kHoverSpeed = 2.0f;            // rad/s, velocidad del hamacado
    static constexpr float kThrowInterval = 2.2f;         // s entre huevos
    // Cuanto se queda agachado antes de tirar de verdad (ver isWindingUp())
    // -- no cuenta como parte de kThrowInterval, se suma aparte.
    static constexpr float kThrowWindupDuration = 0.35f;
    static constexpr float kEggSpeedX = 40.0f;            // px/s horizontal del huevo, hacia donde mira
    static constexpr float kEggSpeedY = -170.0f;          // px/s vertical inicial del huevo (lo tira para arriba antes de que la gravedad lo haga caer)
    static constexpr float kSpawnAboveOffset = 10.0f;      // px por encima de la cabeza donde aparece el huevo (ver throwOriginY)
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
    bool lastFacingRight_ = true;
    // Rafaga en curso (ver el comentario de la clase) y hacia que lado --
    // se fija una sola vez al arrancar la rafaga, no se recalcula cuadro a
    // cuadro (si no, podria revertir de golpe si el personaje lo alcanza a
    // mitad de camino).
    bool catchingUp_ = false;
    bool catchUpRight_ = true;
    float baseY_ = 0.0f;
    float hoverPhase_ = 0.0f;
    float throwTimer_ = 0.0f;
    float throwWindupTimer_ = 0.0f;
    bool throwRequested_ = false;
};
