#pragma once

#include <SDL.h>

#include <string>

#include "../core/Input.h"
#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"
#include "../world/WorldType.h"

enum class MarioAnim {
    Idle,
    Walk,
    Skid,
    Jump,
    Throw,
    Swim,
    Duck,
};

enum class MarioSize {
    Small,
    Big,
    Fire,
};

// Mario chico/grande/fuego (seccion 5.1 de la especificacion): movimiento
// (correr, salto de altura variable, friccion, gravedad, patinada al
// frenar) contra un Tilemap de prueba, con el spritesheet recortado a
// partir de la hoja de referencia (formato de la seccion 8.1: grilla de
// frames de tamano fijo, una fila por animacion). El hitbox crece con el
// tamano (chico 16px de alto, grande/fuego 32px), manteniendo los pies en
// el mismo lugar al cambiar de estado.
//
// La fisica de Luigi es identica a la de Mario en el juego original, asi
// que esta misma clase sirve para instanciar a Luigi con su propio
// spritesheet (ver loadSprite) y su propio Input.
class Mario {
public:
    bool loadSprite(SDL_Renderer* renderer, const std::string& smallPath,
                     const std::string& bigPath = "", const std::string& firePath = "");
    // Variantes de paleta de estrella (3 bloques de filas apiladas en un solo
    // archivo, mismo layout de filas que el sheet base correspondiente).
    // Opcional: sin esto, setInvincible() no tiene efecto visual.
    bool loadStarSprite(SDL_Renderer* renderer, const std::string& smallStarPath,
                         const std::string& bigStarPath = "", const std::string& fireStarPath = "");
    void spawn(float x, float y);
    void update(float dt, Input& input, const Tilemap& level);
    // cameraX: ver Game::cameraX_, se resta a la posicion en X antes de
    // dibujar (mismo criterio que el resto del elenco).
    void render(SDL_Renderer* renderer, float cameraX = 0.0f) const;

    MarioSize size() const { return size_; }
    // Cambia de tamano manteniendo los pies fijos. No hace nada si el
    // spritesheet correspondiente no fue cargado (todavia sin power-ups
    // reales, esto lo usa por ahora una tecla de debug).
    void setSize(MarioSize newSize);

