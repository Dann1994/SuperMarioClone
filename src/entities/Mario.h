#pragma once

#include <array>

#include <SDL.h>

#include <string>
#include <vector>

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
    Climb,
};

enum class MarioSize {
    Small,
    Big,
    Fire,
    // Traje de Hammer Bro (como en Mario 3): tira martillos con el boton de
    // correr y agachado se mete en el caparazon (no le hace dano el fuego).
    Hammer,
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
    // true una sola vez el frame en que arranca un salto de verdad (parado y
    // se apreto el boton -- no en cada frame que sigue en el aire), para que
    // Game.cpp sepa cuando reproducir el sonido de salto (grande o chico
    // segun size(), no hace falta que esta clase sepa nada de audio). Se
    // resetea solo, mismo criterio que headBumpedThisFrame().
    bool consumeJumpRequest() {
        const bool value = jumpRequested_;
        jumpRequested_ = false;
        return value;
    }
    // Idem, para el sonido de cada brazada nadando (Underwater, tecla de
    // saltar): true el frame en que arranca una brazada nueva. Este SI se
    // consume (un solo golpe, no loop -- cada brazada es corta).
    // Adentro de un remolino (lo decide Game cada cuadro).
    void setInWhirlpool(bool on) { inWhirlpool_ = on; }
    bool inWhirlpool() const { return inWhirlpool_; }
    bool consumeSwimStrokeSoundRequest() {
        const bool value = swimStrokeSoundRequested_;
        swimStrokeSoundRequested_ = false;
        return value;
    }
    float x() const { return x_; }
    float y() const { return y_; }
    int width() const { return currentWidth(); }
    int height() const { return currentHeight(); }
    // Tope del FRAME del sprite (mas arriba que y(), que es el tope del
    // hitbox -- ver el comentario de kSmallHitboxWidth): lo usa
    // Game::trySpawnFireball para que la bola de fuego salga de la mano en
    // vez de a una altura arbitraria (mismo punto de referencia que usa el
    // original para esto, Player_Y_Position).
    float spriteTopY() const { return y_ - static_cast<float>(currentSpriteFrameHeight() - currentHeight()); }
    // Altura fija de Mario/Luigi chico (a diferencia de height(), que
    // depende del tamano actual) -- publica porque Blooper.h la usa como
    // distancia de referencia para frenar antes de tocar el piso (ver
    // Blooper::kFloorAvoidDistance), sin necesitar una instancia de Mario.
    static constexpr int kSmallHeight = 16;
    bool facingRight() const { return facingRight_; }
    void setFacingRight(bool right) { facingRight_ = right; }
    bool isGrounded() const { return grounded_; }
    // Mueve al personaje en X sin tocar velocidad ni estado (laberintos de
    // castillo: el original corre al jugador 4 paginas para atras).
    void shiftX(float dx) { x_ += dx; }
    float velocityX() const { return velX_; }
    float velocityY() const { return velY_; }

    // Para que Game resuelva colisiones entre Mario y Luigi desde afuera
    // (ver Game::resolveCharacterCollision): empujones/rebotes directos,
    // sin pasar por el input del jugador.
    void setVelocityX(float v) { velX_ = v; }
    void setVelocityY(float v) { velY_ = v; }
    // Animacion de caminar sin moverse por su cuenta (lo mueve Game: entrar
    // caminando a una tuberia lateral).
    void stepWalkAnimation(float dt);
    void standStill() { animState_ = MarioAnim::Idle; }
    // Trampolin (ChkForLandJumpSpring): sale con la gravedad fuerte
    // (VerticalForce 0x70) aunque se mantenga saltar, hasta empezar a caer.
    void launchFromSpring(float velocityY) {
        velY_ = velocityY;
        springLaunch_ = true;
    }
    void nudgeX(float dx) { x_ += dx; }
    void nudgeY(float dy) { y_ += dy; }
    // Para las animaciones que mueven al personaje desde Game (tuberia):
    // apoyado se dibuja parado en vez de en pose de salto.
    void setGrounded(bool grounded) { grounded_ = grounded; }
    // Plataformas moviles del nivel (rectangulos en px, ver
    // Game::updatePlatforms): moveAndCollide las trata como piso de un solo
    // sentido -- se apoya cayendo desde arriba, se atraviesan subiendo. Al
    // ser parte de su propia colision cuenta como apoyado para todo
    // (animacion, poder saltar), igual que el piso de tiles.
    // platformIndex: en cual quedo parado este cuadro (-1 si en ninguna).
    void setPlatforms(const std::vector<SDL_FRect>* platforms) { platforms_ = platforms; }
    int platformIndex() const { return platformIndex_; }
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
    // Estrella del original: StarInvincibleTimer 0x23 intervalos de 21
    // cuadros (12,25 s).
    void setInvincible(bool on, float duration = kStarDuration);
    bool isInvincible() const { return invincible_; }
    // La musica de la estrella se corta antes del final (StarInvincibleTimer
    // == 4: quedan 4 intervalos, 1,4 s) y vuelve la del nivel.
    bool isStarMusicTime() const { return invincible_ && invincibleTimer_ > kStarMusicEndsAt; }

