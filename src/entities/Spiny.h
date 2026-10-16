#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"

// Spiny (seccion 5.2): el huevo que tira el Lakitu (Lakitu.h) al tocar el
// piso "eclosiona" y se convierte en este mismo enemigo caminando -- una
// sola clase con SpinyState como eje de estado (mismo criterio que Koopa con
// KoopaColor/KoopaState), no dos entidades separadas, porque a diferencia
// del Hammer Bro/martillo aca es literal el mismo bicho antes y despues de
// tocar el piso, no un proyectil aparte.
//
// Egg    -> cae por gravedad con el empuje horizontal que le dio el Lakitu
//           al tirarlo (ver Lakitu::throwVelocityX/Y), sin rebotar contra
//           paredes (muy poco recorrido como para que importe). Se dibuja
//           como bola enrollada girando (columnas 0/1 de
//           assets/sprites/spiny.png, ver render()). Al tocar el piso o
//           una plataforma, eclosiona: pasa a Walking con esa misma
//           direccion horizontal.
// Walking -> misma fisica que el Goomba (Goomba.h): rebota contra pared,
//           cae por gravedad, SIN miedo a los bordes (se cae, como el
//           original -- a diferencia del Koopa rojo). Se dibuja con las
//           pas afuera caminando (columnas 2/3 del sheet).
//
// Una sola paleta en la hoja de referencia ("Red Enemies - All
// Environments", igual que el Koopa rojo): render() siempre usa la fila 0
// del sheet, no hay worldRow.
//
// A diferencia de todo el resto del elenco, las espinas no tienen pisoton: CUALQUIER
// contacto (de arriba tambien) hace dano, en Egg o en Walking por igual --
// solo estrella o fuego lo eliminan (Game::checkSpinyCollision/
// checkFireballSpinyCollision), con la misma parabola de "se da vuelta y
// sale despedido" que el resto usa para esas dos formas de morir.
enum class SpinyState {
    Egg,
    Walking,
};

class Spiny {
public:
    // Le cayo encima al caparazon del traje de Hammer Bro: rebota para
    // arriba. true si recien rebota (venia cayendo), para el sonido.
    bool bounceUp(float velY) {
        const bool wasFalling = velY_ >= 0.0f;
        velY_ = velY;
        return wasFalling;
    }
    // velX/velY: impulso inicial mientras esta en Egg (ver el comentario de
    // la clase) -- lo tira Lakitu, no lo elige este spawn.
    void spawn(float x, float y, float velX, float velY);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, const Tilemap& level, int screenHeight);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de dibujar.
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    SpinyState state() const { return state_; }
    // Egg y Walking son ambos "peligrosos" (ver el comentario de la clase);
    // solo deja de serlo al morir (falling_, la parabola de estrella/fuego).
    bool isDangerous() const { return alive_ && !falling_; }

    // Estrella o fuego: se da vuelta y sale disparado en parabola hacia
    // flyRight, igual que el Goomba.
    bool killFalling(bool flyRight);
    // Golpe desde abajo al bloque donde esta: el original lo da vuelta y se
    // recupera al aterrizar -- aca, un saltito y sigue caminando.
    void bumpFromBelow() {
        if (!alive_ || falling_) return;
        y_ -= 2.0f;
        velY_ = -180.0f;
    }
    // Al cruzarse con otro Spiny caminando (Game::checkSpinyBounce): mismo
    // efecto que chocar una pared, invierte direccion, mas un empuje pushX
    // para separarlos y que no queden rebotando en el mismo lugar.
    void bounceOffEnemy(float pushX) {
        x_ += pushX;
        velX_ = -velX_;
        lastFacingRight_ = velX_ > 0.0f;
    }

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kSize; }
    int height() const { return kSize; }
    static constexpr int kSize = 16;

private:
    static constexpr float kWalkSpeed = 30.0f;           // px/s, igual que el Goomba
    static constexpr float kEggGravity = 450.0f;  // px/s^2, original: 0x20 para el huevo
    static constexpr float kGravity = 858.0f;  // px/s^2, original: 0x3d/256 px/cuadro^2
    static constexpr float kMaxFallSpeed = 180.0f;  // px/s, original: 3 px/cuadro
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kWalkFrameDuration = 0.25f;
    static constexpr float kFallKillHopImpulse = -180.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    SpinyState state_ = SpinyState::Egg;
    bool lastFacingRight_ = true;
    float walkAnimTimer_ = 0.0f;
    int walkFrame_ = 0;
};