    // Info para instanciar una Fireball al lanzar (ver Game::update).
    bool consumeThrowRequest();
    // Game la llama una vez por frame despues de update(): si devuelve
    // true, este frame Mario choco de cabeza contra algo solido saltando
    // (fila/columnas del tile, para que Game.cpp chequee si era un ladrillo
    // rompible -- ver Game::checkBrickBlockHit). Se resetea solo, no hace
    // falta "consumirlo" como consumeThrowRequest.
    bool headBumpedThisFrame(int& row, int& col0, int& col1) const {
        row = headBumpRow_;
        col0 = headBumpCol0_;
        col1 = headBumpCol1_;
        return headBumped_;
    }
    // Game la llama solo si realmente se pudo instanciar la bola de fuego
    // (si los 2 slots ya estaban ocupados, no se reproduce la pose de tirar).
    void playThrowAnimation();
    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return kWidth; }
    int height() const { return currentHeight(); }
    // Altura fija de Mario/Luigi chico (a diferencia de height(), que
    // depende del tamano actual) -- publica porque Blooper.h la usa como
    // distancia de referencia para frenar antes de tocar el piso (ver
    // Blooper::kFloorAvoidDistance), sin necesitar una instancia de Mario.
    static constexpr int kSmallHeight = 16;
    bool facingRight() const { return facingRight_; }
    void setFacingRight(bool right) { facingRight_ = right; }
    bool isGrounded() const { return grounded_; }
    float velocityX() const { return velX_; }
    float velocityY() const { return velY_; }

    // Para que Game resuelva colisiones entre Mario y Luigi desde afuera
    // (ver Game::resolveCharacterCollision): empujones/rebotes directos,
    // sin pasar por el input del jugador.
    void setVelocityX(float v) { velX_ = v; }
    void setVelocityY(float v) { velY_ = v; }
    void nudgeX(float dx) { x_ += dx; }
    void setY(float newY) { y_ = newY; }
    // Fuerza la animacion de patinar por un instante (choque contra el otro
    // jugador), sin importar el estado real (incluso nadando, donde no
    // existiria patinada natural). Ver Game::resolveCharacterCollision.
    void playBumpAnimation(float duration = 0.2f) { bumpAnimTimer_ = duration; }
    // Evita rebotes en cadena: mientras dura, Game no aplica un nuevo
    // impulso de pisada aunque siga habiendo colision vertical con el otro.
    void setStompCooldown(float duration) { stompCooldownTimer_ = duration; }
    bool canStompBounce() const { return stompCooldownTimer_ <= 0.0f; }

    // Estrella: invencibilidad temporal con parpadeo de 4 paletas (la
    // original + 3 variantes). Sin power-up real todavia, se activa por
    // debug (ver Game::processEvents).
    void setInvincible(bool on, float duration = 10.0f);
    bool isInvincible() const { return invincible_; }

    // Ventana de gracia parpadeando (mismo parpadeo simple que queda tras la
    // secuencia de danio), sin pasar por toda esa secuencia. La usa Game al
    // reaparecer a alguien que murio, para que no lo vuelvan a matar de una.
    void grantRespawnInvincibility(float duration = kHurtInvincibleDuration);

    // Tipo de mundo actual (seccion 5.5): por ahora solo Underwater cambia
    // la fisica (nado en vez de salto, velocidad reducida). Lo setea Game
    // por tecla de debug hasta que los niveles tengan esto en su JSON.
    void setWorldType(WorldType type) { worldType_ = type; }
    WorldType worldType() const { return worldType_; }

    // Danio: si esta grande/fuego, arranca la secuencia clasica (se congela
    // el juego, parpadea achicandose, vuelve a chico e invulnerable un rato
    // parpadeando). Devuelve true si arranco la secuencia (para que Game
    // sepa que tiene que congelar todo). Si ya es chico, no hace nada -- el
    // llamador tiene que usar startDeath() en ese caso. No hace nada
    // tampoco si ya esta en una secuencia, o si ya es invulnerable
    // (estrella o post-danio).
    bool takeDamage();
    bool isInDamageSequence() const { return inDamageSequence_; }
    // Game la llama en vez de update() mientras dura la secuencia de danio
    // (todo lo demas del juego esta congelado ese rato).
    void updateDamageSequence(float dt);
    bool isHurtInvincible() const { return hurtInvincible_; }

    // Crecer (hongo o flor de fuego en chico): igual que el danio pero al
    // reves (chico -> grande/fuego), tambien congela el juego y parpadea.
    // targetSize es MarioSize::Big (hongo) o MarioSize::Fire (flor en
    // chico, que en el juego original crece Y se pone de fuego de una).
    // Solo tiene efecto si es chico; si ya es grande/fuego no arranca
    // secuencia (el llamador se encarga de igual "consumir" el power-up,
    // y de pasar directo a fuego con setSize() si corresponde).
    bool startGrow(MarioSize targetSize = MarioSize::Big);
    bool isInGrowSequence() const { return inGrowSequence_; }
    void updateGrowSequence(float dt);

    // Morir (chico + toca algo que hace danio): salta un poco, cae con la
    // pose de "muerto" congelada mientras el juego entero esta pausado, y
    // al terminar Game usa consumeDeathFinished() para reaparecerlo. Solo
    // tiene efecto si es chico (grande/fuego usa takeDamage() en cambio).
    bool startDeath();
    bool isInDeathSequence() const { return inDeathSequence_; }
    void updateDeathSequence(float dt);
    // true una sola vez, justo cuando termina la secuencia (se resetea al leerlo).
    bool consumeDeathFinished();