    // Ventana de gracia parpadeando (mismo parpadeo simple que queda tras la
    // secuencia de danio), sin pasar por toda esa secuencia. La usa Game al
    // reaparecer a alguien que murio, para que no lo vuelvan a matar de una.
    void grantRespawnInvincibility(float duration = kHurtInvincibleDuration);

    // Tipo de mundo actual (seccion 5.5): por ahora solo Underwater cambia
    // la fisica (nado en vez de salto, velocidad reducida). Lo setea Game
    // por tecla de debug hasta que los niveles tengan esto en su JSON.
    // Tambien cambia la paleta del personaje si hay una para ese mundo (ver
    // loadPaletteSprites); si no, usa la normal.
    void setWorldType(WorldType type);
    // Intercambia todos los sprites (y paletas) con otro personaje: para
    // jugar con Luigi en 1 jugador usando el objeto de Mario.
    void swapAppearance(Mario& other);
    // Paleta alternativa para un tipo de mundo (los tres tamanos), ver
    // tools/asset_scripts/gen_player_palettes.py. Si algun archivo no existe
    // devuelve false y ese mundo usa la paleta normal.
    bool loadPaletteSprites(SDL_Renderer* renderer, WorldType type, const std::string& smallPath,
                            const std::string& bigPath, const std::string& firePath);
    WorldType worldType() const { return worldType_; }
    // Cambio de paleta de Mario/Luigi segun el tipo de mundo (las hojas
    // *_underground/_castle/_underwater.png): guardado pero apagado por
    // ahora (settings.json "player_world_palettes", sin opcion en el menu).
    static void setWorldPalettesEnabled(bool on) { sWorldPalettes = on; }

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
    // Muerte sin importar el tamano ni la invencibilidad (se le acabo el
    // tiempo o cayo a un pozo, como en el original): pasa a chico y arranca
    // la misma secuencia que startDeath(). hop = false para el pozo: sin el
    // saltito, si no volveria a asomar por el borde de abajo de la
    // pantalla. No hace nada en la bandera de meta.
    bool killInstantly(bool hop = true);
    bool isInDeathSequence() const { return inDeathSequence_; }
    void updateDeathSequence(float dt);
    // true una sola vez, justo cuando termina la secuencia (se resetea al leerlo).
    bool consumeDeathFinished();

