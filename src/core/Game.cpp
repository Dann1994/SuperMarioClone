#include "Game.h"

#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../render/DebugFont.h"

namespace {
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr float kMaxFrameTime = 0.25f;  // evita "spiral of death" si hay un frame muy largo

// Punto de aparicion/reaparicion (al morir chico) de cada personaje.
constexpr float kMarioSpawnX = 32.0f;
constexpr float kMarioSpawnY = 64.0f;
constexpr float kLuigiSpawnX = 56.0f;
constexpr float kLuigiSpawnY = 64.0f;

// Nivel de prueba (levels/world1/1-1.json): un item por plataforma, separados
// para poder probar cada uno sin que se pisen entre si.
constexpr float kMushroomTestX = 88.0f;
constexpr float kMushroomTestY = 160.0f;
constexpr float kFireFlowerTestX = 136.0f;
constexpr float kFireFlowerTestY = 128.0f;
constexpr float kStarTestX = 168.0f;
constexpr float kStarTestY = 160.0f;
constexpr float kCoinTestX = 200.0f;
constexpr float kCoinTestY = 128.0f;

// Goomba y Koopas de prueba: el nivel de enemigos tiene paredes en los dos
// bordes (test_enemies.json), asi que pueden arrancar en cualquier punto
// del piso. Dos Koopas separados para poder patear uno contra el otro y
// probar que un caparazon en movimiento mate a otro (checkShellVsKoopa).
constexpr float kGoombaTestX = 216.0f;
constexpr float kGoombaTestY = 190.0f;
constexpr float kKoopaATestX = 64.0f;
constexpr float kKoopaATestY = 190.0f;
constexpr float kKoopaBTestX = 152.0f;
constexpr float kKoopaBTestY = 190.0f;
// Koopa Alado de prueba: en el juego original de 1985 el color fija el
// comportamiento (no es libre como en Koopa::spawn) -- el verde va a los
// saltos, el rojo se hamaca arriba y abajo en un punto fijo. Por eso aca
// solo hay una instancia de cada combinacion, no las cuatro posibles (ver
// el comentario de KoopaState en Koopa.h).
// Saltarin verde: "Y" es solo la altura inicial, cae y arranca a los saltos
// por su cuenta.
constexpr float kKoopaJumpingTestX = 190.0f;
constexpr float kKoopaJumpingTestY = 140.0f;
// Koopa rojo de prueba: arranca sobre la plataforma angosta de
// test_enemies.json (fila 9, columnas 2-5) caminando hacia la derecha, para
// ver que da la vuelta antes de caerse por el borde en vez de tirarse.
constexpr float kKoopaRedTestX = 48.0f;
constexpr float kKoopaRedTestY = 100.0f;
// Volador rojo: "Y" es la altura central del hamacado (ver
// Koopa::kFlyHoverAmplitude), no una posicion en el piso.
constexpr float kKoopaRedFlyingTestX = 130.0f;
constexpr float kKoopaRedFlyingTestY = 170.0f;
// Buzzy Beetle de prueba: mismo piso que el resto, entre koopaA y koopaB.
constexpr float kBuzzyBeetleTestX = 104.0f;
constexpr float kBuzzyBeetleTestY = 190.0f;

// Pirana Plant de prueba: sale del tile angosto agregado a test_enemies.json
// (fila 12-13, columna 12) -- "Y" es el borde de arriba de ese tile (donde
// apoya la planta bien asomada), no una posicion en el piso.
constexpr float kPiranhaPlantTestX = 192.0f;
constexpr float kPiranhaPlantTestPipeTopY = 192.0f;
// Que tan cerca (en X e Y) puede estar un personaje del cano sin que la
// planta se niegue a asomar (ver Game::isSafeForPiranhaPlant).
constexpr float kPiranhaSafeDistanceX = 32.0f;
constexpr float kPiranhaSafeDistanceY = 48.0f;

// Hammer Bro de prueba: arranca sobre la plataforma de bloques de abajo de
// test_hammerbro.json (fila 11, columnas 5-10) para poder ver que le teme a
// los bordes en vez de tirarse, patrullando de punta a punta.
constexpr float kHammerBroTestX = 88.0f;
constexpr float kHammerBroTestY = 140.0f;

// Lakitu de prueba: arranca arriba en el aire de test_lakitu.json (piso
// solido recien en las filas 14-15), lejos de las cabezas de Mario/Luigi
// para poder ver el hamacado y el seguimiento antes de que este cerca. La
// tecla S tira un Spiny directo en ese mismo punto (sin esperar al timer
// del Lakitu) para poder probarlo aislado.
constexpr float kLakituTestX = 120.0f;
constexpr float kLakituTestY = 40.0f;
constexpr float kSpinyTestX = 120.0f;
constexpr float kSpinyTestY = 40.0f;

// Blooper de prueba: arranca en el medio del nivel de agua de
// test_blooper.json, lejos de los bordes para poder ver el hundimiento y
// los chorros antes de que llegue a la pared.
constexpr float kBlooperTestX = 120.0f;
constexpr float kBlooperTestY = 60.0f;

// Cicla chico -> grande -> fuego -> chico. Herramienta de debug (seccion 11)
// para probar los tres estados de Mario antes de tener los power-ups reales
// (hongo, flor de fuego) que los disparan en el juego.
MarioSize nextSize(MarioSize size) {
    switch (size) {
        case MarioSize::Small: return MarioSize::Big;
        case MarioSize::Big: return MarioSize::Fire;
        case MarioSize::Fire: default: return MarioSize::Small;
    }
}

// Constantes de la colision Mario<->Luigi (prototipo, sin base documentada
// en el juego original -- este mismo no tiene multiplayer simultaneo). Se
// ajustan a ojo, como el resto de la fisica del proyecto.
constexpr float kMoveThreshold = 15.0f;       // px/s de velocidad de acercamiento, minimo para contar como choque
constexpr float kMinCollisionSpeed = 45.0f;   // px/s, piso de velocidad de choque usado para el impulso (ver mas abajo)
constexpr float kStompBounce = -180.0f;       // px/s, pisada normal (mas chica que el salto: -260)
constexpr float kStompBounceBoosted = -260.0f; // px/s, pisada con Jump sostenido: igual a un salto completo
constexpr float kKoopaStompBounce = -220.0f;  // px/s, pisar un Koopa rebota un poco mas alto que el resto
// Tolerancia para "los pies ya estaban por encima" al detectar pisotones
// contra Goomba/Koopa (ver checkGoombaCollision/checkKoopaCollision): un
// par de pixeles de margen por las dudas de redondeo, no para que cuente
// como pisoton un toque que en realidad ya arranco de costado.
constexpr float kStompLeewayPx = 2.0f;
constexpr float kStompSidewaysSpeed = 75.0f;  // px/s, empuje lateral para no rebotar en el mismo lugar
constexpr float kStompCooldown = 0.3f;        // s, evita un segundo rebote encadenado

// Peso (masa) relativo para el choque horizontal: chico = referencia (1x),
// grande y fuego (mismo hitbox de 32px) pesan mas.
constexpr float kSmallWeight = 1.0f;
constexpr float kBigWeight = 1.8f;
// Cuanto rebotan al chocar (0 = quedan pegados sin rebote, 1 = elastico
// perfecto). Ver resolveCharacterCollision: la velocidad de cada uno despues
// del choque sale de un impulso de colision real (conservacion de momento
// con restitucion), usando la velocidad y el peso de cada personaje en ese
// instante -- no un valor fijo. Asi, tamano Y velocidad (por ejemplo correr)
// afectan el resultado solos, sin casos especiales para "esta corriendo".
constexpr float kCollisionRestitution = 0.7f;

float weightOf(MarioSize size) {
    return size == MarioSize::Small ? kSmallWeight : kBigWeight;
}

// Para el cartel de tipo de mundo (overlay de debug, junto al FPS): mismo
// orden/criterio que el resto de las paletas por WorldType (F7 lo cicla).
const char* worldTypeName(WorldType type) {
    switch (type) {
        case WorldType::Overworld: return "OVERWORLD";
        case WorldType::Underwater: return "UNDERWATER";
        case WorldType::Underground: return "UNDERGROUND";
        case WorldType::Castle: return "CASTLE";
    }
    return "";
}
}  // namespace

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init fallo: %s", SDL_GetError());
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init fallo: %s", IMG_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        "Super Mario Clone",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kLogicalWidth * 3, kLogicalHeight * 3,
        SDL_WINDOW_RESIZABLE);
    if (!window_) {
        SDL_Log("SDL_CreateWindow fallo: %s", SDL_GetError());
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        SDL_Log("SDL_CreateRenderer fallo: %s", SDL_GetError());
        return false;
    }

    SDL_RenderSetLogicalSize(renderer_, kLogicalWidth, kLogicalHeight);
    SDL_RenderSetIntegerScale(renderer_, SDL_TRUE);

    if (!mario_.loadSprite(renderer_, "assets/sprites/mario_small.png",
                            "assets/sprites/mario_big.png", "assets/sprites/mario_fire.png")) {
        return false;
    }
    if (!luigi_.loadSprite(renderer_, "assets/sprites/luigi_small.png",
                            "assets/sprites/luigi_big.png", "assets/sprites/luigi_fire.png")) {
        return false;
    }
    // Paleta de estrella: por ahora solo la variante "overworld" (falta el
    // concepto de tipo de mundo para elegir entre overworld/castle/
    // underground/underwater).
    if (!mario_.loadStarSprite(renderer_, "assets/sprites/mario_small_star.png",
                                "assets/sprites/mario_big_star.png", "assets/sprites/mario_fire_star.png")) {
        return false;
    }
    if (!luigi_.loadStarSprite(renderer_, "assets/sprites/luigi_small_star.png",
                                "assets/sprites/luigi_big_star.png", "assets/sprites/luigi_fire_star.png")) {
        return false;
    }
    if (!fireballSprite_.loadFromFile(renderer_, "assets/sprites/fireball.png", Fireball::kSize, Fireball::kSize)) {
        return false;
    }
    if (!fireballExplosionSprite_.loadFromFile(renderer_, "assets/sprites/fireball_explosion.png", 16, 16)) {
        return false;
    }
    if (!mushroomSprite_.loadFromFile(renderer_, "assets/sprites/mushroom.png", Mushroom::kSize, Mushroom::kSize)) {
        return false;
    }
    if (!fireFlowerSprite_.loadFromFile(renderer_, "assets/sprites/fire_flower.png", FireFlower::kSize, FireFlower::kSize)) {
        return false;
    }
    if (!starSprite_.loadFromFile(renderer_, "assets/sprites/star_item.png", StarItem::kSize, StarItem::kSize)) {
        return false;
    }
    if (!coinSprite_.loadFromFile(renderer_, "assets/sprites/coin.png", Coin::kSize, Coin::kSize)) {
        return false;
    }
    if (!goombaSprite_.loadFromFile(renderer_, "assets/sprites/goomba.png", Goomba::kSize, Goomba::kSize)) {
        return false;
    }
    if (!koopaSprite_.loadFromFile(renderer_, "assets/sprites/koopa.png", Koopa::kFrameWidth, Koopa::kFrameHeight)) {
        return false;
    }
    if (!koopaRedSprite_.loadFromFile(renderer_, "assets/sprites/koopa_red.png", Koopa::kFrameWidth, Koopa::kFrameHeight)) {
        return false;
    }
    if (!buzzyBeetleSprite_.loadFromFile(renderer_, "assets/sprites/buzzy_beetle.png", BuzzyBeetle::kSize,
                                          BuzzyBeetle::kSize)) {
        return false;
    }
    if (!piranhaPlantSprite_.loadFromFile(renderer_, "assets/sprites/piranha_plant.png", PiranhaPlant::kWidth,
                                           PiranhaPlant::kHeight)) {
        return false;
    }
    if (!hammerBroSprite_.loadFromFile(renderer_, "assets/sprites/hammer_bro.png", HammerBro::kWidth,
                                        HammerBro::kHeight)) {
        return false;
    }
    if (!hammerSprite_.loadFromFile(renderer_, "assets/sprites/hammer.png", Hammer::kSize, Hammer::kSize)) {
        return false;
    }
    if (!lakituSprite_.loadFromFile(renderer_, "assets/sprites/lakitu.png", Lakitu::kWidth, Lakitu::kHeight)) {
        return false;
    }
    if (!spinySprite_.loadFromFile(renderer_, "assets/sprites/spiny.png", Spiny::kSize, Spiny::kSize)) {
        return false;
    }
    if (!brickBlockSprite_.loadFromFile(renderer_, "assets/sprites/brick_block.png", 16, 16)) {
        return false;
    }
    if (!brickDebrisSprite_.loadFromFile(renderer_, "assets/sprites/brick_debris.png", BrickDebris::kSize,
                                          BrickDebris::kSize)) {
        return false;
    }
    if (!blooperSprite_.loadFromFile(renderer_, "assets/sprites/blooper.png", Blooper::kWidth, Blooper::kHeight)) {
        return false;
    }
    // Color fijo para toda la vida de la instancia (ver Koopa::setColor),
    // se llama una sola vez aca en vez de en cada spawn()/reset().
    koopaRed_.setColor(KoopaColor::Red);
    koopaRedFlying_.setColor(KoopaColor::Red);

    // Arranca en el selector (ver renderScenarioSelect): recien se carga un
    // nivel y se ponen entidades en pie cuando se elige un escenario.
    running_ = true;
    return true;
}

