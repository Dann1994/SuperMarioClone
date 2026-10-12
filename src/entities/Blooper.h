#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"
#include "Mario.h"

// Blooper (seccion 5.2, octavo enemigo, niveles de agua): flota, sin
// gravedad real. Ciclo de tres fases (BlooperState):
//   Resting   -> flota derecho hacia abajo, despacio (kFloatDownSpeed), sin
//                velocidad horizontal -- "en reposo", no se mueve en
//                diagonal. Dura kRestDuration, PERO si en el medio queda a
//                menos de kFloorAvoidDistance del piso (ver isNearFloor)
//                pasa a WindingUp de una, sin esperar el timer -- asi
//                siempre alcanza a frenar antes de tocarlo.
//   WindingUp -> mismo hundimiento derecho que Resting (sigue "en reposo"
//                para el movimiento), pero con los tentaculos recogidos
//                (columna 1 del sheet, ver render()) como aviso de que esta
//                por moverse bruscamente -- dura exactamente kWindupDuration.
//                Al terminar decide la diagonal del envion: horizontal
//                hacia donde este el personaje en X; vertical hacia donde
//                este en Y (arriba O abajo, no siempre arriba) -- SALVO que
//                siga cerca del piso, ahi fuerza para arriba sin importar
//                donde este el personaje (ver el comentario de arriba).
//   Bursting  -> envion recto en esa diagonal a kBurstSpeedX/Y durante
//                kBurstDuration, despues vuelve a Resting.
// Choque contra piso o pared (colision agregada como respaldo -- no hay
// piso en un nivel de agua real, pero test_blooper.json si tiene uno):
// corta el envion de una y vuelve a Resting. Con el frenado anticipado de
// arriba esto en teoria no deberia llegar a pasar nunca (ni tocar el piso
// ni, de rebote, poder alcanzar a un Mario chico parado en el piso), pero
// se deja como red de seguridad.
//
// Esta clase no conoce a Mario/Luigi ni recibe una instancia de ninguno de
// los dos (igual que Lakitu, Game.cpp le pasa la posicion del personaje mas
// cercano una vez por frame): el #include "Mario.h" es solo para usar
// Mario::kSmallHeight como distancia de referencia en kFloorAvoidDistance
// (ver arriba), no agrega ninguna dependencia de comportamiento.
//
// Sin caparazon ni pisoton (Game::checkBlooperCollision): a diferencia del
// resto del elenco con parabola de muerte propia, ACA el pisoton NO lo mata
// -- solo dana como un toque de costado, igual que la Pirana Plant/Spiny.
// Solo estrella o fuego lo eliminan, con la misma parabola de "se da vuelta
// y sale despedido" que el resto del elenco sin arte de muerte propio.
//
// Tiene una fila por WorldType en la hoja de referencia (igual que Lakitu,
// a diferencia del Spiny): render() recibe worldRow.
enum class BlooperState {
    Resting,
    WindingUp,
    Bursting,
};

class Blooper {
public:
    void spawn(float x, float y);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    // targetX/targetY: centro del personaje mas cercano (ver el comentario
    // de la clase) -- deciden la diagonal del envion al terminar WindingUp.
    // screenHeight: ver Goomba::update, mismo criterio para "fuera de
    // campo de vision" mientras cae dado vuelta.
    void update(float dt, const Tilemap& level, float targetX, float targetY, int screenHeight);
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
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 16;
    static constexpr int kHeight = 24;

private:
    static constexpr float kFloatDownSpeed = 30.0f;  // px/s: se hunde 1 px cada 2 cuadros, como el original
    static constexpr float kRestDuration = 0.7f;  // s: 2 intervalos (0x2) antes del proximo impulso
    static constexpr float kWindupDuration = 0.15f;  // s: pose de tentaculos recogidos antes de impulsarse
    static constexpr float kBurstDuration = 0.4f;  // s: 24 cuadros (velocidad 1, 2, 1 px/cuadro)
    static constexpr float kBurstSpeedX = 80.0f;  // px/s: ~32 px en el impulso, como el original
    static constexpr float kBurstSpeedY = 80.0f;  // px/s: ~32 px hacia arriba en el impulso
    // px: si el piso esta mas cerca que esto fuerza el aviso y el envion
    // para arriba. Igual a la altura de Mario/Luigi chico (Mario::kSmallHeight,
    // ver el comentario de la clase) en vez de un numero suelto: asi
    // alcanza para no tocar el piso NI a un Mario chico parado en el
    // (el caso mas alto que puede estorbar, ver kFloorAvoidDistance).
    static constexpr float kFloorAvoidDistance = static_cast<float>(Mario::kSmallHeight);
    static constexpr float kEdgeEpsilon = 0.01f;
    static constexpr float kGravity = 211.0f;  // px/s^2: caida lenta del original al derrotarlo (0x0f)
    static constexpr float kMaxFallSpeed = 120.0f;  // px/s: 2 px/cuadro
    static constexpr float kFallKillHopImpulse = -180.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;

    void applyHorizontalWithWallBounce(float dt, const Tilemap& level);
    // Devuelve true si esta llamada choco contra el piso (recien aterrizo).
    bool applyVerticalWithFloorCollision(float dt, const Tilemap& level);
    // Corta lo que este haciendo y vuelve a Resting desde cero (timer
    // reiniciado, sin velocidad horizontal) -- lo llama tanto el fin normal
    // del envion como el respaldo de colision contra piso/pared en pleno
    // Bursting.
    void backToResting();
    // true si hay un tile solido a menos de kFloorAvoidDistance por debajo
    // (ver el comentario de la clase) -- mira solo hacia abajo, no importa
    // la pared.
    bool isNearFloor(const Tilemap& level) const;

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    bool lastFacingRight_ = true;
    BlooperState state_ = BlooperState::Resting;
    float phaseTimer_ = 0.0f;
};