    // Bandera de meta (seccion 5.3, ver Flagpole.h): al agarrar el mastil
    // deja de responder al input y hace la secuencia clasica sola --
    // baja deslizandose pegado al palo (pose de trepar), espera abajo hasta
    // que Game avisa que la bandera termino de bajar (releaseFromFlagpole),
    // se pasa al otro lado del palo mirando hacia el, y despues camina solo
    // hacia la derecha hasta exitTargetX (la puerta del castillo), donde
    // desaparece. Mientras dure no recibe danio ni muere.
    void startFlagpoleGrab(float poleCenterX, float bottomY, float exitTargetX);
    // Enredaderas (rectangulos trepables, los arma Game cada cuadro; como
    // HandleClimbing/ClimbingSub del original): se agarra al tocar la
    // columna de la enredadera y queda al costado (sprite a 7 px del borde
    // del tile, del lado de donde vino); arriba/abajo trepa (52,5 / 120
    // px/s), izquierda/derecha se corre 14 px al otro lado o 4 px hacia
    // afuera (cada 0x18 cuadros si se mantiene) y se suelta al dejar de
    // tocarla.
    // floors: por enredadera, 1 si su base es un piso (no se baja mas, la
    // de llegada a otra area); 0 si al bajar de la base se suelta.
    void setVines(const std::vector<SDL_FRect>* vines, const std::vector<uint8_t>* floors) {
        vines_ = vines;
        vineFloors_ = floors;
    }
    bool isClimbing() const { return climbing_; }
    int vineIndex() const { return climbing_ ? vineIndex_ : -1; }
    // Llega colgado de una enredadera (abajo de todo, sin moverse) para la
    // entrada trepando a otra area.
    void startVineEntry(int vineIndex, float vineCenterX, float feetY, bool leftSide);
    // Trepa solo hacia arriba (Vine_AutoClimb) hasta que los pies llegan a
    // targetFeetY (sin tope: hasta que Game cambie de area).
    void autoClimbTo(float targetFeetY) { climbAuto_ = true; climbAutoTargetFeet_ = targetFeetY; }
    bool isAutoClimbing() const { return climbing_ && climbAuto_; }
    // Se suelta del lado derecho (llegando a un area trepando).
    void stepOffVineRight();
    void releaseFromFlagpole();
    bool isOnFlagpole() const { return flagpolePhase_ != FlagpolePhase::None; }
    // Todavia agarrado al palo (bajando, esperando abajo o pasandose al
    // otro lado) -- quieto al lado del mastil, a diferencia de la caminata.
    bool isHoldingFlagpole() const {
        return flagpolePhase_ == FlagpolePhase::Slide || flagpolePhase_ == FlagpolePhase::Bottom ||
               flagpolePhase_ == FlagpolePhase::Flip;
    }
    bool isWaitingAtFlagpoleBottom() const { return flagpolePhase_ == FlagpolePhase::Bottom; }
    bool isSlidingFlagpole() const { return flagpolePhase_ == FlagpolePhase::Slide; }
    // Traje de Hammer Bro: su hoja (mario_hammer.png, cuadros de 16x34) y la
    // de la estrella (las 12 paletas, ver gen_mario_hammer.py).
    bool loadHammerSprite(SDL_Renderer* renderer, const std::string& path, const std::string& starPath = "");
    // Agachado con el traje de Hammer Bro: el caparazon lo cubre del fuego.
    bool isHammerShell() const { return size_ == MarioSize::Hammer && isDucking_; }
    // Ya entro al castillo (no se dibuja mas).
    bool hasFinishedLevel() const { return flagpolePhase_ == FlagpolePhase::Done; }

private:
    enum class FlagpolePhase { None, Slide, Bottom, Flip, Walk, Done };

    void updateFlagpole(float dt, const Tilemap& level);
    bool tryGrabVine(const Input& input);
    void updateClimb(float dt, Input& input, const Tilemap& level);
    void releaseVine();
    void snapToVineSide();
    void updateHorizontal(float dt, const Input& input);
    void updateVertical(float dt, Input& input);
    void moveAndCollide(float dt, const Tilemap& level);
    void updateAnimation(float dt, const Input& input);
    int currentHeight() const;
    int currentWidth() const;
    int currentSpriteFrameHeight() const;
    const SpriteSheet& currentSprite() const;
    const SpriteSheet& currentStarSprite() const;

    // Tamano de frame del spritesheet (recorte del PNG, seccion 8.1): NO es
    // el hitbox, es mas grande que el (el original tambien lo hace asi, ver
    // el comentario de kSmallHitboxWidth mas abajo) -- no tocar estos dos
    // sin volver a extraer los sprites, loadSprite() los usa para cortar la
    // grilla.
    static constexpr int kSpriteFrameWidth = 16;
    static constexpr int kBigHeight = 32;
    static constexpr int kHammerHeight = 34;   // el casco: 2 px mas alto que grande
    static constexpr int kHammerThrowTopHeight = 26;   // alto de la pose de tirar martillo (sin piernas)