void Game::startScenario(TestScenario scenario) {
    scenario_ = scenario;
    const char* path = "levels/world1/test_items.json";
    if (scenario == TestScenario::Enemies) {
        path = "levels/world1/test_enemies.json";
    } else if (scenario == TestScenario::HammerBro) {
        path = "levels/world1/test_hammerbro.json";
    } else if (scenario == TestScenario::LakituSpiny) {
        path = "levels/world1/test_lakitu.json";
    } else if (scenario == TestScenario::Blooper) {
        path = "levels/world1/test_blooper.json";
    }
    level_.loadFromFile(path);

    // Nivel de agua (seccion 5.5): arranca directo en Underwater en vez del
    // Overworld por defecto -- F7 lo sigue pudiendo ciclar manualmente
    // despues, pero no tendria sentido abrir este escenario a pie.
    worldType_ = scenario == TestScenario::Blooper ? WorldType::Underwater : WorldType::Overworld;
    mario_.setWorldType(worldType_);
    luigi_.setWorldType(worldType_);

    mario_.spawn(kMarioSpawnX, kMarioSpawnY);
    luigi_.spawn(kLuigiSpawnX, kLuigiSpawnY);

    // Apaga todo lo de los OTROS escenarios (por si se viene de jugar otro)
    // y pone en pie solo lo que corresponde al elegido.
    mushroom_.collect();
    fireFlower_.collect();
    star_.collect();
    coin_.collect();
    goomba_.reset();
    koopaA_.reset();
    koopaB_.reset();
    koopaJumping_.reset();
    koopaRed_.reset();
    koopaRedFlying_.reset();
    buzzyBeetle_.reset();
    piranhaPlant_.reset();
    hammerBro_.reset();
    for (Hammer& hammer : hammers_) hammer.reset();
    lakitu_.reset();
    for (Spiny& spiny : spinies_) spiny.reset();
    blooper_.reset();

    if (scenario == TestScenario::Items) {
        mushroom_.spawn(kMushroomTestX, kMushroomTestY, true);
        fireFlower_.spawn(kFireFlowerTestX, kFireFlowerTestY);
        star_.spawn(kStarTestX, kStarTestY, true);
        coin_.spawn(kCoinTestX, kCoinTestY);
    } else if (scenario == TestScenario::Enemies) {
        goomba_.spawn(kGoombaTestX, kGoombaTestY, true);
        koopaA_.spawn(kKoopaATestX, kKoopaATestY, true);
        koopaB_.spawn(kKoopaBTestX, kKoopaBTestY, false);
        koopaJumping_.spawn(kKoopaJumpingTestX, kKoopaJumpingTestY, false, KoopaState::Jumping);
        koopaRed_.spawn(kKoopaRedTestX, kKoopaRedTestY, true);
        koopaRedFlying_.spawn(kKoopaRedFlyingTestX, kKoopaRedFlyingTestY, true, KoopaState::Flying);
        buzzyBeetle_.spawn(kBuzzyBeetleTestX, kBuzzyBeetleTestY, true);
        piranhaPlant_.spawn(kPiranhaPlantTestX, kPiranhaPlantTestPipeTopY);
    } else if (scenario == TestScenario::HammerBro) {
        hammerBro_.spawn(kHammerBroTestX, kHammerBroTestY, true);
    } else if (scenario == TestScenario::LakituSpiny) {
        lakitu_.spawn(kLakituTestX, kLakituTestY, true);
    } else {
        blooper_.spawn(kBlooperTestX, kBlooperTestY);
    }

    appState_ = AppState::Playing;
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running_ = false;
        } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                if (appState_ == AppState::Playing) {
                    // Primer ESC: volver al selector, no cerrar el juego de
                    // una (asi no se pierde el progreso por error).
                    appState_ = AppState::ScenarioSelect;
                } else {
                    running_ = false;
                }
                continue;
            }

            if (appState_ == AppState::ScenarioSelect) {
                if (event.key.keysym.scancode == SDL_SCANCODE_1) {
                    startScenario(TestScenario::Items);
                } else if (event.key.keysym.scancode == SDL_SCANCODE_2) {
                    startScenario(TestScenario::Enemies);
                } else if (event.key.keysym.scancode == SDL_SCANCODE_3) {
                    startScenario(TestScenario::HammerBro);
                } else if (event.key.keysym.scancode == SDL_SCANCODE_4) {
                    startScenario(TestScenario::LakituSpiny);
                } else if (event.key.keysym.scancode == SDL_SCANCODE_5) {
                    startScenario(TestScenario::Blooper);
                }
                continue;
            }

            if (event.key.keysym.scancode == SDL_SCANCODE_F3) {
                showDebugOverlay_ = !showDebugOverlay_;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F4 && scenario_ == TestScenario::Items) {
                star_.spawn(kStarTestX, kStarTestY, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F1) {
                mario_.setSize(nextSize(mario_.size()));
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F2) {
                luigi_.setSize(nextSize(luigi_.size()));
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F5) {
                mario_.setInvincible(!mario_.isInvincible());
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F6) {
                luigi_.setInvincible(!luigi_.isInvincible());
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F7) {
                worldType_ = nextWorldType(worldType_);
                mario_.setWorldType(worldType_);
                luigi_.setWorldType(worldType_);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F8) {
                characterCollisionEnabled_ = !characterCollisionEnabled_;
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F9) {
                mario_.takeDamage();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F10) {
                luigi_.takeDamage();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F11 && scenario_ == TestScenario::Items) {
                mushroom_.spawn(kMushroomTestX, kMushroomTestY, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_F12 && scenario_ == TestScenario::Items) {
                fireFlower_.spawn(kFireFlowerTestX, kFireFlowerTestY);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_C && scenario_ == TestScenario::Items) {
                coin_.spawn(kCoinTestX, kCoinTestY);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_G && scenario_ == TestScenario::Enemies) {
                goomba_.spawn(kGoombaTestX, kGoombaTestY, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_K && scenario_ == TestScenario::Enemies) {
                koopaA_.spawn(kKoopaATestX, kKoopaATestY, true);
                koopaB_.spawn(kKoopaBTestX, kKoopaBTestY, false);
                koopaJumping_.spawn(kKoopaJumpingTestX, kKoopaJumpingTestY, false, KoopaState::Jumping);
                koopaRed_.spawn(kKoopaRedTestX, kKoopaRedTestY, true);
                koopaRedFlying_.spawn(kKoopaRedFlyingTestX, kKoopaRedFlyingTestY, true, KoopaState::Flying);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_B && scenario_ == TestScenario::Enemies) {
                buzzyBeetle_.spawn(kBuzzyBeetleTestX, kBuzzyBeetleTestY, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_P && scenario_ == TestScenario::Enemies) {
                piranhaPlant_.spawn(kPiranhaPlantTestX, kPiranhaPlantTestPipeTopY);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_H && scenario_ == TestScenario::HammerBro) {
                hammerBro_.spawn(kHammerBroTestX, kHammerBroTestY, true);
                for (Hammer& hammer : hammers_) hammer.reset();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_L && scenario_ == TestScenario::LakituSpiny) {
                lakitu_.spawn(kLakituTestX, kLakituTestY, true);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_S && scenario_ == TestScenario::LakituSpiny) {
                for (Spiny& spiny : spinies_) {
                    if (spiny.isAlive()) continue;
                    spiny.spawn(kSpinyTestX, kSpinyTestY, 0.0f, 0.0f);
                    break;
                }
            } else if (event.key.keysym.scancode == SDL_SCANCODE_O && scenario_ == TestScenario::Blooper) {
                blooper_.spawn(kBlooperTestX, kBlooperTestY);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_M) {
                twoPlayerMode_ = !twoPlayerMode_;
            }
        }
    }
}

void Game::trySpawnFireball(Mario& shooter, std::array<Fireball, kMaxFireballsPerPlayer>& pool) {
    if (!shooter.consumeThrowRequest()) return;

    for (Fireball& fireball : pool) {
        if (fireball.isAlive()) continue;
        const bool right = shooter.facingRight();
        const float spawnX = right ? shooter.x() + 12.0f : shooter.x() - Fireball::kSize;
        const float spawnY = shooter.y() + shooter.height() * 0.4f;
        fireball.spawn(spawnX, spawnY, right);
        shooter.playThrowAnimation();
        return;
    }
    // Los dos slots estan ocupados: como en el original, no se tira una tercera
    // (y no se reproduce la animacion de tirar, ya que no se tiro nada).
}

void Game::resolveCharacterCollision() {
    if (!characterCollisionEnabled_) return;
    // Ninguno de los dos empuja/pisa/choca al otro mientras alguno esta en
    // su secuencia de muerte (ver el mismo criterio en checkGoombaCollision).
    if (mario_.isInDeathSequence() || luigi_.isInDeathSequence()) return;

    const float ax = mario_.x(), ay = mario_.y();
    const float aw = static_cast<float>(mario_.width()), ah = static_cast<float>(mario_.height());
    const float bx = luigi_.x(), by = luigi_.y();
    const float bw = static_cast<float>(luigi_.width()), bh = static_cast<float>(luigi_.height());

    const bool overlapX = ax < bx + bw && ax + aw > bx;
    const bool overlapY = ay < by + bh && ay + ah > by;
    if (!overlapX || !overlapY) return;

    const float overlapLeft = (bx + bw) - ax;   // A entra por la izquierda de B
    const float overlapRight = (ax + aw) - bx;  // A entra por la derecha de B (B esta a la izq de A)
    const float overlapTop = (by + bh) - ay;    // A entra por arriba de B
    const float overlapBottom = (ay + ah) - by; // A entra por abajo de B

    const float overlapXAmount = std::min(overlapLeft, overlapRight);
    const float overlapYAmount = std::min(overlapTop, overlapBottom);

    if (overlapYAmount < overlapXAmount) {
        // Colision vertical: alguien esta arriba del otro. Solo cuenta como
        // "pisada" si el de arriba esta cayendo (si no, es un roce lateral
        // con distinta altura de pie, no un salto encima).
        const bool marioOnTop = ay < by;
        Mario& top = marioOnTop ? mario_ : luigi_;
        Mario& bottom = marioOnTop ? luigi_ : mario_;
        Input& topInput = marioOnTop ? input1_ : input2_;

        if (top.velocityY() > 20.0f) {
            top.setY(bottom.y() - top.height());
            if (top.canStompBounce()) {
                const bool boosted = topInput.down(Action::Jump);
                top.setVelocityY(boosted ? kStompBounceBoosted : kStompBounce);
                // Empuja hacia el lado que mira para que no quede rebotando
                // en el mismo lugar, y bloquea un segundo rebote encadenado
                // si cae de nuevo antes de separarse del todo.
                top.setVelocityX(top.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
                top.setStompCooldown(kStompCooldown);
            } else {
                top.setVelocityY(0.0f);
            }
        }
        return;
    }

    // Colision horizontal: separar segun peso y, si se estan acercando,
    // rebotar con un impulso de colision real (momento + restitucion).
    const bool marioLeft = ax < bx;
    Mario& left = marioLeft ? mario_ : luigi_;
    Mario& right = marioLeft ? luigi_ : mario_;

    const float leftWeight = weightOf(left.size());
    const float rightWeight = weightOf(right.size());
    const float totalWeight = leftWeight + rightWeight;

    // El mas pesado cede menos terreno al separarse de la superposicion.
    left.nudgeX(-overlapXAmount * (rightWeight / totalWeight));
    right.nudgeX(overlapXAmount * (leftWeight / totalWeight));

    const float leftV = left.velocityX();
    const float rightV = right.velocityX();
    const float closingSpeed = leftV - rightV;  // > 0 si se estan acercando

    if (closingSpeed > kMoveThreshold) {
        // Impulso de colision 1D con restitucion (formula estandar de
        // conservacion de momento). Con la misma velocidad y peso para los
        // dos, ambos rebotan igual; el mas pesado y/o mas rapido desplaza
        // mas al otro y se desvia menos el mismo. La animacion de "patinar"
        // se fuerza explicitamente (no depende de que sigan apretando la
        // direccion, ni de si estan nadando).
        //
        // La velocidad de choque real tiene un piso (kMinCollisionSpeed):
        // sin esto, si uno mantiene la tecla apretado contra el otro sin
        // acelerar mucho, el impulso sale tan chico que quedan "pegados"
        // resbalando de a poco en vez de separarse con un rebote que se note.
        const float effectiveClosingSpeed = std::max(closingSpeed, kMinCollisionSpeed);
        const float impulse = -(1.0f + kCollisionRestitution) * effectiveClosingSpeed / (1.0f / leftWeight + 1.0f / rightWeight);
        left.setVelocityX(leftV + impulse / leftWeight);
        right.setVelocityX(rightV - impulse / rightWeight);
        left.setFacingRight(true);
        right.setFacingRight(false);
        left.playBumpAnimation();
        right.playBumpAnimation();
    }
    // Si no se estan acercando lo suficiente, ya quedaron separados por el
    // nudgeX de arriba, sin rebote.
}

void Game::checkMushroomPickup(Mario& character) {
    // Sin colision mientras esta en su secuencia de muerte (ver el mismo
    // criterio en checkGoombaCollision/checkKoopaCollision): no tiene
    // sentido seguir interactuando con nada hasta que reaparezca.
    if (character.isInDeathSequence()) return;
    if (!mushroom_.isAlive()) return;

    const bool overlap =
        character.x() < mushroom_.x() + Mushroom::kSize && character.x() + character.width() > mushroom_.x() &&
        character.y() < mushroom_.y() + Mushroom::kSize && character.y() + character.height() > mushroom_.y();
    if (!overlap) return;

    if (character.size() == MarioSize::Small) {
        character.startGrow();
    }
    spawnScorePopup(mushroom_.x(), mushroom_.y(), "1000");
    mushroom_.collect();
}

void Game::checkFireFlowerPickup(Mario& character) {
    if (character.isInDeathSequence()) return;
    if (!fireFlower_.isAlive()) return;

    const bool overlap =
        character.x() < fireFlower_.x() + FireFlower::kSize && character.x() + character.width() > fireFlower_.x() &&
        character.y() < fireFlower_.y() + FireFlower::kSize && character.y() + character.height() > fireFlower_.y();
    if (!overlap) return;

    if (character.size() == MarioSize::Small) {
        // En este clon, si la agarra chico se comporta como el hongo (crece
        // a grande nomas, con la misma animacion) en vez de pasar directo a
        // fuego como en el juego original.
        character.startGrow(MarioSize::Big);
    } else if (character.size() == MarioSize::Big) {
        // Misma secuencia de parpadeo, pero sin cambio de altura (grande y
        // fuego miden igual): parpadea entre la paleta normal y la de fuego.
        character.startGrow(MarioSize::Fire);
    }
    spawnScorePopup(fireFlower_.x(), fireFlower_.y(), "1000");
    fireFlower_.collect();
}

void Game::checkStarPickup(Mario& character) {
    if (character.isInDeathSequence()) return;
    if (!star_.isAlive()) return;

    const bool overlap =
        character.x() < star_.x() + StarItem::kSize && character.x() + character.width() > star_.x() &&
        character.y() < star_.y() + StarItem::kSize && character.y() + character.height() > star_.y();
    if (!overlap) return;

    character.setInvincible(true);
    spawnScorePopup(star_.x(), star_.y(), "1000");
    star_.collect();
}

void Game::checkCoinPickup(Mario& character) {
    if (character.isInDeathSequence()) return;
    if (!coin_.isAlive()) return;

    const bool overlap =
        character.x() < coin_.x() + Coin::kSize && character.x() + character.width() > coin_.x() &&
        character.y() < coin_.y() + Coin::kSize && character.y() + character.height() > coin_.y();
    if (!overlap) return;

    // Sin contador de monedas/vidas todavia (seccion 6): por ahora solo se
    // recolecta (sin popup de puntaje, la moneda no lo tiene en el original).
    coin_.collect();
}

void Game::checkGoombaCollision(Mario& character, float prevY) {
    // Sin colision mientras esta en su secuencia de muerte: ya esta cayendo
    // con la pose de "muerto" fija, no tiene sentido que un enemigo lo
    // pisotee/dane/mate de nuevo hasta que reaparezca (relevante en 2P: el
    // otro jugador puede seguir jugando mientras este cae).
    if (character.isInDeathSequence()) return;
    if (!goomba_.isDangerous()) return;

    const bool overlap =
        character.x() < goomba_.x() + Goomba::kSize && character.x() + character.width() > goomba_.x() &&
        character.y() < goomba_.y() + Goomba::kSize && character.y() + character.height() > goomba_.y();
    if (!overlap) return;

    // Con estrella: lo elimina de un toque, desde cualquier lado, sin
    // pisoton (no hay rebote) ni dano -- la invencibilidad ya cubre el "no
    // recibe dano" del lado del personaje. Muerte distinta a un pisoton: sale
    // despedido en parabola hacia el lado contrario de donde vino el
    // personaje, en vez de achatarse (ver Goomba::killFalling).
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= goomba_.x();
        goomba_.killFalling(flyRight);
        return;
    }

    // Pisoton: los pies YA estaban por encima de la cabeza del Goomba antes
    // de moverse este frame (no importa cuanto se haya hundido AHORA -- eso
    // evita confundirlo con un golpe de costado por caer rapido o pegarle
    // en diagonal, que es lo que fallaba comparando solo la posicion actual).
    const bool fromAbove = (prevY + character.height()) <= goomba_.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        goomba_.squish();
        character.setVelocityY(kStompBounce);
        return;
    }

    // Contacto de costado: mismo dano que takeDamage()/startDeath() normal.
    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballGoombaCollision(Fireball& fireball) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!goomba_.isDangerous()) return;

    const bool overlap =
        fireball.x() < goomba_.x() + Goomba::kSize && fireball.x() + Fireball::kSize > goomba_.x() &&
        fireball.y() < goomba_.y() + Goomba::kSize && fireball.y() + Fireball::kSize > goomba_.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= goomba_.x();
    goomba_.killFalling(flyRight);
    fireball.explode();
}