private:
    void updateHorizontal(float dt, const Input& input);
    void updateVertical(float dt, Input& input);
    void moveAndCollide(float dt, const Tilemap& level);
    void updateAnimation(float dt, const Input& input);
    int currentHeight() const;
    const SpriteSheet& currentSprite() const;
    const SpriteSheet& currentStarSprite() const;

    static constexpr int kWidth = 12;
    static constexpr int kBigHeight = 32;
    static constexpr int kSpriteFrameWidth = 16;

    // Constantes aproximadas a partir de las curvas de fisica documentadas
    // por la comunidad para SMB1 (px/frame a 60fps convertidas a px/s).
    // Pendientes de ajuste iterativo jugando, como pide la seccion 4 del doc.
    static constexpr float kMaxWalkSpeed = 93.75f;    // px/s
    static constexpr float kWalkAccel = 135.0f;       // px/s^2
    static constexpr float kMaxRunSpeed = 153.75f;    // px/s, con Action::Run mantenido
    static constexpr float kRunAccel = 202.5f;        // px/s^2
    static constexpr float kFriction = 183.0f;        // px/s^2
    static constexpr float kGravityAscend = 540.0f;   // px/s^2, salto sostenido
    static constexpr float kGravityDescend = 1575.0f; // px/s^2, caida / salto cortado
    static constexpr float kJumpImpulse = 260.0f;     // px/s (velocidad inicial hacia arriba)
    // Un poco mas alto si al saltar ya esta a velocidad maxima de carrera
    // (kMaxRunSpeed, con Action::Run mantenido) -- a pedido del usuario,
    // como en el original (correr a fondo da un salto mas alto).
    static constexpr float kRunJumpImpulse = 275.0f;  // px/s
    static constexpr float kMaxFallSpeed = 300.0f;    // px/s
    static constexpr float kSkidVelocityThreshold = 10.0f; // px/s, minimo para considerar patinada
    static constexpr float kThrowAnimDuration = 0.15f; // segundos que se ve la pose de tirar
    static constexpr float kStarFlickerInterval = 0.08f; // s entre cada paso del ciclo de 4 paletas

    // Fisica de nado (Underwater): todo mas lento y flotante, sin salto real.
    static constexpr float kSwimMaxSpeed = 47.0f;      // px/s
    static constexpr float kSwimAccel = 80.0f;         // px/s^2
    static constexpr float kSwimFriction = 90.0f;      // px/s^2
    static constexpr float kSwimGravity = 220.0f;      // px/s^2, se hunde despacio
    static constexpr float kSwimStrokeImpulse = -150.0f; // px/s, cada brazada
    static constexpr float kSwimMaxRiseSpeed = -120.0f;  // px/s, tope subiendo
    static constexpr float kSwimMaxSinkSpeed = 100.0f;   // px/s, tope hundiendose
    static constexpr float kSwimBaseFps = 8.0f;         // fps del ciclo de nado en reposo
    static constexpr float kSwimStrokeFps = 22.0f;      // fps del ciclo justo al dar una brazada
    static constexpr float kSwimStrokeBoostDuration = 0.25f; // s que dura el impulso de animacion por brazada

    // Secuencia de danio (grande/fuego -> chico), estilo NES.
    static constexpr float kDamageFreezeDuration = 2.0f;   // s totales congelado
    static constexpr float kDamageFlickerInterval = 0.07f; // s entre cada paso del parpadeo (grande/invisible/chico/invisible)
    static constexpr float kHurtInvincibleDuration = 2.0f; // s de invulnerabilidad post-danio
    static constexpr float kHurtFlickerInterval = 0.05f;   // s entre visible/invisible

    // Secuencia de crecer (chico -> grande), estilo NES.
    static constexpr float kGrowDuration = 1.0f;        // s totales congelado (mas corto que el danio)
    static constexpr float kGrowFlickerInterval = 0.08f; // s entre cada paso del parpadeo

    // Secuencia de morir (chico), estilo NES: salto chiquito y cae, con la
    // pantalla congelada (no choca contra el piso, sigue cayendo).
    static constexpr float kDeathDuration = 2.5f;      // s totales antes de reaparecer
    static constexpr float kDeathHopImpulse = -200.0f; // px/s, saltito inicial
    static constexpr float kDeathGravity = 600.0f;     // px/s^2

    float x_ = 0.0f;
    float y_ = 0.0f;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool grounded_ = false;
    bool facingRight_ = true;
    MarioSize size_ = MarioSize::Small;
    bool wantsThrow_ = false;
    bool isDucking_ = false;
    bool headBumped_ = false;
    int headBumpRow_ = 0;
    int headBumpCol0_ = 0;
    int headBumpCol1_ = 0;

    SpriteSheet spriteSmall_;
    SpriteSheet spriteBig_;
    SpriteSheet spriteFire_;
    SpriteSheet spriteSmallStar_;
    SpriteSheet spriteBigStar_;
    SpriteSheet spriteFireStar_;
    MarioAnim animState_ = MarioAnim::Idle;
    float walkAnimTimer_ = 0.0f;
    int walkFrame_ = 0;
    float throwAnimTimer_ = 0.0f;
    float swimAnimTimer_ = 0.0f;
    int swimFrame_ = 0;
    float swimStrokeBoostTimer_ = 0.0f;
    float bumpAnimTimer_ = 0.0f;
    float stompCooldownTimer_ = 0.0f;
    WorldType worldType_ = WorldType::Overworld;

    bool invincible_ = false;
    float invincibleTimer_ = 0.0f;
    float starFlickerTimer_ = 0.0f;
    int starCycleIndex_ = 0;  // 0 = paleta normal, 1..3 = variantes de estrella

    bool inDamageSequence_ = false;
    float damageSequenceTimer_ = 0.0f;
    float damageFlickerTimer_ = 0.0f;
    int damageFlickerPhase_ = 0;  // 0=grande, 1=invisible, 2=chico, 3=invisible (cicla)

    bool hurtInvincible_ = false;
    float hurtInvincibleTimer_ = 0.0f;
    float hurtFlickerTimer_ = 0.0f;
    bool hurtFlickerVisible_ = true;

    bool inGrowSequence_ = false;
    float growSequenceTimer_ = 0.0f;
    float growFlickerTimer_ = 0.0f;
    int growFlickerPhase_ = 0;  // 0=chico, 1=invisible, 2=grande/fuego, 3=invisible (cicla)
    MarioSize growTargetSize_ = MarioSize::Big;

    bool inDeathSequence_ = false;
    float deathSequenceTimer_ = 0.0f;
    bool deathFinished_ = false;
};