    // Constantes sacadas de la tabla real de fisica de SMB1 (disassembly de
    // doppelganger, MaxLeftXSpdData/MaxRightXSpdData/FrictionData/
    // JumpMForceData/FallMForceData/PlayerYSpdData en subpixeles por frame a
    // 60fps, convertidas a px/s y px/s^2) -- investigado a pedido del
    // usuario para reemplazar los valores aproximados que habia antes.
    static constexpr float kMaxWalkSpeed = 90.0f;         // px/s (0x18 = 1.5px/frame)
    static constexpr float kMaxRunSpeed = 150.0f;         // px/s, con Action::Run mantenido (0x28 = 2.5px/frame)
    // El original usa el MISMO valor para acelerar caminando y para frenar
    // (soltar todo o patinar en sentido contrario): no hay una constante de
    // friccion separada de verdad, kFriction = kWalkAccel a proposito.
    static constexpr float kWalkAccel = 133.59375f;       // px/s^2 (0x98/256 subpixel/frame, /16 y *3600)
    static constexpr float kRunAccel = 200.390625f;       // px/s^2 (0xe4, con Action::Run mantenido)
    static constexpr float kFriction = kWalkAccel;        // px/s^2, frenar/patinar (mismo valor que caminar)
    // Gravedad y velocidad inicial de salto: el original usa dos juegos de
    // valores segun que tan rapido se este moviendo Mario AL SALTAR (mas de
    // 5 umbrales en realidad, ver JumpMForceData/PlayerYSpdData en el
    // disassembly) -- se simplifica a los dos casos extremos (parado/
    // caminando vs corriendo a fondo) para que coincida con el diseño de
    // "salto un poco mas alto corriendo a fondo" que ya se habia pedido.
    static constexpr float kGravityAscend = 450.0f;       // px/s^2, salto sostenido (parado/caminando)
    static constexpr float kGravityDescend = 1575.0f;     // px/s^2, caida / salto cortado (parado/caminando)
    // 255 = 4.25 px/cuadro: con 240 (4 px/cuadro) la altura maxima daba
    // justo 64 px (v^2 / 2g con kGravityAscend) y parado no alcanzaba para
    // subirse a los bloques a 4 de altura del principio del 1-1.
    static constexpr float kJumpImpulse = 255.0f;         // px/s, velocidad inicial (parado/caminando)
    static constexpr float kGravityAscendRun = 562.5f;    // px/s^2, salto sostenido (corriendo a fondo)
    static constexpr float kGravityDescendRun = 2025.0f;  // px/s^2, caida / salto cortado (corriendo a fondo)
    static constexpr float kRunJumpImpulse = 300.0f;      // px/s, velocidad inicial (corriendo a fondo)
    // Velocidad horizontal a partir de la cual se usa el juego de salto de
    // "corriendo a fondo" (kRunJumpImpulse/kGravity*Run) en vez del normal
    // -- umbral real del original (0x19 = 1.5625px/frame), no "estar al
    // maximo de kMaxRunSpeed" como antes.
    static constexpr float kRunJumpThreshold = 93.75f;    // px/s
    static constexpr float kMaxFallSpeed = 240.0f;        // px/s, velocidad terminal de caida (0x04 = 4px/frame)
    static constexpr float kSkidVelocityThreshold = 41.25f; // px/s, minimo para la animacion de patinada (0x0b)
    static constexpr float kThrowAnimDuration = 0.15f; // segundos que se ve la pose de tirar
    // Cambio de colores: cada 2 cuadros, y en el tramo final (menos de 8
    // intervalos, 2,8 s) cada 8, mas lento, avisando que se termina.
    static constexpr float kStarFlickerInterval = 2.0f / 60.0f;
    static constexpr float kStarFlickerSlowInterval = 8.0f / 60.0f;
    static constexpr float kStarSlowFlickerAt = 8.0f * 21.0f / 60.0f;
    static constexpr float kStarDuration = 0x23 * 21.0f / 60.0f;
    static constexpr float kStarMusicEndsAt = 4.0f * 21.0f / 60.0f;

    // Velocidad del ciclo de caminar/correr: el original NO interpola
    // continuo, usa 3 escalones fijos segun la velocidad horizontal
    // (PlayerAnimTmrData en el disassembly, en frames por paso de
    // animacion: 2/4/7 a 60fps).
    static constexpr float kAnimSpeedFastThreshold = 105.0f;   // px/s (0x1c)
    static constexpr float kAnimSpeedMediumThreshold = 52.5f;  // px/s (0x0e)
    static constexpr float kAnimFpsFast = 30.0f;    // 60/2
    static constexpr float kAnimFpsMedium = 15.0f;  // 60/4
    static constexpr float kAnimFpsSlow = 60.0f / 7.0f;