void Game::checkKoopaCollision(Mario& character, Koopa& koopa, float prevY) {
    // Ver el comentario en checkGoombaCollision.
    if (character.isInDeathSequence()) return;
    if (!koopa.isDangerous()) return;
    // Este personaje puntual esta inmune (recien lo piso/pateo, ver
    // Koopa::isImmuneTo) -- el otro jugador y los demas enemigos no se ven
    // afectados por esto, siguen interactuando con el caparazon normal.
    if (koopa.isImmuneTo(&character)) return;

    const bool overlap =
        character.x() < koopa.x() + koopa.width() && character.x() + character.width() > koopa.x() &&
        character.y() < koopa.y() + koopa.height() && character.y() + character.height() > koopa.y();
    if (!overlap) return;

    // Con estrella: lo elimina en cualquier estado, igual que al Goomba.
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= koopa.x();
        koopa.killFalling(flyRight);
        return;
    }

    bool fromAbove;
    if (koopa.state() == KoopaState::ShellMoving) {
        // Regla estricta aparte para el caparazon en movimiento (el que mas
        // problemas daba de clasificar bien, por lo rapido que se mueve):
        // solo cuenta como pisoton si los pies del personaje quedan en la
        // mitad de arriba del hitbox del caparazon -- ahi le pega "a la
        // altura de las piernas" y lo frena; si se hunde mas que eso, le
        // pega "del pecho para arriba" y hace dano. Puramente geometrico,
        // con las hitboxes tal cual estan ahora (sin velocidad ni posicion
        // previa de por medio).
        fromAbove = (character.y() + character.height()) <= (koopa.y() + koopa.height() / 2.0f);
    } else {
        // Walking / ShellIdle: los pies YA estaban por encima del Koopa
        // antes de moverse este frame (ver el comentario en checkGoombaCollision).
        fromAbove = (prevY + character.height()) <= koopa.y() + kStompLeewayPx &&
                    character.velocityY() > 0.0f;
    }

    if (koopa.state() == KoopaState::ShellIdle) {
        if (fromAbove) {
            // Pisar un caparazon quieto tambien lo patea (no solo el toque
            // de costado): sin un "lado" natural para un pisoton, usa hacia
            // donde esta mirando el personaje en ese momento. Sin rebote (a
            // diferencia del resto de los pisotones) -- pero hay que frenar
            // la caida en seco igual (no dejarla en positivo): si no, el
            // personaje sigue "cayendo" sobre el caparazon que el mismo
            // acaba de patear (ya ShellMoving) y al toque siguiente frame
            // deja de contar como pisoton (los pies ya bajaron de mas) y
            // pasa a tratarse como toque de costado contra un ShellMoving,
            // que SI hace dano -- terminaria matando al que lo piso.
            koopa.kick(character.facingRight(), &character);
            character.setVelocityY(0.0f);
            // Empuje lateral (igual que el pisoton Mario<->Luigi, ver
            // resolveCharacterCollision): si el personaje se queda parado en
            // el mismo lugar, la gravedad lo hace caer de nuevo justo ahi y
            // vuelve a pisar el caparazon ya pateado -- que al estar quieto
            // otra vez lo re-patearia sin parar en vez de quedar quieto.
            character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
        } else {
            // De costado: patada, hacia el lado contrario de donde vino el personaje.
            const bool kickRight = character.x() <= koopa.x();
            koopa.kick(kickRight, &character);
        }
        return;
    }

    // Walking o ShellMoving: pisoton lo esconde/frena (con rebote); de
    // costado hace dano como cualquier enemigo.
    if (fromAbove) {
        koopa.stomp(&character);
        character.setVelocityY(kKoopaStompBounce);
        character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
    } else if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballKoopaCollision(Fireball& fireball, Koopa& koopa) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!koopa.isDangerous()) return;

    const bool overlap =
        fireball.x() < koopa.x() + koopa.width() && fireball.x() + Fireball::kSize > koopa.x() &&
        fireball.y() < koopa.y() + koopa.height() && fireball.y() + Fireball::kSize > koopa.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= koopa.x();
    koopa.killFalling(flyRight);
    fireball.explode();
}

