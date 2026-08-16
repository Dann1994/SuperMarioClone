#pragma once

#include <SDL.h>

#include "../render/SpriteSheet.h"

// Bowser (seccion 5.2, jefe final): a diferencia del resto del elenco no
// patrulla una plataforma leida del tilemap sino una arena fija al final
// del puente -- camina de un lado al otro dentro de kPatrolRange alrededor
// del punto de spawn, rebotando en esos bordes (no hace falta wall-bounce
// por tiles, el arena de prueba es plana). Investigado contra el
// comportamiento real de SMB1 (1985) antes de programarlo: camina, salta
// cada tanto (parabola por gravedad, vuelve siempre al mismo groundY_ ya
// que la arena de prueba es plana, sin plataformas que trepar -- a
// diferencia de HammerBro no hace falta "comprometerse" a un salto largo).
//
// Solo usa las 4 poses de caminata (Documentaciones/Enemigos.png -- Green
// Enemies, fila "grande"), NO la fila agachada de mas abajo (esa trae
// pegado un iconito de mano+martillo que es una anotacion de la hoja de
// referencia, no arte real -- confirmado por el usuario, ver el historial).
// De esas 4, las primeras 2 son boca abierta y las otras 2 boca cerrada
// (confirmado visualmente: se le nota el colmillo/lengua en las primeras
// dos, en las otras dos tiene la boca sellada). Normalmente cicla boca
// abierta (caminando, o en el amague/tiro del martillo -- sin pose propia
// para eso, mismo criterio que HammerBro que tampoco tiene una) -- SOLO
// cierra la boca durante el amague antes de escupir fuego (ver render()), y
// la vuelve a abrir en el instante que lo escupe.
//
// En el original, distintos Bowser de distintos mundos tienen distinto
// "moveset" (los primeros solo escupen fuego, los de mundos mas avanzados
// tambien tiran martillos como un Hammer Bro -- en algunos, come 6-4, tira
// tantos martillos que hay que pasar por abajo en vez de saltarlo). Eso se
// elige con setAttackMode() (ver AttackMode mas abajo), fijo antes del
// primer spawn(), mismo criterio que Koopa::setColor:
//   FireOnly   -> solo fuego (BowserFire, ver BowserFire.h), en linea recta.
//   HammerOnly -> martillos, el MISMO objeto que usa HammerBro (ver
//                 consumeHammerThrowRequest/hammerOriginX.../Y -- Game.cpp
//                 es quien lo instancia, mismo patron pull-based que
//                 HammerBro::consumeThrowRequest) -- Bowser reusa esa
//                 entidad en vez de tener una propia. A diferencia del
//                 fuego, no tira uno solo por amague: tira una RAFAGA de
//                 kHammerBurstCount seguidos, cada kHammerBurstInterval
//                 (a pedido del usuario).
//   Both       -> alterna entre fuego (un tiro) y martillo (una rafaga
//                 entera), nunca dos seguidos del mismo tipo.
// Hacia donde MIRA (direccion de los ataques) es independiente de hacia
// donde CAMINA, mismo criterio que HammerBro (ver faceTowards()).
//
// Sin pisoton: cualquier contacto hace dano, ni la estrella lo derrota (ver
// Game::checkBowserCollision -- lo protege a Mario/Luigi del dano como
// siempre, pero a Bowser no le hace nada, igual que Podoboo). La UNICA
// forma de derrotarlo es killByFireball() cinco veces seguidas (necesita
// Mario/Luigi de fuego, como en el original) o killByAxe() de una sola vez
// (el hacha del final del puente, unica forma sin flor de fuego -- ver
// Axe.h). Cualquiera de las dos usa la misma parabola de "se da vuelta y
// sale despedido" que el resto del elenco sin arte de muerte propio (no
// tiene, ver el comentario de arriba).
enum class BowserAttackMode {
    FireOnly,
    HammerOnly,
    Both,
};

class Bowser {
public:
    // Fijo antes del primer spawn() (ver el comentario de la clase), mismo
    // criterio que Koopa::setColor.
    void setAttackMode(BowserAttackMode mode) { attackMode_ = mode; }
    BowserAttackMode attackMode() const { return attackMode_; }
    void spawn(float x, float y, bool movingRight);
    // Lo desactiva sin ninguna animacion de muerte: para cuando se cambia
    // de escenario de prueba y este no tiene que seguir vivo.
    void reset();
    void update(float dt, int screenHeight);
    // Hacia donde mira para atacar (independiente de por donde camina, ver
    // el comentario de la clase) -- llamar una vez por frame con la
    // posicion en X del personaje mas cercano, esta clase no conoce a
    // Mario/Luigi.
    void faceTowards(float targetX);
    void render(SDL_Renderer* renderer, const SpriteSheet& sprite, int worldRow, float cameraX = 0.0f) const;

    bool isAlive() const { return alive_; }
    bool isFalling() const { return falling_; }
    bool isDangerous() const { return alive_ && !falling_; }
    // Se queda en true para siempre despues de derrotarlo (aunque alive_
    // termine en false cuando la parabola de caida sale de pantalla) --
    // por si mas adelante hace falta saber "ya se gano la pelea".
    bool isDefeated() const { return defeated_; }

    // Ver el comentario de la clase: solo estas dos formas lo derrotan.
    // flyRight: mismo criterio que CheepCheep::killFalling/etc, lo decide
    // el llamador segun de que lado vino el golpe/toque.
    void killByFireball(bool flyRight);
    void killByAxe(bool flyRight);