    // Hitbox real (BoundBoxCtrlData en el disassembly): mas chico que el
    // frame del sprite de arriba, alineado por los pies (el sprite se
    // dibuja un poco mas arriba de x_,y_ para que la base coincida, ver
    // render()) -- antes el hitbox usaba directamente el tamano del frame
    // completo. kCrouchHitboxHeight es grande/fuego agachado (isDucking_):
    // mismo ancho que parado, la mitad de alto.
    static constexpr int kSmallHitboxWidth = 10;
    static constexpr int kSmallHitboxHeight = 12;
    static constexpr int kBigHitboxWidth = 12;
    static constexpr int kBigHitboxHeight = 24;
    static constexpr int kCrouchHitboxHeight = 12;

    // Fisica de nado (Underwater): todo mas lento y flotante, sin salto real.
    static constexpr float kSwimMaxSpeed = 47.0f;      // px/s
    static constexpr float kSwimAccel = 80.0f;         // px/s^2
    static constexpr float kSwimFriction = 90.0f;      // px/s^2
    static constexpr float kSwimGravity = 220.0f;      // px/s^2, se hunde despacio
    static constexpr float kSwimStrokeImpulse = -150.0f; // px/s, cada brazada
    // Remolino (ProcessWhirlpools del original): gravedad de mas (0x10/256
    // px por cuadro^2) y la brazada sube la mitad (-1 en vez de -2 px/cuadro).
    static constexpr float kWhirlpoolPull = 16.0f / 256.0f * 3600.0f;   // px/s^2
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
    bool jumpRequested_ = false;
    bool springLaunch_ = false;
    bool swimStrokeSoundRequested_ = false;
    bool inWhirlpool_ = false;
    bool isDucking_ = false;
    bool headBumped_ = false;
    int headBumpRow_ = 0;
    int headBumpCol0_ = 0;
    int headBumpCol1_ = 0;

    SpriteSheet spriteSmall_;
    SpriteSheet spriteBig_;
    SpriteSheet spriteFire_;
    SpriteSheet spriteHammer_;
    SpriteSheet spriteHammerStar_;
    // Paletas por tipo de mundo: la activa vive en spriteSmall_/Big_/Fire_ y
    // su lugar aca queda vacio; cambiar de paleta es intercambiar texturas.
    std::array<std::array<SpriteSheet, 3>, 4> paletteSprites_;
    std::array<bool, 4> paletteLoaded_{true, false, false, false};  // Overworld = la normal
    int activePalette_ = 0;
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

    const std::vector<SDL_FRect>* platforms_ = nullptr;
    int platformIndex_ = -1;

    FlagpolePhase flagpolePhase_ = FlagpolePhase::None;
    float flagpolePoleX_ = 0.0f;
    float flagpoleBottomY_ = 0.0f;
    float flagpoleExitX_ = 0.0f;
    float flagpoleTimer_ = 0.0f;
    int climbFrame_ = 0;
    const std::vector<SDL_FRect>* vines_ = nullptr;
    const std::vector<uint8_t>* vineFloors_ = nullptr;
    bool climbing_ = false;
    int vineIndex_ = -1;
    float vineX_ = 0.0f;
    bool climbAuto_ = false;
    float climbAutoTargetFeet_ = 0.0f;
    float climbAnimTimer_ = 0.0f;
    float vineGrabCooldown_ = 0.0f;
    float climbSideTimer_ = 0.0f;
    static constexpr float kClimbUpSpeed = 0.875f * 60.0f;  // px/s (-1 + 0x20/256 por cuadro)
    static constexpr float kClimbDownSpeed = 2.0f * 60.0f;  // px/s
    // Mismo valor que Flagpole::kLowerSpeed (la bandera baja a la par).
    static constexpr float kFlagpoleSlideSpeed = 120.0f;  // px/s
    static constexpr float kClimbFrameInterval = 0.08f;   // s entre los 2 frames de trepar al bajar
    static constexpr float kFlagpoleFlipDuration = 0.4f;  // s del otro lado del palo antes de bajarse
    static constexpr float kFlagpoleWalkMaxDuration = 4.0f; // s maximos caminando hacia el castillo
    static inline bool sWorldPalettes = false;
};