void Game::checkShellVsGoomba(Koopa& koopa) {
    if (!koopa.isMovingShell()) return;
    if (!goomba_.isDangerous()) return;

    const bool overlap =
        koopa.x() < goomba_.x() + Goomba::kSize && koopa.x() + koopa.width() > goomba_.x() &&
        koopa.y() < goomba_.y() + Goomba::kSize && koopa.y() + koopa.height() > goomba_.y();
    if (!overlap) return;

    const bool flyRight = goomba_.x() >= koopa.x();
    goomba_.killFalling(flyRight);
    // El caparazon sigue de largo (no se frena al matar a otro enemigo).
}

void Game::checkShellVsKoopa(Koopa& a, Koopa& b) {
    if (!a.isAlive() || !b.isAlive()) return;

    const bool overlap =
        a.x() < b.x() + b.width() && a.x() + a.width() > b.x() &&
        a.y() < b.y() + b.height() && a.y() + a.height() > b.y();
    if (!overlap) return;

    const bool aMoving = a.isMovingShell();
    const bool bMoving = b.isMovingShell();
    if (!aMoving && !bMoving) return;

    if (aMoving && bMoving) {
        // Chocan de frente: mueren los dos, cada uno hacia el lado contrario del otro.
        const bool aFliesRight = a.x() >= b.x();
        a.killFalling(aFliesRight);
        b.killFalling(!aFliesRight);
    } else if (aMoving) {
        const bool bFliesRight = b.x() >= a.x();
        b.killFalling(bFliesRight);
    } else {
        const bool aFliesRight = a.x() >= b.x();
        a.killFalling(aFliesRight);
    }
}

void Game::checkGoombaKoopaBounce(Goomba& goomba, Koopa& koopa) {
    if (!goomba.isDangerous() || !koopa.isDangerous()) return;
    if (koopa.isMovingShell()) return;  // eso lo mata, no lo rebota (ver checkShellVsGoomba)

    const float gx = goomba.x();
    const float gw = static_cast<float>(Goomba::kSize);
    const float kx = koopa.x();
    const float kw = static_cast<float>(koopa.width());
    const bool overlapX = gx < kx + kw && gx + gw > kx;
    const bool overlapY = goomba.y() < koopa.y() + koopa.height() && goomba.y() + Goomba::kSize > koopa.y();
    if (!overlapX || !overlapY) return;

    // Empuje minimo para separarlos del todo (mas un margen chico), asi no
    // se reevaluan como superpuestos el frame siguiente y quedan
    // rebotando en el mismo lugar sin avanzar.
    const float overlapLeft = (kx + kw) - gx;
    const float overlapRight = (gx + gw) - kx;
    const float pushAmount = std::min(overlapLeft, overlapRight) * 0.5f + 0.5f;

    const bool goombaOnLeft = gx < kx;
    goomba.bounceOffEnemy(goombaOnLeft ? -pushAmount : pushAmount);
    koopa.bounceOffEnemy(goombaOnLeft ? pushAmount : -pushAmount);
}

void Game::checkKoopaKoopaBounce(Koopa& a, Koopa& b) {
    if (!a.isDangerous() || !b.isDangerous()) return;
    if (a.isMovingShell() || b.isMovingShell()) return;  // eso los mata, no los rebota (ver checkShellVsKoopa)

    const float ax = a.x();
    const float aw = static_cast<float>(a.width());
    const float bx = b.x();
    const float bw = static_cast<float>(b.width());
    const bool overlapX = ax < bx + bw && ax + aw > bx;
    const bool overlapY = a.y() < b.y() + b.height() && a.y() + a.height() > b.y();
    if (!overlapX || !overlapY) return;

    const float overlapLeft = (bx + bw) - ax;
    const float overlapRight = (ax + aw) - bx;
    const float pushAmount = std::min(overlapLeft, overlapRight) * 0.5f + 0.5f;

    const bool aOnLeft = ax < bx;
    a.bounceOffEnemy(aOnLeft ? -pushAmount : pushAmount);
    b.bounceOffEnemy(aOnLeft ? pushAmount : -pushAmount);
}

void Game::checkBuzzyBeetleCollision(Mario& character, BuzzyBeetle& buzzy, float prevY) {
    // Identico a checkKoopaCollision salvo que no hay caso de fuego (ver el
    // comentario de BuzzyBeetle.h) -- esa funcion ni se llama para este
    // enemigo.
    if (character.isInDeathSequence()) return;
    if (!buzzy.isDangerous()) return;
    if (buzzy.isImmuneTo(&character)) return;

    const bool overlap =
        character.x() < buzzy.x() + buzzy.width() && character.x() + character.width() > buzzy.x() &&
        character.y() < buzzy.y() + buzzy.height() && character.y() + character.height() > buzzy.y();
    if (!overlap) return;

    // Con estrella: lo elimina en cualquier estado, igual que al Koopa.
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= buzzy.x();
        buzzy.killFalling(flyRight);
        return;
    }

    bool fromAbove;
    if (buzzy.state() == BuzzyBeetleState::ShellMoving) {
        // Misma regla estricta que Koopa::ShellMoving, ver ese comentario.
        fromAbove = (character.y() + character.height()) <= (buzzy.y() + buzzy.height() / 2.0f);
    } else {
        fromAbove = (prevY + character.height()) <= buzzy.y() + kStompLeewayPx && character.velocityY() > 0.0f;
    }

    if (buzzy.state() == BuzzyBeetleState::ShellIdle) {
        if (fromAbove) {
            buzzy.kick(character.facingRight(), &character);
            character.setVelocityY(0.0f);
            character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
        } else {
            const bool kickRight = character.x() <= buzzy.x();
            buzzy.kick(kickRight, &character);
        }
        return;
    }

    // Walking o ShellMoving: pisoton lo esconde/frena (con rebote); de
    // costado hace dano como cualquier enemigo.
    if (fromAbove) {
        buzzy.stomp(&character);
        character.setVelocityY(kKoopaStompBounce);
        character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
    } else if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballBuzzyBeetleCollision(Fireball& fireball, BuzzyBeetle& buzzy) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!buzzy.isDangerous()) return;

    const bool overlap =
        fireball.x() < buzzy.x() + buzzy.width() && fireball.x() + Fireball::kSize > buzzy.x() &&
        fireball.y() < buzzy.y() + buzzy.height() && fireball.y() + Fireball::kSize > buzzy.y();
    if (!overlap) return;

    // Inmune (ver BuzzyBeetle.h): la bola explota igual, pero al Buzzy
    // Beetle no le pasa nada -- sigue caminando/en su estado actual.
    fireball.explode();
}