    // Mientras dura el amague (boca cerrada, ver kAttackWindupDuration):
    // Game.cpp puede usarlo igual que HammerBro::isWindingUp().
    bool isWindingUp() const { return attackWindupTimer_ > 0.0f; }
    // Mismo patron pull-based que HammerBro::consumeThrowRequest(): Game.cpp
    // llama esto una vez por frame, si devuelve true tiene que instanciar un
    // BowserFire con fireOriginX/Y()/fireVelocityX()/fireMaxY() ya calculados.
    bool consumeFireRequest();
    float fireOriginX() const;
    float fireOriginY() const;
    float fireVelocityX() const;
    // El piso (ver el comentario del usuario: el desvio hacia abajo del
    // fuego nunca tiene que pasar de ahi) -- groundY_, no y_, para que de
    // el piso de verdad tambien cuando dispara en pleno salto (ver
    // kJumpImpulse) y no de su altura momentanea en el aire, que quedaria
    // mas arriba del piso real. BowserFire lo usa como tope fijo, no como
    // algo que siga a Bowser durante el vuelo.
    float fireMaxY() const { return groundY_ + static_cast<float>(kHeight); }
    // Mismo patron pull-based para el martillo (ver HammerOnly/Both en el
    // comentario de la clase): solo se activa alguna vez si attackMode_ no
    // es FireOnly.
    bool consumeHammerThrowRequest();
    float hammerOriginX() const;
    float hammerOriginY() const;
    float hammerVelocityX() const;
    float hammerVelocityY() const;

    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return kHeight; }
    static constexpr int kWidth = 32;
    static constexpr int kHeight = 32;

private:
    enum class AttackType { Fire, Hammer };

    static constexpr float kWalkSpeed = 15.0f;    // px/s, patrulla lenta
    static constexpr float kPatrolRange = 48.0f;  // px a cada lado del punto de spawn
    static constexpr float kGravity = 1000.0f;  // caida al derrotarlo (killFalling), no toca el salto en vivo
    static constexpr float kMaxFallSpeed = 300.0f;
    // Salto en el lugar (sigue caminando, ver el comentario de la clase):
    // gravedad propia bien floja para que sea lento y pesado, no un
    // brinquito -- misma altura de antes pero el doble de lento (mismo
    // criterio que CheepCheep::kJumpGravity, separada de kGravity de arriba
    // para no afectar la caida al derrotarlo).
    static constexpr float kJumpImpulse = -160.0f;   // px/s
    static constexpr float kJumpGravity = 320.0f;    // px/s^2
    static constexpr float kJumpIntervalMin = 1.5f;
    static constexpr float kJumpIntervalMax = 3.0f;
    static constexpr float kWalkFrameDuration = 0.25f;
    static constexpr float kAttackIntervalMin = 2.0f;
    static constexpr float kAttackIntervalMax = 3.5f;
    static constexpr float kAttackWindupDuration = 0.4f;   // amague antes de atacar de verdad
    static constexpr float kAttackRecoverDuration = 0.3f;  // un toque despues del ataque
    static constexpr float kFireSpeedX = 70.0f;
    static constexpr float kHammerSpeedX = 50.0f;
    static constexpr float kHammerSpeedY = -270.0f;  // arco hacia arriba, igual que HammerBro
    // Rafaga de martillos (ver el comentario de la clase): cuantos y cada
    // cuanto, uno detras del otro.
    static constexpr int kHammerBurstCount = 8;
    static constexpr float kHammerBurstInterval = 0.15f;
    // Altura de la boca (medido a pixel contra el sprite, ver el
    // comentario de fireOriginX()): siempre aparece ahi, sin importar hacia
    // donde termine yendo (ver BowserFire::kVerticalShiftPixels/Duration --
    // ese desvio progresivo hacia arriba o abajo es lo que hace que a
    // veces baje lo suficiente como para golpear a Mario/Luigi parado, en
    // vez de bajar el punto de aparicion en si).
    static constexpr float kFireOriginYOffset = 6.0f;
    static constexpr float kFallKillHopImpulse = -180.0f;
    static constexpr float kFallKillHorizontalSpeed = 60.0f;
    static constexpr int kFireballsToDefeat = 5;

    void killFalling(bool flyRight);

    float x_ = 0.0f;
    float y_ = 0.0f;
    float spawnX_ = 0.0f;
    float groundY_ = 0.0f;  // altura de spawn: donde apoya, a la que vuelve tras cada salto
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool alive_ = false;
    bool falling_ = false;
    bool defeated_ = false;
    bool lastFacingRight_ = false;
    float jumpTimer_ = 0.0f;
    float attackTimer_ = 0.0f;
    float attackWindupTimer_ = 0.0f;
    float attackRecoverTimer_ = 0.0f;
    BowserAttackMode attackMode_ = BowserAttackMode::FireOnly;
    // Que ataque toca esta vez (fijado al arrancar el amague, no cambia
    // hasta el proximo -- ver el comentario de la clase) y cual le toca
    // DESPUES (solo relevante con Both, para alternar).
    AttackType currentAttack_ = AttackType::Fire;
    AttackType nextAttack_ = AttackType::Fire;
    bool fireRequested_ = false;
    bool hammerThrowRequested_ = false;
    // Cuantos le quedan por tirar en la rafaga actual (0 = no esta en
    // rafaga) y cuanto falta para el proximo (ver kHammerBurstCount/Interval).
    int hammerBurstRemaining_ = 0;
    float hammerBurstTimer_ = 0.0f;
    int fireballHits_ = 0;
    float animTimer_ = 0.0f;
    int animFrame_ = 0;
};