void Game::checkShellVsBuzzyBeetle(Koopa& koopa, BuzzyBeetle& buzzy) {
    if (!koopa.isAlive() || !buzzy.isAlive()) return;

    const bool overlap =
        koopa.x() < buzzy.x() + buzzy.width() && koopa.x() + koopa.width() > buzzy.x() &&
        koopa.y() < buzzy.y() + buzzy.height() && koopa.y() + koopa.height() > buzzy.y();
    if (!overlap) return;

    const bool koopaMoving = koopa.isMovingShell();
    const bool buzzyMoving = buzzy.isMovingShell();
    if (!koopaMoving && !buzzyMoving) return;

    if (koopaMoving && buzzyMoving) {
        const bool koopaFliesRight = koopa.x() >= buzzy.x();
        koopa.killFalling(koopaFliesRight);
        buzzy.killFalling(!koopaFliesRight);
    } else if (koopaMoving) {
        const bool buzzyFliesRight = buzzy.x() >= koopa.x();
        buzzy.killFalling(buzzyFliesRight);
    } else {
        const bool koopaFliesRight = koopa.x() >= buzzy.x();
        koopa.killFalling(koopaFliesRight);
    }
}

void Game::checkBuzzyShellVsGoomba(BuzzyBeetle& buzzy) {
    if (!buzzy.isMovingShell()) return;
    if (!goomba_.isDangerous()) return;

    const bool overlap =
        buzzy.x() < goomba_.x() + Goomba::kSize && buzzy.x() + buzzy.width() > goomba_.x() &&
        buzzy.y() < goomba_.y() + Goomba::kSize && buzzy.y() + buzzy.height() > goomba_.y();
    if (!overlap) return;

    const bool flyRight = goomba_.x() >= buzzy.x();
    goomba_.killFalling(flyRight);
}

void Game::checkGoombaBuzzyBeetleBounce(Goomba& goomba, BuzzyBeetle& buzzy) {
    if (!goomba.isDangerous() || !buzzy.isDangerous()) return;
    if (buzzy.isMovingShell()) return;  // eso lo mata, no lo rebota (ver checkBuzzyShellVsGoomba)

    const float gx = goomba.x();
    const float gw = static_cast<float>(Goomba::kSize);
    const float bx = buzzy.x();
    const float bw = static_cast<float>(buzzy.width());
    const bool overlapX = gx < bx + bw && gx + gw > bx;
    const bool overlapY = goomba.y() < buzzy.y() + buzzy.height() && goomba.y() + Goomba::kSize > buzzy.y();
    if (!overlapX || !overlapY) return;

    const float overlapLeft = (bx + bw) - gx;
    const float overlapRight = (gx + gw) - bx;
    const float pushAmount = std::min(overlapLeft, overlapRight) * 0.5f + 0.5f;

    const bool goombaOnLeft = gx < bx;
    goomba.bounceOffEnemy(goombaOnLeft ? -pushAmount : pushAmount);
    buzzy.bounceOffEnemy(goombaOnLeft ? pushAmount : -pushAmount);
}

void Game::checkKoopaBuzzyBeetleBounce(Koopa& koopa, BuzzyBeetle& buzzy) {
    if (!koopa.isDangerous() || !buzzy.isDangerous()) return;
    if (koopa.isMovingShell() || buzzy.isMovingShell()) return;  // eso los mata, no los rebota (ver checkShellVsBuzzyBeetle)

    const float kx = koopa.x();
    const float kw = static_cast<float>(koopa.width());
    const float bx = buzzy.x();
    const float bw = static_cast<float>(buzzy.width());
    const bool overlapX = kx < bx + bw && kx + kw > bx;
    const bool overlapY = koopa.y() < buzzy.y() + buzzy.height() && koopa.y() + koopa.height() > buzzy.y();
    if (!overlapX || !overlapY) return;

    const float overlapLeft = (bx + bw) - kx;
    const float overlapRight = (kx + kw) - bx;
    const float pushAmount = std::min(overlapLeft, overlapRight) * 0.5f + 0.5f;

    const bool koopaOnLeft = kx < bx;
    koopa.bounceOffEnemy(koopaOnLeft ? -pushAmount : pushAmount);
    buzzy.bounceOffEnemy(koopaOnLeft ? pushAmount : -pushAmount);
}

void Game::checkPiranhaPlantCollision(Mario& character, PiranhaPlant& plant) {
    if (character.isInDeathSequence()) return;
    if (!plant.isDangerous()) return;

    const bool overlap =
        character.x() < plant.x() + plant.width() && character.x() + character.width() > plant.x() &&
        character.y() < plant.y() + plant.height() && character.y() + character.height() > plant.y();
    if (!overlap) return;

    if (character.isInvincible()) {
        plant.kill();
        return;
    }

    // Sin pisoton (ver PiranhaPlant.h): cualquier contacto hace dano, sin
    // importar de que lado venga.
    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballPiranhaPlantCollision(Fireball& fireball, PiranhaPlant& plant) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!plant.isDangerous()) return;

    const bool overlap =
        fireball.x() < plant.x() + plant.width() && fireball.x() + Fireball::kSize > plant.x() &&
        fireball.y() < plant.y() + plant.height() && fireball.y() + Fireball::kSize > plant.y();
    if (!overlap) return;

    plant.kill();
    fireball.explode();
}

bool Game::isSafeForPiranhaPlant(const PiranhaPlant& plant) const {
    const float plantCenterX = plant.x() + static_cast<float>(plant.width()) / 2.0f;
    auto tooClose = [&](const Mario& character) {
        const float characterCenterX = character.x() + static_cast<float>(character.width()) / 2.0f;
        const float dx = std::abs(characterCenterX - plantCenterX);
        const float dy = std::abs(character.y() - plant.y());
        return dx < kPiranhaSafeDistanceX && dy < kPiranhaSafeDistanceY;
    };
    if (tooClose(mario_)) return false;
    if (twoPlayerMode_ && tooClose(luigi_)) return false;
    return true;
}

void Game::checkHammerBroCollision(Mario& character, float prevY) {
    if (character.isInDeathSequence()) return;
    if (!hammerBro_.isDangerous()) return;

    const bool overlap = character.x() < hammerBro_.x() + hammerBro_.width() &&
                          character.x() + character.width() > hammerBro_.x() &&
                          character.y() < hammerBro_.y() + hammerBro_.height() &&
                          character.y() + character.height() > hammerBro_.y();
    if (!overlap) return;

    // Cualquier forma de matarlo usa la misma parabola (ver HammerBro.h):
    // no hay caparazon ni frame de achatado propio en el arte fuente.
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= hammerBro_.x();
        hammerBro_.killFalling(flyRight);
        return;
    }

    const bool fromAbove = (prevY + character.height()) <= hammerBro_.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        const bool flyRight = character.x() <= hammerBro_.x();
        hammerBro_.killFalling(flyRight);
        character.setVelocityY(kStompBounce);
        return;
    }

    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballHammerBroCollision(Fireball& fireball) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!hammerBro_.isDangerous()) return;

    const bool overlap = fireball.x() < hammerBro_.x() + hammerBro_.width() &&
                          fireball.x() + Fireball::kSize > hammerBro_.x() &&
                          fireball.y() < hammerBro_.y() + hammerBro_.height() &&
                          fireball.y() + Fireball::kSize > hammerBro_.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= hammerBro_.x();
    hammerBro_.killFalling(flyRight);
    fireball.explode();
}

void Game::checkHammerCollision(Mario& character, Hammer& hammer) {
    if (character.isInDeathSequence()) return;
    if (!hammer.isAlive()) return;

    const bool overlap =
        character.x() < hammer.x() + hammer.width() && character.x() + character.width() > hammer.x() &&
        character.y() < hammer.y() + hammer.height() && character.y() + character.height() > hammer.y();
    if (!overlap) return;

    // No es un enemigo (no hace falta pisarlo ni matarlo con estrella) y no
    // desaparece al tocar a nadie (ver Hammer.h): sigue de largo rebotando.
    // Con estrella no hace dano (el personaje ya esta protegido, no hace
    // falta nada especial aca -- Mario::takeDamage() ya se encarga).
    if (character.isInvincible()) return;

    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkLakituCollision(Mario& character, float prevY) {
    if (character.isInDeathSequence()) return;
    if (!lakitu_.isDangerous()) return;

    const bool overlap = character.x() < lakitu_.x() + lakitu_.width() &&
                          character.x() + character.width() > lakitu_.x() &&
                          character.y() < lakitu_.y() + lakitu_.height() &&
                          character.y() + character.height() > lakitu_.y();
    if (!overlap) return;

    // Cualquier forma de matarlo usa la misma parabola (ver Lakitu.h): a
    // diferencia de la Pirana Plant, aca el pisoton SI lo mata (no hay
    // razon para que un salto encima no lo tire de la nube).
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= lakitu_.x();
        lakitu_.killFalling(flyRight);
        return;
    }

    const bool fromAbove = (prevY + character.height()) <= lakitu_.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        const bool flyRight = character.x() <= lakitu_.x();
        lakitu_.killFalling(flyRight);
        character.setVelocityY(kStompBounce);
        return;
    }

    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballLakituCollision(Fireball& fireball) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!lakitu_.isDangerous()) return;

    const bool overlap = fireball.x() < lakitu_.x() + lakitu_.width() &&
                          fireball.x() + Fireball::kSize > lakitu_.x() &&
                          fireball.y() < lakitu_.y() + lakitu_.height() &&
                          fireball.y() + Fireball::kSize > lakitu_.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= lakitu_.x();
    lakitu_.killFalling(flyRight);
    fireball.explode();
}

void Game::checkSpinyCollision(Mario& character, Spiny& spiny) {
    if (character.isInDeathSequence()) return;
    if (!spiny.isDangerous()) return;

    const bool overlap =
        character.x() < spiny.x() + Spiny::kSize && character.x() + character.width() > spiny.x() &&
        character.y() < spiny.y() + Spiny::kSize && character.y() + character.height() > spiny.y();
    if (!overlap) return;

    if (character.isInvincible()) {
        const bool flyRight = character.x() <= spiny.x();
        spiny.killFalling(flyRight);
        return;
    }

    // Sin pisoton (ver Spiny.h): cualquier contacto hace dano, sin importar
    // de que lado venga.
    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballSpinyCollision(Fireball& fireball, Spiny& spiny) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!spiny.isDangerous()) return;

    const bool overlap =
        fireball.x() < spiny.x() + Spiny::kSize && fireball.x() + Fireball::kSize > spiny.x() &&
        fireball.y() < spiny.y() + Spiny::kSize && fireball.y() + Fireball::kSize > spiny.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= spiny.x();
    spiny.killFalling(flyRight);
    fireball.explode();
}

void Game::checkSpinyBounce(Spiny& a, Spiny& b) {
    // Solo caminando: un huevo cayendo no tiene sentido que "rebote" contra
    // otro (ver el comentario de checkSpinyBounce en Game.h).
    if (a.state() != SpinyState::Walking || b.state() != SpinyState::Walking) return;
    if (!a.isDangerous() || !b.isDangerous()) return;

    const float ax = a.x();
    const float aw = static_cast<float>(Spiny::kSize);
    const float bx = b.x();
    const float bw = static_cast<float>(Spiny::kSize);
    const bool overlapX = ax < bx + bw && ax + aw > bx;
    const bool overlapY = a.y() < b.y() + Spiny::kSize && a.y() + Spiny::kSize > b.y();
    if (!overlapX || !overlapY) return;

    // Empuje minimo para separarlos del todo (mas un margen chico), asi no
    // se reevaluan como superpuestos el frame siguiente (ver el mismo
    // criterio en checkGoombaKoopaBounce).
    const float overlapLeft = (bx + bw) - ax;
    const float overlapRight = (ax + aw) - bx;
    const float pushAmount = std::min(overlapLeft, overlapRight) * 0.5f + 0.5f;

    const bool aOnLeft = ax < bx;
    a.bounceOffEnemy(aOnLeft ? -pushAmount : pushAmount);
    b.bounceOffEnemy(aOnLeft ? pushAmount : -pushAmount);
}

void Game::checkBlooperCollision(Mario& character, Blooper& blooper) {
    if (character.isInDeathSequence()) return;
    if (!blooper.isDangerous()) return;

    const bool overlap = character.x() < blooper.x() + blooper.width() &&
                          character.x() + character.width() > blooper.x() &&
                          character.y() < blooper.y() + blooper.height() &&
                          character.y() + character.height() > blooper.y();
    if (!overlap) return;

    if (character.isInvincible()) {
        const bool flyRight = character.x() <= blooper.x();
        blooper.killFalling(flyRight);
        return;
    }

    // Sin pisoton (ver Blooper.h): cualquier contacto hace dano, sin
    // importar de que lado venga.
    if (character.size() == MarioSize::Small) {
        character.startDeath();
    } else {
        character.takeDamage();
    }
}

void Game::checkFireballBlooperCollision(Fireball& fireball, Blooper& blooper) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!blooper.isDangerous()) return;

    const bool overlap = fireball.x() < blooper.x() + blooper.width() &&
                          fireball.x() + Fireball::kSize > blooper.x() &&
                          fireball.y() < blooper.y() + blooper.height() &&
                          fireball.y() + Fireball::kSize > blooper.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= blooper.x();
    blooper.killFalling(flyRight);
    fireball.explode();
}

void Game::checkBrickBlockHit(Mario& character) {
    int row = 0, col0 = 0, col1 = 0;
    if (!character.headBumpedThisFrame(row, col0, col1)) return;

    for (int col = col0; col <= col1; ++col) {
        if (!level_.isBrickTile(col, row)) continue;
        // Chico: el golpe ya freno el salto en moveAndCollide(); esto solo
        // agrega la sacudida visual, no rompe el tile.
        if (character.size() == MarioSize::Small) {
            bumpBrickBlock(col, row);
        } else {
            breakBrickBlock(col, row);
        }
        return;
    }
}

void Game::breakBrickBlock(int col, int row) {
    level_.breakTile(col, row);

    const int ts = level_.tileSize();
    const float tileX = static_cast<float>(col * ts);
    const float tileY = static_cast<float>(row * ts);
    const float centerX = tileX + static_cast<float>(ts) / 2.0f - static_cast<float>(BrickDebris::kSize) / 2.0f;
    const float centerY = tileY + static_cast<float>(ts) / 2.0f - static_cast<float>(BrickDebris::kSize) / 2.0f;

    // Cuatro pedazos, uno por esquina: dos mas altos hacia arriba-adentro,
    // dos mas bajos hacia arriba-afuera, como el clasico "estallido" en cruz.
    const float velocities[4][2] = {
        {-70.0f, -260.0f}, {70.0f, -260.0f}, {-40.0f, -180.0f}, {40.0f, -180.0f}};
    int spawned = 0;
    for (BrickDebris& debris : brickDebris_) {
        if (debris.isAlive() || spawned >= 4) continue;
        debris.spawn(centerX, centerY, velocities[spawned][0], velocities[spawned][1]);
        ++spawned;
    }

    killEnemyOnTopOfTile(col, row);
}

void Game::killEnemyOnTopOfTile(int col, int row) {
    // Cualquier golpe a un bloque (se rompa o no) mata al enemigo parado
    // justo encima, igual que en el original -- por eso esto es comun a
    // breakBrickBlock y bumpBrickBlock en vez de estar solo en uno.
    const int ts = level_.tileSize();
    const float tileTop = static_cast<float>(row * ts);
    const float tileLeft = static_cast<float>(col * ts);
    const float tileRight = tileLeft + static_cast<float>(ts);
    auto onTop = [&](float x, float y, int width, int height) {
        return x < tileRight && x + width > tileLeft && std::abs((y + height) - tileTop) < 4.0f;
    };
    if (goomba_.isDangerous() && onTop(goomba_.x(), goomba_.y(), Goomba::kSize, Goomba::kSize)) {
        goomba_.killFalling(true);
    }
    const std::array<Koopa*, 5> koopasOnTop{&koopaA_, &koopaB_, &koopaJumping_, &koopaRed_, &koopaRedFlying_};
    for (Koopa* koopa : koopasOnTop) {
        if (koopa->isDangerous() && onTop(koopa->x(), koopa->y(), koopa->width(), koopa->height())) {
            koopa->killFalling(true);
        }
    }
    if (buzzyBeetle_.isDangerous() &&
        onTop(buzzyBeetle_.x(), buzzyBeetle_.y(), BuzzyBeetle::kSize, BuzzyBeetle::kSize)) {
        buzzyBeetle_.killFalling(true);
    }
    if (hammerBro_.isDangerous() && onTop(hammerBro_.x(), hammerBro_.y(), HammerBro::kWidth, HammerBro::kHeight)) {
        hammerBro_.killFalling(true);
    }
    for (Spiny& spiny : spinies_) {
        if (spiny.isDangerous() && onTop(spiny.x(), spiny.y(), Spiny::kSize, Spiny::kSize)) {
            spiny.killFalling(true);
        }
    }
}

void Game::bumpBrickBlock(int col, int row) {
    killEnemyOnTopOfTile(col, row);

    // Si esta misma celda ya esta sacudiendose, reinicia el timer en vez de
    // agregar una segunda (un solo golpe a la vez tiene sentido visualmente).
    for (BlockBump& bump : blockBumps_) {
        if (bump.active && bump.col == col && bump.row == row) {
            bump.timer = kBumpDuration;
            return;
        }
    }
    for (BlockBump& bump : blockBumps_) {
        if (bump.active) continue;
        bump.active = true;
        bump.col = col;
        bump.row = row;
        bump.timer = kBumpDuration;
        return;
    }
    // Pool lleno (con kMaxBlockBumps=2 y un solo Mario/Luigi a la vez
    // golpeando bloques, no debería pasar): se pierde esta sacudida, no es
    // grave, solo visual.
}

void Game::spawnScorePopup(float x, float y, const std::string& text) {
    for (ScorePopup& popup : scorePopups_) {
        if (popup.isAlive()) continue;
        popup.spawn(x, y, text);
        return;
    }
    // Sin slots libres: se pierde este popup, no es grave (solo visual).
}

SDL_Point Game::findRespawnPoint(float aroundX, int characterHeight) const {
    const int ts = level_.tileSize();
    const int aroundCol = static_cast<int>(aroundX) / ts;
    const int widthTiles = level_.widthTiles();

    for (int radius = 0; radius <= widthTiles; ++radius) {
        for (int dir = 0; dir < 2; ++dir) {
            if (radius == 0 && dir == 1) continue;  // radio 0 es la misma columna en los dos lados
            const int col = dir == 0 ? aroundCol + radius : aroundCol - radius;
            if (col < 0 || col >= widthTiles) continue;
            for (int row = 0; row < level_.heightTiles(); ++row) {
                if (!level_.isSolidTile(col, row)) continue;
                // Primer piso solido de arriba hacia abajo en esa columna:
                // si no hay ninguno, la columna es un pozo y se descarta,
                // se sigue buscando hacia el costado.
                const SDL_Rect spawnBox{col * ts, row * ts - characterHeight, ts, characterHeight};
                return SDL_Point{spawnBox.x, spawnBox.y};
            }
        }
    }
    // No deberia pasar (implicaria un nivel sin piso en ningun lado).
    return SDL_Point{static_cast<int>(kMarioSpawnX), static_cast<int>(kMarioSpawnY)};
}

void Game::respawnNearestPlatform(Mario& character) {
    // Preferentemente en el centro de la pantalla, no en donde murio.
    const SDL_Point point = findRespawnPoint(kLogicalWidth / 2.0f, character.height());
    character.spawn(static_cast<float>(point.x), static_cast<float>(point.y));
    character.grantRespawnInvincibility();
}

void Game::respawnAtCheckpoint(Mario& character, float spawnX, float spawnY) {
    // Placeholder hasta que exista un sistema de checkpoints real (seccion
    // 6): vuelve al spawn original del nivel.
    character.spawn(spawnX, spawnY);
    character.grantRespawnInvincibility();
}

void Game::updateCamera(float dt) {
    // El de mas adelante (mayor X) es quien "empuja" la camara -- en 1P es
    // directamente Mario. No sigue al que se quedo atras: asi no retrocede
    // sola si un personaje se da vuelta, solo importa hasta donde llego el
    // que mas avanzo.
    const float leaderX = twoPlayerMode_ ? std::max(mario_.x(), luigi_.x()) : mario_.x();
    // Centrada en el personaje, pero sin mostrar mas alla de los bordes del
    // nivel -- si el nivel entero entra en una pantalla (los escenarios
    // viejos, sin scroll), maxCameraX da <= 0 y se clampea a 0 siempre.
    const float maxCameraX = std::max(0.0f, static_cast<float>(level_.widthPixels() - kLogicalWidth));
    const float previousCameraX = cameraX_;
    cameraX_ = std::clamp(leaderX - static_cast<float>(kLogicalWidth) / 2.0f, 0.0f, maxCameraX);
    // Ver el comentario de cameraScrollSpeed_ en Game.h (equivalente a
    // ScrollAmount en el original): 0 en vez de un valor real cuando la
    // camara esta pegada a un borde del nivel (clampeada), aunque el
    // personaje se siga moviendo -- ahi no esta "empujando" la camara de
    // verdad aunque corra.
    cameraScrollSpeed_ = dt > 0.0f ? (cameraX_ - previousCameraX) / dt : 0.0f;
}

void Game::update(float dt) {
    if (appState_ != AppState::Playing) return;

    // Congelado (secuencias de danio/crecer, o alguien que "muere del
    // todo"): todo lo demas se pausa. En 2P eso es cuando mueren los dos a
    // la vez; en 1P, Mario es el unico jugador, asi que su muerte sola ya
    // cuenta como "murieron todos" y tambien congela.
    const bool damageOrGrowFrozen = mario_.isInDamageSequence() || mario_.isInGrowSequence() ||
        (twoPlayerMode_ && (luigi_.isInDamageSequence() || luigi_.isInGrowSequence()));
    const bool everyoneDying = twoPlayerMode_
        ? (mario_.isInDeathSequence() && luigi_.isInDeathSequence())
        : mario_.isInDeathSequence();

    if (damageOrGrowFrozen || everyoneDying) {
        mario_.updateDamageSequence(dt);
        mario_.updateGrowSequence(dt);
        mario_.updateDeathSequence(dt);
        if (mario_.consumeDeathFinished()) {
            respawnAtCheckpoint(mario_, kMarioSpawnX, kMarioSpawnY);
        }
        if (twoPlayerMode_) {
            luigi_.updateDamageSequence(dt);
            luigi_.updateGrowSequence(dt);
            luigi_.updateDeathSequence(dt);
            if (luigi_.consumeDeathFinished()) {
                respawnAtCheckpoint(luigi_, kLuigiSpawnX, kLuigiSpawnY);
            }
        }
        return;
    }

    // Guardados de antes de mover: para saber si un enemigo lo pisa "de
    // arriba" hace falta ver si los pies YA estaban por encima antes de
    // este frame, no solo cuanto se superponen despues de moverse (eso se
    // presta a confusion con golpes en diagonal o caidas rapidas que saltan
    // varios pixeles en un solo frame -- ver checkGoombaCollision/checkKoopaCollision).
    const float marioPrevY = mario_.y();
    const float luigiPrevY = luigi_.y();

    mario_.update(dt, input1_, level_);
    checkBrickBlockHit(mario_);
    // Solo se llega aca muriendo en 2P con el otro todavia vivo (en 1P o en
    // "murieron los dos" ya se atendio arriba): reaparece en la plataforma
    // mas cercana en vez del spawn del nivel, sin pausar nada.
    if (mario_.consumeDeathFinished()) {
        respawnNearestPlatform(mario_);
    }

    if (twoPlayerMode_) {
        luigi_.update(dt, input2_, level_);
        checkBrickBlockHit(luigi_);
        if (luigi_.consumeDeathFinished()) {
            respawnNearestPlatform(luigi_);
        }
        resolveCharacterCollision();
    }

    updateCamera(dt);

    mushroom_.update(dt, level_, kLogicalWidth);
    checkMushroomPickup(mario_);
    if (twoPlayerMode_) checkMushroomPickup(luigi_);

    fireFlower_.update(dt);
    checkFireFlowerPickup(mario_);
    if (twoPlayerMode_) checkFireFlowerPickup(luigi_);

    star_.update(dt, level_, kLogicalWidth);
    checkStarPickup(mario_);
    if (twoPlayerMode_) checkStarPickup(luigi_);

    coin_.update(dt);
    checkCoinPickup(mario_);
    if (twoPlayerMode_) checkCoinPickup(luigi_);

    goomba_.update(dt, level_, kLogicalHeight);
    checkGoombaCollision(mario_, marioPrevY);
    if (twoPlayerMode_) checkGoombaCollision(luigi_, luigiPrevY);

    buzzyBeetle_.update(dt, level_, kLogicalHeight);
    checkBuzzyBeetleCollision(mario_, buzzyBeetle_, marioPrevY);
    if (twoPlayerMode_) checkBuzzyBeetleCollision(luigi_, buzzyBeetle_, luigiPrevY);
    checkBuzzyShellVsGoomba(buzzyBeetle_);
    checkGoombaBuzzyBeetleBounce(goomba_, buzzyBeetle_);

    piranhaPlant_.update(dt, isSafeForPiranhaPlant(piranhaPlant_));
    checkPiranhaPlantCollision(mario_, piranhaPlant_);
    if (twoPlayerMode_) checkPiranhaPlantCollision(luigi_, piranhaPlant_);

    hammerBro_.update(dt, level_, kLogicalHeight);
    // Mira al personaje mas cercano (ver el comentario de faceTowards),
    // independiente de hacia donde este caminando.
    if (twoPlayerMode_ && std::abs(luigi_.x() - hammerBro_.x()) < std::abs(mario_.x() - hammerBro_.x())) {
        hammerBro_.faceTowards(luigi_.x());
    } else {
        hammerBro_.faceTowards(mario_.x());
    }
    checkHammerBroCollision(mario_, marioPrevY);
    if (twoPlayerMode_) checkHammerBroCollision(luigi_, luigiPrevY);
    if (hammerBro_.consumeThrowRequest()) {
        for (Hammer& hammer : hammers_) {
            if (hammer.isAlive()) continue;
            hammer.spawn(hammerBro_.throwOriginX(), hammerBro_.throwOriginY(), hammerBro_.throwVelocityX(),
                         hammerBro_.throwVelocityY());
            break;
        }
    }
    for (Hammer& hammer : hammers_) {
        hammer.update(dt, level_, kLogicalWidth, kLogicalHeight);
        checkHammerCollision(mario_, hammer);
        if (twoPlayerMode_) checkHammerCollision(luigi_, hammer);
    }

    // Mismo criterio que HammerBro::faceTowards: sigue/mira al personaje
    // mas cercano, esta clase tampoco conoce a Mario/Luigi.
    const float lakituTargetX = (twoPlayerMode_ && std::abs(luigi_.x() - lakitu_.x()) < std::abs(mario_.x() - lakitu_.x()))
                                     ? luigi_.x()
                                     : mario_.x();
    lakitu_.update(dt, lakituTargetX, cameraScrollSpeed_, level_.widthPixels(), kLogicalHeight);
    checkLakituCollision(mario_, marioPrevY);
    if (twoPlayerMode_) checkLakituCollision(luigi_, luigiPrevY);
    if (lakitu_.consumeThrowRequest()) {
        for (Spiny& spiny : spinies_) {
            if (spiny.isAlive()) continue;
            spiny.spawn(lakitu_.throwOriginX(), lakitu_.throwOriginY(), lakitu_.throwVelocityX(),
                        lakitu_.throwVelocityY());
            break;
        }
    }
    for (Spiny& spiny : spinies_) {
        spiny.update(dt, level_, kLogicalHeight);
        checkSpinyCollision(mario_, spiny);
        if (twoPlayerMode_) checkSpinyCollision(luigi_, spiny);
    }
    for (size_t i = 0; i < spinies_.size(); ++i) {
        for (size_t j = i + 1; j < spinies_.size(); ++j) {
            checkSpinyBounce(spinies_[i], spinies_[j]);
        }
    }

    // Mismo criterio que Lakitu (targetX): apunta al personaje mas cercano,
    // esta clase tampoco conoce a Mario/Luigi. Ademas necesita Y (el chorro
    // apunta en las dos direcciones, no solo en X).
    {
        const bool luigiCloser = twoPlayerMode_ &&
            std::hypot(luigi_.x() - blooper_.x(), luigi_.y() - blooper_.y()) <
                std::hypot(mario_.x() - blooper_.x(), mario_.y() - blooper_.y());
        const float blooperTargetX = luigiCloser ? luigi_.x() : mario_.x();
        const float blooperTargetY = luigiCloser ? luigi_.y() : mario_.y();
        blooper_.update(dt, level_, blooperTargetX, blooperTargetY, kLogicalHeight);
    }
    checkBlooperCollision(mario_, blooper_);
    if (twoPlayerMode_) checkBlooperCollision(luigi_, blooper_);

    // Los 5 Koopa de prueba (verde piso x2 + verde Alado saltarin + rojo
    // piso + rojo Alado volador -- ver KoopaState/KoopaColor, solo estas dos
    // combinaciones de Alado existen en el juego original de 1985): en
    // array solo para no tener que escribir a mano todas las combinaciones
    // de cada chequeo de a pares (caparazon-vs-caparazon, rebote-vs-Goomba)
    // cada vez que se agrega una instancia de prueba mas.
    const std::array<Koopa*, 5> koopas{&koopaA_, &koopaB_, &koopaJumping_, &koopaRed_, &koopaRedFlying_};
    for (Koopa* koopa : koopas) {
        koopa->update(dt, level_, kLogicalHeight);
        checkKoopaCollision(mario_, *koopa, marioPrevY);
        if (twoPlayerMode_) checkKoopaCollision(luigi_, *koopa, luigiPrevY);
        checkShellVsGoomba(*koopa);
        checkGoombaKoopaBounce(goomba_, *koopa);
        checkShellVsBuzzyBeetle(*koopa, buzzyBeetle_);
        checkKoopaBuzzyBeetleBounce(*koopa, buzzyBeetle_);
    }
    for (size_t i = 0; i < koopas.size(); ++i) {
        for (size_t j = i + 1; j < koopas.size(); ++j) {
            checkShellVsKoopa(*koopas[i], *koopas[j]);
            checkKoopaKoopaBounce(*koopas[i], *koopas[j]);
        }
    }

    for (ScorePopup& popup : scorePopups_) popup.update(dt);
    for (BrickDebris& debris : brickDebris_) debris.update(dt, kLogicalWidth, kLogicalHeight);
    for (BlockBump& bump : blockBumps_) {
        if (!bump.active) continue;
        bump.timer -= dt;
        if (bump.timer <= 0.0f) bump.active = false;
    }

    trySpawnFireball(mario_, marioFireballs_);
    for (Fireball& fireball : marioFireballs_) {
        fireball.update(dt, level_, kLogicalWidth);
        checkFireballGoombaCollision(fireball);
        for (Koopa* koopa : koopas) checkFireballKoopaCollision(fireball, *koopa);
        checkFireballBuzzyBeetleCollision(fireball, buzzyBeetle_);
        checkFireballPiranhaPlantCollision(fireball, piranhaPlant_);
        checkFireballHammerBroCollision(fireball);
        checkFireballLakituCollision(fireball);
        for (Spiny& spiny : spinies_) checkFireballSpinyCollision(fireball, spiny);
        checkFireballBlooperCollision(fireball, blooper_);
    }

    if (twoPlayerMode_) {
        trySpawnFireball(luigi_, luigiFireballs_);
        for (Fireball& fireball : luigiFireballs_) {
            fireball.update(dt, level_, kLogicalWidth);
            checkFireballGoombaCollision(fireball);
            for (Koopa* koopa : koopas) checkFireballKoopaCollision(fireball, *koopa);
            checkFireballBuzzyBeetleCollision(fireball, buzzyBeetle_);
            checkFireballPiranhaPlantCollision(fireball, piranhaPlant_);
            checkFireballHammerBroCollision(fireball);
            checkFireballLakituCollision(fireball);
            for (Spiny& spiny : spinies_) checkFireballSpinyCollision(fireball, spiny);
            checkFireballBlooperCollision(fireball, blooper_);
        }
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    if (appState_ == AppState::ScenarioSelect) {
        renderScenarioSelect();
        SDL_RenderPresent(renderer_);
        return;
    }

    const int worldRow = static_cast<int>(worldType_);
    std::vector<SDL_Point> bumpingTiles;
    for (const BlockBump& bump : blockBumps_) {
        if (bump.active) bumpingTiles.push_back(SDL_Point{bump.col, bump.row});
    }
    level_.render(renderer_, backgroundColorFor(worldType_), brickBlockSprite_, worldRow, cameraX_, bumpingTiles);
    const int camOffset = static_cast<int>(std::lround(cameraX_));
    for (const BlockBump& bump : blockBumps_) {
        if (!bump.active) continue;
        // Sube y vuelve a bajar en el mismo lapso (seno completo entre 0 y
        // pi): 0 al arrancar y al terminar, maximo a la mitad.
        const float progress = 1.0f - bump.timer / kBumpDuration;
        const int offsetY = -static_cast<int>(std::lround(std::sin(progress * 3.14159265f) * kBumpHeight));
        brickBlockSprite_.draw(renderer_, worldRow, 0, bump.col * level_.tileSize() - camOffset,
                                bump.row * level_.tileSize() + offsetY, false);
    }
    mushroom_.render(renderer_, mushroomSprite_, worldRow, cameraX_);
    fireFlower_.render(renderer_, fireFlowerSprite_, worldRow, cameraX_);
    star_.render(renderer_, starSprite_, worldRow, cameraX_);
    coin_.render(renderer_, coinSprite_, worldRow, cameraX_);
    goomba_.render(renderer_, goombaSprite_, worldRow, cameraX_);
    koopaA_.render(renderer_, koopaSprite_, worldRow, cameraX_);
    koopaB_.render(renderer_, koopaSprite_, worldRow, cameraX_);
    koopaJumping_.render(renderer_, koopaSprite_, worldRow, cameraX_);
    koopaRed_.render(renderer_, koopaRedSprite_, worldRow, cameraX_);
    koopaRedFlying_.render(renderer_, koopaRedSprite_, worldRow, cameraX_);
    buzzyBeetle_.render(renderer_, buzzyBeetleSprite_, worldRow, cameraX_);
    piranhaPlant_.render(renderer_, piranhaPlantSprite_, worldRow, cameraX_);
    hammerBro_.render(renderer_, hammerBroSprite_, worldRow, cameraX_);
    if (hammerBro_.isWindingUp()) {
        // Amague antes de tirar (ver HammerBro::isWindingUp): el martillo
        // quieto sobre la cabeza un instante, reusando el sprite del
        // proyectil (frame 0, cualquiera sirve para la pose quieta) en vez
        // de necesitar un frame propio de HammerBro para esto.
        const int drawX = static_cast<int>(std::lround(hammerBro_.throwOriginX())) - Hammer::kSize / 2 - camOffset;
        const int drawY = static_cast<int>(std::lround(hammerBro_.y())) - Hammer::kSize + 4;
        hammerSprite_.draw(renderer_, worldRow, 0, drawX, drawY, false);
    }
    lakitu_.render(renderer_, lakituSprite_, worldRow, cameraX_);
    // Una sola paleta (ver el comentario de Spiny.h): fila 0 siempre, no worldRow.
    for (const Spiny& spiny : spinies_) spiny.render(renderer_, spinySprite_, 0, cameraX_);
    blooper_.render(renderer_, blooperSprite_, worldRow, cameraX_);
    mario_.render(renderer_, cameraX_);
    for (const Fireball& fireball : marioFireballs_) fireball.render(renderer_, fireballSprite_, fireballExplosionSprite_, cameraX_);
    if (twoPlayerMode_) {
        luigi_.render(renderer_, cameraX_);
        for (const Fireball& fireball : luigiFireballs_) fireball.render(renderer_, fireballSprite_, fireballExplosionSprite_, cameraX_);
    }
    // Los martillos en vuelo van al final a proposito, adelante de todo
    // (personajes incluidos): que nunca queden tapados por nada mientras
    // vuelan.
    for (const Hammer& hammer : hammers_) hammer.render(renderer_, hammerSprite_, worldRow, cameraX_);
    for (const BrickDebris& debris : brickDebris_) debris.render(renderer_, brickDebrisSprite_, worldRow, cameraX_);

    for (const ScorePopup& popup : scorePopups_) popup.render(renderer_, cameraX_);

    if (showDebugOverlay_) {
        char fpsText[32];
        std::snprintf(fpsText, sizeof(fpsText), "FPS:%3d", currentFps_);
        DebugFont::drawText(renderer_, 4, 4, fpsText, 2, SDL_Color{255, 255, 255, 255});
        // Cartel de tipo de mundo (F7 lo cicla), para guiarse mientras se
        // prueban las paletas por WorldType de cada item/enemigo.
        DebugFont::drawText(renderer_, 4, 16, worldTypeName(worldType_), 1, SDL_Color{255, 255, 255, 255});
    }

    SDL_RenderPresent(renderer_);
}

void Game::renderScenarioSelect() {
    // Fondo azul overworld fijo, no depende de worldType_ (todavia no se
    // eligio nivel). Fuente bitmap propia (DebugFont) -- placeholder hasta
    // tener fuentes reales para menus (seccion 10).
    SDL_SetRenderDrawColor(renderer_, 92, 148, 252, 255);
    SDL_Rect background{0, 0, kLogicalWidth, kLogicalHeight};
    SDL_RenderFillRect(renderer_, &background);

    const SDL_Color white{255, 255, 255, 255};
    const SDL_Color yellow{252, 216, 88, 255};
    DebugFont::drawText(renderer_, 84, 24, "TEST", 3, white);

    DebugFont::drawText(renderer_, 40, 90, "1 ITEMS", 2, yellow);
    DebugFont::drawText(renderer_, 40, 115, "2 ENEMIGOS", 2, yellow);
    DebugFont::drawText(renderer_, 40, 140, "3 HAMMER BRO", 2, yellow);
    DebugFont::drawText(renderer_, 40, 165, "4 LAKITU", 2, yellow);
    DebugFont::drawText(renderer_, 40, 190, "5 BLOOPER", 2, yellow);

    DebugFont::drawText(renderer_, 8, 215, "PRESIONA 1, 2, 3, 4 O 5", 1, white);
}

void Game::run() {
    Uint64 prevCounter = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    double accumulator = 0.0;

    Uint32 fpsWindowStart = SDL_GetTicks();
    int fpsFrameCount = 0;

    while (running_) {
        const Uint64 now = SDL_GetPerformanceCounter();
        float frameTime = static_cast<float>((now - prevCounter) / frequency);
        prevCounter = now;
        if (frameTime > kMaxFrameTime) {
            frameTime = kMaxFrameTime;
        }
        accumulator += frameTime;

        input1_.beginFrame();
        input2_.beginFrame();
        processEvents();

        while (accumulator >= kFixedDt) {
            update(kFixedDt);
            accumulator -= kFixedDt;
        }

        render();

        ++fpsFrameCount;
        const Uint32 nowTicks = SDL_GetTicks();
        if (nowTicks - fpsWindowStart >= 500) {
            currentFps_ = static_cast<int>(fpsFrameCount * 1000.0 / (nowTicks - fpsWindowStart));
            fpsFrameCount = 0;
            fpsWindowStart = nowTicks;
        }
    }
}

void Game::shutdown() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    SDL_Quit();
}
