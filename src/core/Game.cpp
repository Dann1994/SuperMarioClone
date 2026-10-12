#include "Game.h"

#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "../render/DebugFont.h"

namespace {
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr float kMaxFrameTime = 0.25f;  // evita "spiral of death" si hay un frame muy largo

// Punto de aparicion/reaparicion (al morir chico) de cada personaje.
constexpr float kMarioSpawnX = 32.0f;
constexpr float kMarioSpawnY = 64.0f;
constexpr float kLuigiSpawnX = 56.0f;
constexpr float kLuigiSpawnY = 64.0f;

// Posiciones de prueba para soltar cada item SUELTO (teclas F11/F12/F4/C,
// ver el comentario de mushroom_ en Game.h) sin pasar por su bloque --
// mismos valores de siempre, separados para poder probar cada uno sin que
// se pisen entre si.
constexpr float kMushroomTestX = 88.0f;
constexpr float kMushroomTestY = 160.0f;
constexpr float kFireFlowerTestX = 136.0f;
constexpr float kFireFlowerTestY = 128.0f;
constexpr float kStarTestX = 168.0f;
constexpr float kStarTestY = 160.0f;
// La moneda normal (Coin.h) NO sale de ningun bloque (ver el comentario de
// spawnItemFromQuestionBlock): esta es donde queda puesta, quieta en el
// piso del escenario de Items -- lejos de los bloques para no confundirla
// con la moneda que salta de uno (BlockCoin.h). Tecla C la repone aca mismo.
constexpr float kCoinTestX = 224.0f;
constexpr float kCoinTestY = 208.0f;  // 224 (piso) - 16 (Coin::kSize)

constexpr float kQuestionBlockAnimFrameDuration = 0.2f;  // s por frame del brillo (3 frames)
constexpr float kBrickCoinAnimFrameDuration = 0.12f;  // s por frame del giro, igual que Coin::kFrameDuration

// Que tan lejos atras del borde izquierdo de la camara tiene que quedar un
// enemigo/item antes de eliminarlo del todo -- pedido del usuario: en un
// nivel con scroll, si el jugador sigue de largo y no vuelve, un enemigo
// (ej. un Koopa rebotando entre dos paredes) se quedaria corriendo/rebotando
// para siempre en el fondo, fuera de la vista, sin ningun motivo. Se
// investigo como lo maneja el original: ahi tambien se elimina en base a la
// posicion de la camara (no de un tramo fijo del nivel, que era el bug que
// se arreglo antes), por limitaciones de memoria/sprites de la NES -- ese
// motivo no aplica aca, asi que se usa un margen generoso (una pantalla
// completa) en vez del margen chico y agresivo del original, total no hay
// urgencia real de liberar memoria. Nunca se aplica hacia ADELANTE (solo
// "atras", osea x_ menor a la camara): lo que todavia no se alcanzo tiene
// que seguir estando ahi cuando la camara llegue.
constexpr float kOffscreenBehindMargin = static_cast<float>(kWideLogicalWidth);

// Velocidad maxima (px/s) con la que la camara de 2P se acerca a su
// objetivo (ver Game::updateCamera) -- mas del doble de Mario::kMaxRunSpeed.
constexpr float kCameraCatchUpSpeed = 360.0f;

// Segundos que queda el cartel de "nivel completo" despues de que el
// primero entra al castillo, antes de volver al selector.
constexpr float kLevelClearDuration = 2.5f;

// Timer de nivel (seccion 6), con los valores del original: cada unidad del
// contador dura 24 cuadros (0.4s), y al llegar a 100 suena el aviso de
// apuro. Al terminar el nivel lo que sobra se cuenta a 50 puntos por unidad,
// rapido (kTallyUnitsPerSecond) y con un "bip" cada tanto.
constexpr float kTimeUnitSeconds = 0.4f;
constexpr float kHurryUpThreshold = 100.0f;
constexpr int kTallyPointsPerUnit = 50;
constexpr float kTallyUnitsPerSecond = 120.0f;
constexpr float kTallySoundInterval = 0.06f;

// Vidas (seccion 6): 3 al empezar, como el original. Duracion de la
// pantalla de intro antes de cada intento y del cartel de GAME OVER.
constexpr int kStartingLives = 3;
constexpr float kLevelIntroDuration = 2.0f;
constexpr float kGameOverDuration = 4.0f;
// Lo que dura el jingle de "entrar a la tuberia" de la intro (into_pipe.ogg).
constexpr float kIntroPipeDuration = 3.6f;
// Animacion de tuberia: cuanto tarda en hundirse/entrar y en salir, y
// cuanto puede estar corrido del centro del cano para poder bajar.
constexpr float kPipeEnterDuration = 0.8f;
constexpr float kPipeExitDuration = 0.8f;
constexpr float kPipeCenterTolerance = 6.0f;

// Castillo: cada cuanto se cae un tramo del puente y cuanto se espera
// despues de que Bowser cae a la lava antes de cerrar el nivel.
constexpr float kBridgeCollapseInterval = 0.07f;
constexpr float kCastleClearDelay = 2.5f;
// Canones de Bill Bala: cada cuanto disparan y a que distancia del
// personaje dejan de hacerlo.
constexpr float kCannonIntervalMin = 2.5f;
constexpr float kCannonIntervalMax = 4.5f;
constexpr float kCannonSafeDistance = 40.0f;

// Piezas de escenografia (assets/sprites/scenery_<type>.png, ver
// tools/asset_scripts/gen_scenery.py), mismo orden que
// Game::decorationSprites_.
struct DecorationInfo {
    const char* type;
    int width;
    int height;
};
constexpr DecorationInfo kDecorations[] = {
    {"castle_small", 80, 80}, {"hill_big", 80, 48},   {"hill_small", 48, 32}, {"bush_big", 80, 16},
    {"bush_small", 48, 16},   {"cloud_big", 64, 32},  {"cloud_small", 48, 32}, {"pipe_side", 64, 64},
};

WorldType parseWorldType(const std::string& name, WorldType fallback) {
    if (name == "overworld") return WorldType::Overworld;
    if (name == "underground") return WorldType::Underground;
    if (name == "underwater") return WorldType::Underwater;
    if (name == "castle") return WorldType::Castle;
    return fallback;
}

// reset()/collect() hacen lo mismo (alive_ = false) pero se llaman distinto
// segun la clase (ver el comentario de cada una) -- de ahi las dos
// variantes en vez de una sola con duck typing por nombre de metodo.
template <typename T>
void cullIfLeftBehindReset(T& entity, float width, float cameraX) {
    if (entity.isAlive() && entity.x() + width < cameraX - kOffscreenBehindMargin) {
        entity.reset();
    }
}

template <typename T>
void cullIfLeftBehindCollect(T& entity, float width, float cameraX) {
    if (entity.isAlive() && entity.x() + width < cameraX - kOffscreenBehindMargin) {
        entity.collect();
    }
}

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

// Cheep Cheep de prueba: la Swimming arranca cerca del borde izquierdo a
// media altura para poder ver el cruce completo antes de que salga por el
// derecho (ver Game.cpp -- tecla N la repone si sale). La Jumping arranca
// cerca del piso de test_cheepcheep.json para caer y empezar a saltar
// enseguida (tecla J la repone).
// Lejos del spawn de Mario (kMarioSpawnX/Y = 32/64) para que no queden
// pegadas una arriba de la otra apenas arranca el escenario. Arrancan del
// lado derecho nadando hacia la izquierda (a pedido del usuario).
constexpr float kCheepCheepSwimTestX = 216.0f;
constexpr float kCheepCheepSwimTestY = 40.0f;
// La Oscillating arranca mas abajo que la Straight, separadas para poder
// ver las dos a la vez sin superponerse.
constexpr float kCheepCheepSwimOscTestY = 140.0f;

// Cheep Cheep Jumping de prueba (escenario propio, largo con scroll -- ver
// TestScenario::CheepCheepJump): aparecen desde abajo del borde inferior de
// pantalla (kCheepCheepJumpSpawnY, unos px por debajo de kLogicalHeight para
// que ni el primer frame se alcance a ver el "pop" de aparicion), un poco
// por delante del personaje mas adelantado (kCheepCheepJumpAheadOffsetX) en
// vez de en un punto fijo del nivel, para que el patron se vea sin importar
// que tan rapido camine. Intervalo entre apariciones al azar (mismo
// criterio que HammerBro/Lakitu, ver randomRange), mientras haya lugar en
// el pool (cheepCheepJumps_).
constexpr float kCheepCheepJumpSpawnY = static_cast<float>(kWorldViewBottom) + 8.0f;
// Distancia al personaje al azar (ver Game::spawnNextCheepCheepJump), no un
// punto fijo, para que no se note el patron.
constexpr float kCheepCheepJumpAheadOffsetMin = 40.0f;
constexpr float kCheepCheepJumpAheadOffsetMax = 90.0f;
constexpr float kCheepCheepJumpIntervalMin = 1.0f;
constexpr float kCheepCheepJumpIntervalMax = 2.0f;

// Bill Bala de prueba: dos instancias disparadas en direcciones opuestas
// desde los dos bordes de test_bulletbill.json, a la misma altura, para
// poder ver el cruce (ver BulletBill.h -- sin rebote ni colision, se
// atraviesan y siguen de largo, como el original).
constexpr float kBulletBillATestX = 16.0f;
constexpr float kBulletBillBTestX = 224.0f;
constexpr float kBulletBillTestY = 120.0f;

// Podoboo de prueba: columna fija en el medio de test_podoboo.json,
// baseY es el borde de arriba del piso solido (filas 14-15) -- se hunde ahi
// entre saltos (ver Podoboo.h).
constexpr float kPodobooTestX = 120.0f;
constexpr float kPodobooTestY = 224.0f;

// Bowser y el hacha de prueba: test_bowser.json es de una sola pantalla
// (16 columnas, 256px, sin scroll -- a pedido del usuario, un nivel largo
// dejaba a Bowser fuera de camara la mayor parte del tiempo mientras el
// personaje seguia lejos, cerca de su spawn). Bowser patrulla centrado
// (Bowser::kPatrolRange = 48, spans 92-188) dejando el hacha bien a la
// derecha, del otro lado de la patrulla -- para tocarla hay que pasarlo,
// como en el original ("run under or jump over Bowser"). Y = borde de
// arriba del piso solido menos la altura de cada uno.
constexpr float kBowserTestX = 140.0f;
constexpr float kBowserTestY = 128.0f;  // 160 (tope del puente, fila 10) - 32 (Bowser::kHeight)
constexpr float kAxeTestX = 208.0f;  // columna 13, la plataforma de la derecha del puente
constexpr float kAxeTestY = 144.0f;  // 160 (tope de la plataforma) - 16 (Axe::kHeight)

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

// Timer de aparicion de los Cheep Cheep Jumping (ver kCheepCheepJumpIntervalMin/Max).
float randomRange(float minVal, float maxVal) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
    return minVal + t * (maxVal - minVal);
}

// Constantes de la colision Mario<->Luigi (prototipo, sin base documentada
// en el juego original -- este mismo no tiene multiplayer simultaneo). Se
// ajustan a ojo, como el resto de la fisica del proyecto.
constexpr float kMoveThreshold = 15.0f;       // px/s de velocidad de acercamiento, minimo para contar como choque
constexpr float kMinCollisionSpeed = 45.0f;   // px/s, piso de velocidad de choque usado para el impulso (ver mas abajo)
constexpr float kStompBounce = -180.0f;       // px/s, pisada normal (mas chica que el salto: -260)
constexpr float kStompBounceBoosted = -260.0f; // px/s, pisada con Jump sostenido: igual a un salto completo
constexpr float kKoopaStompBounce = -220.0f;  // px/s, pisar un Koopa rebota un poco mas alto que el resto
// s de invulnerabilidad post-respawn bajo el agua (ver
// Game::grantRespawnInvincibilityForWorld) -- el doble de la normal
// (Mario::kHurtInvincibleDuration = 2s), Mario nada mucho mas lento que
// corriendo en tierra y necesita mas tiempo para alejarse de un enemigo.
constexpr float kUnderwaterRespawnInvincibleDuration = 4.0f;
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

// Para el cartel de modo de ataque de Bowser (overlay de debug, solo en su
// escenario, tecla T lo cicla -- ver BowserAttackMode en Bowser.h).
const char* bowserAttackModeName(BowserAttackMode mode) {
    switch (mode) {
        case BowserAttackMode::FireOnly: return "FUEGO";
        case BowserAttackMode::HammerOnly: return "MARTILLO";
        case BowserAttackMode::Both: return "FUEGO+MARTILLO";
    }
    return "";
}
}  // namespace

bool Game::init() {
    // Que Windows no estire la imagen por su cuenta en pantallas con
    // escalado (125%, 150%...): asi la pantalla completa usa la resolucion
    // real y se ve nitido. Tiene que ir antes de SDL_Init.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init fallo: %s", SDL_GetError());
        return false;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init fallo: %s", IMG_GetError());
        return false;
    }

    // Efectos de sonido (seccion 8.2): si el audio no abre (ej. sin
    // dispositivo de sonido), el juego sigue andando mudo en vez de
    // frenarse -- no hay motivo para que sea fatal.
    if (audio_.init()) {
        // Mapeo evento->archivo (carpeta original del usuario: "NES - Super
        // Mario Bros. - Miscellaneous - Sound Effects"). El original reusa
        // el mismo .wav para dos eventos que nunca suenan a la vez (mismo
        // canal); se respeta esa misma asociacion aca en vez de un archivo
        // por evento:
        //  - pipepowerdown.wav: entrar/salir de una tuberia Y Mario
        //    encogiendo al recibir danio (PowerDown/Pipe).
        //  - stompswim.wav: pisar/matar un enemigo saltando (achatado, no
        //    volteado -- eso es Kick) Y cada brazada nadando bajo el agua.
        //  - kickkill.wav: patear un caparazon y que un caparazon en
        //    movimiento mate a otro enemigo (Kick).
        // fire.wav se uso para el chorro de Bowser (BowserFire), no para la
        // bola de fuego de Mario (esa es fireball.wav) -- son dos sonidos
        // bien distintos en el archivo, se investigo por nombre cual
        // corresponde a cada uno.
        const std::string sfxDir = "assets/sound/sfx/";
        audio_.load(Sfx::Jump, sfxDir + "jump.wav");
        audio_.load(Sfx::JumpSmall, sfxDir + "jumpsmall.wav");
        audio_.load(Sfx::Coin, sfxDir + "coin.wav");
        audio_.load(Sfx::Bump, sfxDir + "bump.wav");
        audio_.load(Sfx::Brick, sfxDir + "brick.wav");
        audio_.load(Sfx::Item, sfxDir + "item.wav");
        audio_.load(Sfx::PowerUp, sfxDir + "powerup.wav");
        audio_.load(Sfx::PowerDown, sfxDir + "pipepowerdown.wav");
        audio_.load(Sfx::Pipe, sfxDir + "pipepowerdown.wav");
        audio_.load(Sfx::StompKill, sfxDir + "stompswim.wav");
        audio_.load(Sfx::Kick, sfxDir + "kickkill.wav");
        audio_.load(Sfx::Death, sfxDir + "death.wav");
        audio_.load(Sfx::OneUp, sfxDir + "1up.wav");
        audio_.load(Sfx::BowserFall, sfxDir + "bowserfall.wav");
        audio_.load(Sfx::Fireball, sfxDir + "fireball.wav");
        audio_.load(Sfx::BowserFire, sfxDir + "fire.wav");
        audio_.load(Sfx::BillFirework, sfxDir + "billfirework.wav");
        audio_.load(Sfx::Flagpole, sfxDir + "flagpole.wav");
        audio_.load(Sfx::HurryUp, sfxDir + "hurryup.wav");
        audio_.load(Sfx::TimeTally, sfxDir + "beep.wav");
        audio_.load(Sfx::GameOver, sfxDir + "gameover.wav");
        audio_.load(Sfx::Pause, sfxDir + "pause.wav");
        loadMusic();
    }

    // Configuracion guardada (seccion 10.2): la proporcion define el ancho
    // logico, y tiene que estar antes de crear la ventana.
    settings_.load();
    gLogicalWidth = settings_.widescreen ? kWideLogicalWidth : kClassicWidth;
    setEnglish(settings_.english);

    // Ventana inicial: el multiplo entero mas grande de la resolucion
    // logica que entre en ~85% del escritorio (con DPI real, un x3 fijo
    // quedaba chico en pantallas grandes y gigante en las chicas), o el que
    // eligio el jugador.
    int windowScale = 3;
    SDL_Rect usable;
    if (settings_.windowScale > 0) {
        windowScale = settings_.windowScale;
    } else if (SDL_GetDisplayUsableBounds(0, &usable) == 0) {
        const int fitW = static_cast<int>(usable.w * 0.85f) / gLogicalWidth;
        const int fitH = static_cast<int>(usable.h * 0.85f) / kLogicalHeight;
        windowScale = std::max(1, std::min(fitW, fitH));
    }
    window_ = SDL_CreateWindow(
        "Super Mario Clone",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        gLogicalWidth * windowScale, kLogicalHeight * windowScale,
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

    // Escala para llenar la ventana (manteniendo 16:9, con bandas solo si
    // la ventana tiene otra proporcion) en vez de solo multiplos enteros:
    // con escalado entero, una ventana maximizada en 1920 de ancho quedaba
    // en 426x4 = 1704 con bordes negros grandes. Sigue siendo "nearest"
    // (SDL_HINT_RENDER_SCALE_QUALITY por defecto), los pixeles no se borronean.
    SDL_RenderSetLogicalSize(renderer_, gLogicalWidth, kLogicalHeight);
    SDL_RenderSetIntegerScale(renderer_, SDL_FALSE);
    applySettings();

    if (!mario_.loadSprite(renderer_, "assets/sprites/mario_small.png",
                            "assets/sprites/mario_big.png", "assets/sprites/mario_fire.png")) {
        return false;
    }
    if (!luigi_.loadSprite(renderer_, "assets/sprites/luigi_small.png",
                            "assets/sprites/luigi_big.png", "assets/sprites/luigi_fire.png")) {
        return false;
    }
    // Paletas segun el tipo de mundo (gen_player_palettes.py): opcionales,
    // el mundo que no tenga usa la normal.
    for (const auto& [type, suffix] : {std::pair<WorldType, const char*>{WorldType::Underwater, "underwater"},
                                       {WorldType::Underground, "underground"},
                                       {WorldType::Castle, "castle"}}) {
        for (auto& [character, name] : {std::pair<Mario*, const char*>{&mario_, "mario"}, {&luigi_, "luigi"}}) {
            const std::string base = std::string("assets/sprites/") + name + "_";
            character->loadPaletteSprites(renderer_, type, base + "small_" + suffix + ".png",
                                          base + "big_" + suffix + ".png", base + "fire_" + suffix + ".png");
        }
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
    if (!oneUpSprite_.loadFromFile(renderer_, "assets/sprites/oneup_mushroom.png", Mushroom::kSize, Mushroom::kSize)) {
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
    if (!questionBlockSprite_.loadFromFile(renderer_, "assets/sprites/question_block.png", 16, 16)) {
        return false;
    }
    if (!blockCoinSprite_.loadFromFile(renderer_, "assets/sprites/block_coin.png", BlockCoin::kWidth,
                                        BlockCoin::kHeight)) {
        return false;
    }
    if (!pipeSprite_.loadFromFile(renderer_, "assets/sprites/pipe.png", 32, 16)) {
        return false;
    }
    if (!brickDebrisSprite_.loadFromFile(renderer_, "assets/sprites/brick_debris.png", BrickDebris::kSize,
                                          BrickDebris::kSize)) {
        return false;
    }
    if (!blooperSprite_.loadFromFile(renderer_, "assets/sprites/blooper.png", Blooper::kWidth, Blooper::kHeight)) {
        return false;
    }
    if (!cheepCheepSprite_.loadFromFile(renderer_, "assets/sprites/cheep_cheep.png", CheepCheep::kSize,
                                         CheepCheep::kSize)) {
        return false;
    }
    if (!bulletBillSprite_.loadFromFile(renderer_, "assets/sprites/bullet_bill.png", BulletBill::kSize,
                                         BulletBill::kSize)) {
        return false;
    }
    if (!podobooSprite_.loadFromFile(renderer_, "assets/sprites/podoboo.png", Podoboo::kSize, Podoboo::kSize)) {
        return false;
    }
    if (!bowserSprite_.loadFromFile(renderer_, "assets/sprites/bowser.png", Bowser::kWidth, Bowser::kHeight)) {
        return false;
    }
    if (!bowserFireSprite_.loadFromFile(renderer_, "assets/sprites/bowser_fire.png", BowserFire::kWidth,
                                         BowserFire::kHeight)) {
        return false;
    }
    if (!axeSprite_.loadFromFile(renderer_, "assets/sprites/axe.png", Axe::kWidth, Axe::kHeight)) {
        return false;
    }
    if (!flagpoleSprite_.loadFromFile(renderer_, "assets/sprites/flagpole.png", 16, 16)) {
        return false;
    }
    if (!flagSprite_.loadFromFile(renderer_, "assets/sprites/flag.png", 16, 16)) {
        return false;
    }
    if (!terrainSprite_.loadFromFile(renderer_, "assets/sprites/terrain.png", 16, 16)) return false;
    if (!lavaSprite_.loadFromFile(renderer_, "assets/sprites/lava.png", 16, 16)) return false;
    if (!bridgeSprite_.loadFromFile(renderer_, "assets/sprites/bridge.png", 16, 16)) return false;
    if (!liftSprite_.loadFromFile(renderer_, "assets/sprites/lift.png", 8, 8)) return false;
    if (!sceneryTilesSprite_.loadFromFile(renderer_, "assets/sprites/scenery_tiles.png", 16, 16)) return false;
    if (!titleLogoSprite_.loadFromFile(renderer_, "assets/sprites/title_logo.png", 176, 88)) return false;
    if (!titleCursorSprite_.loadFromFile(renderer_, "assets/sprites/title_cursor.png", 8, 8)) return false;
    if (!hudCoinSprite_.loadFromFile(renderer_, "assets/sprites/hud_coin.png", 8, 8)) return false;
    if (!introMarioSprite_.loadFromFile(renderer_, "assets/sprites/mario_small.png", 16, 16)) return false;
    for (int i = 0; i < kDecorationCount; ++i) {
        const DecorationInfo& info = kDecorations[i];
        const std::string path = std::string("assets/sprites/scenery_") + info.type + ".png";
        if (!decorationSprites_[i].loadFromFile(renderer_, path, info.width, info.height)) return false;
    }
    // Color fijo para toda la vida de la instancia (ver Koopa::setColor),
    // se llama una sola vez aca en vez de en cada spawn()/reset().
    koopaRed_.setColor(KoopaColor::Red);
    koopaRedFlying_.setColor(KoopaColor::Red);
    // Variante/patron fijos para toda la vida de la instancia (ver
    // CheepCheep.h), mismo criterio que setColor de arriba.
    cheepCheepSwim_.setVariant(CheepCheepVariant::Swimming);
    cheepCheepSwim_.setSwimPattern(SwimPattern::Straight);
    cheepCheepSwimOsc_.setVariant(CheepCheepVariant::Swimming);
    cheepCheepSwimOsc_.setSwimPattern(SwimPattern::Oscillating);
    for (CheepCheep& fish : cheepCheepJumps_) fish.setVariant(CheepCheepVariant::Jumping);

    // Arranca en el selector (ver renderScenarioSelect): recien se carga un
    // nivel y se ponen entidades en pie cuando se elige un escenario.
    running_ = true;
    return true;
}

void Game::startScenario(TestScenario scenario) {
    // Cada escenario de prueba arranca como una partida nueva: todavia no
    // hay partida que se arrastre de un nivel al otro (seccion 6).
    score_ = 0;
    coinCount_ = 0;
    playerScore_ = {};
    playerCoins_ = {};
    scorer_ = 0;
    livesCount_ = kStartingLives;
    checkpointCol_ = -1;
    // "Probar desde aca" del editor: la columna pedida cuenta como checkpoint.
    if (scenario == TestScenario::CustomLevel && customStartCol_ >= 0) checkpointCol_ = customStartCol_;
    loadScenario(scenario);
}

void Game::loadScenario(TestScenario scenario) {
    scenario_ = scenario;
    if (static_cast<int>(scenario) < kMenuOptionCount) menuSelection_ = static_cast<int>(scenario);
    cameraSnapPending_ = true;
    input1_.clearPresses();
    input2_.clearPresses();
    const char* path = "levels/world1/test_items.json";
    if (scenario == TestScenario::Enemies) {
        path = "levels/world1/test_enemies.json";
    } else if (scenario == TestScenario::HammerBro) {
        path = "levels/world1/test_hammerbro.json";
    } else if (scenario == TestScenario::LakituSpiny) {
        path = "levels/world1/test_lakitu.json";
    } else if (scenario == TestScenario::Blooper) {
        path = "levels/world1/test_blooper.json";
    } else if (scenario == TestScenario::CheepCheep) {
        path = "levels/world1/test_cheepcheep.json";
    } else if (scenario == TestScenario::BulletBill) {
        path = "levels/world1/test_bulletbill.json";
    } else if (scenario == TestScenario::CheepCheepJump) {
        path = "levels/world1/test_cheepcheep_jump.json";
    } else if (scenario == TestScenario::Podoboo) {
        path = "levels/world1/test_podoboo.json";
    } else if (scenario == TestScenario::Bowser) {
        path = "levels/world1/test_bowser.json";
    } else if (scenario == TestScenario::Flagpole) {
        path = "levels/world1/test_flagpole.json";
    } else if (scenario == TestScenario::Platforms) {
        path = "levels/world1/test_platforms.json";
    } else if (scenario == TestScenario::World1_1) {
        path = "levels/world1/1-1.json";
    } else if (scenario == TestScenario::CustomLevel) {
        path = customLevelPath_.c_str();
    }
    level_.loadFromFile(path);

    loadLevelData();

    // Bandera de meta: la pone el propio nivel (tiles 'F'), no el escenario.
    int poleCol = 0, poleTopRow = 0, poleBottomRow = 0;
    if (level_.findFlagpole(poleCol, poleTopRow, poleBottomRow)) {
        flagpole_.setup(poleCol, poleTopRow, poleBottomRow, level_.tileSize());
    } else {
        flagpole_.reset();
    }
    levelClearTimer_ = 0.0f;
    timeTallying_ = false;
    levelEnding_ = false;
    bridgeCollapsing_ = false;
    bridgeTilesToCollapse_.clear();
    castleClearTimer_ = 0.0f;
    castleMessage_ = false;
    marioPlatform_ = -1;
    luigiPlatform_ = -1;
    platformRects_.clear();
    {
        const float ts = static_cast<float>(level_.tileSize());
        size_t next = 0;
        for (MovingPlatform& platform : platforms_) platform.reset();
        for (const LevelPlatform& data : level_.platforms()) {
            if (next >= platforms_.size()) break;
            const PlatformMode mode = data.mode == "horizontal" ? PlatformMode::Horizontal
                                      : data.mode == "lift_up"  ? PlatformMode::LiftUp
                                      : data.mode == "lift_down" ? PlatformMode::LiftDown
                                      : data.mode == "fall"      ? PlatformMode::Fall
                                                                 : PlatformMode::Vertical;
            platforms_[next++].spawn(static_cast<float>(data.col) * ts, static_cast<float>(data.row) * ts,
                                     static_cast<int>(static_cast<float>(data.width) * ts), mode, data.range * ts,
                                     data.period, data.speed);
        }
    }
    resetLevelTimer();

    // Nivel de agua (seccion 5.5): arranca directo en Underwater en vez del
    // Overworld por defecto -- F7 lo sigue pudiendo ciclar manualmente
    // despues, pero no tendria sentido abrir este escenario a pie. El de
    // Cheep Cheep (Swimming, ver CheepCheep.h) tambien arranca en Underwater:
    // en paleta Castle (gris sobre negro) se notaba poco y quedaba dificil de
    // ver. La Jumping tiene su propio escenario (CheepCheepJump, mas abajo)
    // y SI necesita que el personaje camine, asi que ese cae en el else de
    // Overworld como cualquier otro. Podoboo y Bowser arrancan en Castle:
    // solo aparecen ahi en el original (ver Podoboo.h/Bowser.h).
    if (scenario == TestScenario::Blooper) {
        worldType_ = WorldType::Underwater;
    } else if (scenario == TestScenario::CheepCheep) {
        worldType_ = WorldType::Underwater;
    } else if (scenario == TestScenario::Podoboo) {
        worldType_ = WorldType::Castle;
    } else if (scenario == TestScenario::Bowser) {
        worldType_ = WorldType::Castle;
    } else {
        worldType_ = WorldType::Overworld;
    }
    // Si el nivel fija su tipo de mundo (campo "world"), manda ese.
    worldType_ = parseWorldType(level_.world(), worldType_);
    levelWorldType_ = worldType_;
    currentArea_ = -1;  // se resuelve en el primer updateCamera
    mario_.setWorldType(worldType_);
    luigi_.setWorldType(worldType_);

    placeCharactersAtStart();

    // Apaga todo lo de los OTROS escenarios (por si se viene de jugar otro)
    // y pone en pie solo lo que corresponde al elegido.
    mushroom_.collect();
    fireFlower_.collect();
    star_.collect();
    coin_.collect();
    blockCoin_.reset();
    itemEmerge_.active = false;
    for (Goomba& goomba : goombas_) goomba.reset();
    for (Koopa& koopa : levelKoopas_) koopa.reset();
    koopaA_.reset();
    koopaB_.reset();
    koopaJumping_.reset();
    koopaRed_.reset();
    koopaRedFlying_.reset();
    for (BuzzyBeetle& buzzy : buzzyBeetles_) buzzy.reset();
    for (PiranhaPlant& plant : piranhaPlants_) plant.reset();
    for (HammerBro& bro : hammerBros_) bro.reset();
    for (Hammer& hammer : hammers_) hammer.reset();
    lakitu_.reset();
    for (Spiny& spiny : spinies_) spiny.reset();
    for (Blooper& blooper : bloopers_) blooper.reset();
    cheepCheepSwim_.reset();
    cheepCheepSwimOsc_.reset();
    for (CheepCheep& fish : levelCheeps_) fish.reset();
    for (CheepCheep& fish : cheepCheepJumps_) fish.reset();
    // Margen antes del primer Cheep Cheep saltarin (en una zona "cheep_jump"
    // que empieza enseguida no sale uno encima del personaje al arrancar).
    cheepCheepJumpSpawnTimer_ = kCheepCheepJumpIntervalMax;
    for (BulletBill& bill : bulletBills_) bill.reset();
    for (Podoboo& podoboo : podoboos_) podoboo.reset();
    bowser_.reset();
    for (BowserFire& fire : bowserFires_) fire.reset();
    axe_.reset();

    if (scenario == TestScenario::Items) {
        // Mushroom/FireFlower/Star/BlockCoin ya no se ponen de entrada:
        // salen de sus bloques de interrogacion al golpearlos (ver
        // checkQuestionBlockHit) -- las teclas F11/F12/F4 siguen pudiendo
        // soltar los primeros tres sueltos para probarlos sin pasar por el
        // bloque. La moneda normal (Coin.h) SI se pone de entrada: no sale
        // de ningun bloque, va quieta puesta en el piso (ver el comentario
        // de kCoinTestX/Y).
        coin_.spawn(kCoinTestX, kCoinTestY);
    } else if (scenario == TestScenario::Enemies) {
        spawnGoomba(kGoombaTestX, kGoombaTestY, true);
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
    } else if (scenario == TestScenario::Blooper) {
        blooper_.spawn(kBlooperTestX, kBlooperTestY);
    } else if (scenario == TestScenario::CheepCheep) {
        cheepCheepSwim_.spawn(kCheepCheepSwimTestX, kCheepCheepSwimTestY, false);
        cheepCheepSwimOsc_.spawn(kCheepCheepSwimTestX, kCheepCheepSwimOscTestY, false);
    } else if (scenario == TestScenario::BulletBill) {
        bulletBillA_.spawn(kBulletBillATestX, kBulletBillTestY, true);
        bulletBillB_.spawn(kBulletBillBTestX, kBulletBillTestY, false);
    } else if (scenario == TestScenario::CheepCheepJump) {
        // El pool arranca vacio (ya lo dejo reset() arriba): la primera
        // aparece sola cuando el timer llega a 0 en Game::update, no hace
        // falta spawnear ninguna a mano aca.
        cheepCheepJumpSpawnTimer_ = randomRange(kCheepCheepJumpIntervalMin, kCheepCheepJumpIntervalMax);
    } else if (scenario == TestScenario::Podoboo) {
        podoboo_.spawn(kPodobooTestX, kPodobooTestY);
    } else if (scenario == TestScenario::Bowser) {
        bowser_.spawn(kBowserTestX, kBowserTestY, false);
        axe_.spawn(kAxeTestX, kAxeTestY);
    }

    appState_ = AppState::LevelIntro;
    resetLevelMusic();
    // Escena de introduccion (si el nivel tiene): solo arrancando desde el
    // principio, no reapareciendo en un checkpoint.
    cutscene_ = level_.introArea() >= 0 && checkpointCol_ < 0;
    pipeTransitionTimer_ = 0.0f;
    pipeAnim_ = PipeAnim{};
    input1_.setScripted(cutscene_, {Action::MoveRight});
    input2_.setScripted(cutscene_, {Action::MoveRight});
    appStateTimer_ = kLevelIntroDuration;
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running_ = false;
            continue;
        }
        // Alt+Enter: pantalla completa, en cualquier pantalla (antes que
        // nada, asi el Enter no confirma tambien una opcion).
        if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_RETURN &&
            (event.key.keysym.mod & KMOD_ALT)) {
            toggleFullscreen();
            continue;
        }
        if (isMenuState()) {
            handleMenuEvent(event);
            continue;
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            // Cualquier mando conectado maneja el menu (no solo el de
            // Player1). A diferencia de ESC, el mando nunca cierra el juego
            // desde el selector, para no cerrarlo por error.
            const Uint8 button = event.cbutton.button;
            if (appState_ == AppState::ScenarioSelect) {
                if (button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                    moveMenuSelection(-1);
                } else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                    moveMenuSelection(1);
                } else if (button == SDL_CONTROLLER_BUTTON_A || button == SDL_CONTROLLER_BUTTON_START) {
                    startScenario(static_cast<TestScenario>(menuSelection_));
                } else if (button == SDL_CONTROLLER_BUTTON_B || button == SDL_CONTROLLER_BUTTON_BACK) {
                    openTitle();
                }
            } else if (appState_ == AppState::Playing &&
                       (button == SDL_CONTROLLER_BUTTON_START || button == SDL_CONTROLLER_BUTTON_BACK)) {
                openPause();
            }
        } else if (event.type == SDL_CONTROLLERAXISMOTION &&
                   event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            constexpr Sint16 kMenuStickPress = 24000;
            constexpr Sint16 kMenuStickRelease = 8000;
            const int value = event.caxis.value;
            if (std::abs(value) < kMenuStickRelease) {
                menuStickHeld_ = false;
            } else if (std::abs(value) > kMenuStickPress && !menuStickHeld_) {
                menuStickHeld_ = true;
                if (appState_ == AppState::ScenarioSelect) moveMenuSelection(value < 0 ? -1 : 1);
            }
        } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            const SDL_Scancode pressed = event.key.keysym.scancode;
            if (pressed == SDL_SCANCODE_ESCAPE || (pressed == SDL_SCANCODE_RETURN && appState_ == AppState::Playing)) {
                // ESC/Enter jugando: pausa. ESC en el modo prueba o en las
                // pantallas negras: vuelve al menu principal (ahi esta
                // SALIR, ESC ya no cierra el juego de golpe).
                if (appState_ == AppState::Playing) {
                    openPause();
                } else {
                    openTitle();
                }
                continue;
            }

            if (appState_ == AppState::ScenarioSelect) {
                const SDL_Scancode key = event.key.keysym.scancode;
                if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_0) {
                    // SDL ordena 1..9 y despues 0, igual que las opciones
                    // del selector (0 = Bowser, la decima).
                    startScenario(static_cast<TestScenario>(key - SDL_SCANCODE_1));
                } else if (key == SDL_SCANCODE_UP || key == SDL_SCANCODE_W) {
                    moveMenuSelection(-1);
                } else if (key == SDL_SCANCODE_DOWN || key == SDL_SCANCODE_S) {
                    moveMenuSelection(1);
                } else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_SPACE) {
                    startScenario(static_cast<TestScenario>(menuSelection_));
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
                spawnGoomba(kGoombaTestX, kGoombaTestY, true);
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
            } else if (event.key.keysym.scancode == SDL_SCANCODE_N && scenario_ == TestScenario::CheepCheep) {
                cheepCheepSwim_.spawn(kCheepCheepSwimTestX, kCheepCheepSwimTestY, false);
                cheepCheepSwimOsc_.spawn(kCheepCheepSwimTestX, kCheepCheepSwimOscTestY, false);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_J && scenario_ == TestScenario::CheepCheepJump) {
                // Fuerza una aparicion de una sin esperar al timer, igual que
                // el disparo manual de Spiny (tecla S en LakituSpiny).
                spawnNextCheepCheepJump();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_I && scenario_ == TestScenario::BulletBill) {
                bulletBillA_.spawn(kBulletBillATestX, kBulletBillTestY, true);
                bulletBillB_.spawn(kBulletBillBTestX, kBulletBillTestY, false);
                audio_.play(Sfx::BillFirework);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_P && scenario_ == TestScenario::Podoboo) {
                podoboo_.spawn(kPodobooTestX, kPodobooTestY);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_Y && scenario_ == TestScenario::Bowser) {
                resetCastleSequence();
                bowser_.spawn(kBowserTestX, kBowserTestY, false);
                axe_.spawn(kAxeTestX, kAxeTestY);
                for (BowserFire& fire : bowserFires_) fire.reset();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_T && scenario_ == TestScenario::Bowser) {
                // Cicla FireOnly -> HammerOnly -> Both -> FireOnly (ver
                // BowserAttackMode en Bowser.h) y reinicia todo para
                // probar el modo nuevo desde cero, mismo criterio que la
                // tecla Y.
                const BowserAttackMode current = bowser_.attackMode();
                const BowserAttackMode next = current == BowserAttackMode::FireOnly     ? BowserAttackMode::HammerOnly
                                               : current == BowserAttackMode::HammerOnly ? BowserAttackMode::Both
                                                                                          : BowserAttackMode::FireOnly;
                bowser_.setAttackMode(next);
                resetCastleSequence();
                bowser_.spawn(kBowserTestX, kBowserTestY, false);
                axe_.spawn(kAxeTestX, kAxeTestY);
                for (BowserFire& fire : bowserFires_) fire.reset();
                for (Hammer& hammer : hammers_) hammer.reset();
            } else if (event.key.keysym.scancode == SDL_SCANCODE_M) {
                twoPlayerMode_ = !twoPlayerMode_;
                if (twoPlayerMode_) {
                    // Luigi pudo haber quedado en cualquier lado del nivel
                    // (fuera de pantalla si Mario ya avanzo): entra en la
                    // pantalla actual, igual que al reaparecer.
                    const SDL_Point point = findRespawnPoint(cameraX_ + gLogicalWidth / 2.0f, luigi_.height());
                    luigi_.spawn(static_cast<float>(point.x), static_cast<float>(point.y));
                }
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
        // Altura real del original (Player_Y_Position: el tope del sprite,
        // no un porcentaje arbitrario de su alto) -- coincide con la mano
        // en la pose de tirar (kAnimRowThrow), que ya esta jugando en este
        // mismo instante (ver playThrowAnimation() mas abajo). kHandYOffset:
        // un poco mas abajo del tope justo, a pedido del usuario.
        constexpr float kHandYOffset = 6.0f;
        const float spawnY = shooter.spriteTopY() + kHandYOffset;
        fireball.spawn(spawnX, spawnY, right);
        shooter.playThrowAnimation();
        audio_.play(Sfx::Fireball);
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
    setScorer(character);
    // Sin colision mientras esta en su secuencia de muerte (ver el mismo
    // criterio en checkGoombaCollision/checkKoopaCollision): no tiene
    // sentido seguir interactuando con nada hasta que reaparezca.
    if (character.isInDeathSequence()) return;
    if (!mushroom_.isAlive()) return;

    const bool overlap =
        character.x() < mushroom_.x() + Mushroom::kSize && character.x() + character.width() > mushroom_.x() &&
        character.y() < mushroom_.y() + Mushroom::kSize && character.y() + character.height() > mushroom_.y();
    if (!overlap) return;

    if (mushroom_.isOneUp()) {
        ++livesCount_;
        spawnScorePopup(mushroom_.x(), mushroom_.y(), "1UP");
        mushroom_.collect();
        audio_.play(Sfx::OneUp);
        return;
    }

    if (character.size() == MarioSize::Small) {
        character.startGrow();
    }
    spawnScorePopup(mushroom_.x(), mushroom_.y(), "1000");
    mushroom_.collect();
    audio_.play(Sfx::PowerUp);
}

void Game::checkFireFlowerPickup(Mario& character) {
    setScorer(character);
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
    audio_.play(Sfx::PowerUp);
}

void Game::checkStarPickup(Mario& character) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!star_.isAlive()) return;

    const bool overlap =
        character.x() < star_.x() + StarItem::kSize && character.x() + character.width() > star_.x() &&
        character.y() < star_.y() + StarItem::kSize && character.y() + character.height() > star_.y();
    if (!overlap) return;

    character.setInvincible(true);
    spawnScorePopup(star_.x(), star_.y(), "1000");
    star_.collect();
    audio_.play(Sfx::PowerUp);
}

void Game::checkCoinPickup(Mario& character) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!coin_.isAlive()) return;

    const bool overlap =
        character.x() < coin_.x() + Coin::kSize && character.x() + character.width() > coin_.x() &&
        character.y() < coin_.y() + Coin::kSize && character.y() + character.height() > coin_.y();
    if (!overlap) return;

    // Sin popup de puntaje: la moneda no lo tiene en el original.
    coin_.collect();
    collectCoin();
    audio_.play(Sfx::Coin);
}

void Game::applyContactDamage(Mario& character) {
    if (character.size() == MarioSize::Small) {
        if (character.startDeath()) audio_.play(Sfx::Death);
    } else if (character.takeDamage()) {
        audio_.play(Sfx::PowerDown);
    }
}

bool Game::handleUnderwaterContactDamage(Mario& character) {
    if (character.worldType() != WorldType::Underwater) return false;
    applyContactDamage(character);
    return true;
}

void Game::checkGoombaCollision(Mario& character, Goomba& goomba, float prevY) {
    setScorer(character);
    // Sin colision mientras esta en su secuencia de muerte: ya esta cayendo
    // con la pose de "muerto" fija, no tiene sentido que un enemigo lo
    // pisotee/dane/mate de nuevo hasta que reaparezca (relevante en 2P: el
    // otro jugador puede seguir jugando mientras este cae).
    if (character.isInDeathSequence()) return;
    if (!goomba.isDangerous()) return;

    const bool overlap =
        character.x() < goomba.x() + Goomba::kSize && character.x() + character.width() > goomba.x() &&
        character.y() < goomba.y() + Goomba::kSize && character.y() + character.height() > goomba.y();
    if (!overlap) return;

    // Con estrella: lo elimina de un toque, desde cualquier lado, sin
    // pisoton (no hay rebote) ni dano -- la invencibilidad ya cubre el "no
    // recibe dano" del lado del personaje. Muerte distinta a un pisoton: sale
    // despedido en parabola hacia el lado contrario de donde vino el
    // personaje, en vez de achatarse (ver Goomba::killFalling).
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= goomba.x();
        killEnemyFalling(goomba, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

    // Pisoton: los pies YA estaban por encima de la cabeza del Goomba antes
    // de moverse este frame (no importa cuanto se haya hundido AHORA -- eso
    // evita confundirlo con un golpe de costado por caer rapido o pegarle
    // en diagonal, que es lo que fallaba comparando solo la posicion actual).
    const bool fromAbove = (prevY + character.height()) <= goomba.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        goomba.squish();
        character.setVelocityY(kStompBounce);
        audio_.play(Sfx::StompKill);
        awardEnemyPoints(goomba.x(), goomba.y(), 100);
        return;
    }

    // Contacto de costado: mismo dano que takeDamage()/startDeath() normal.
    applyContactDamage(character);
}

void Game::checkFireballGoombaCollision(Fireball& fireball, Goomba& goomba) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!goomba.isDangerous()) return;

    const bool overlap =
        fireball.x() < goomba.x() + Goomba::kSize && fireball.x() + Fireball::kSize > goomba.x() &&
        fireball.y() < goomba.y() + Goomba::kSize && fireball.y() + Fireball::kSize > goomba.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= goomba.x();
    killEnemyFalling(goomba, flyRight);
    fireball.explode();
}

void Game::checkKoopaCollision(Mario& character, Koopa& koopa, float prevY) {
    setScorer(character);
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
        killEnemyFalling(koopa, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

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
            audio_.play(Sfx::Kick);
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
            audio_.play(Sfx::Kick);
        }
        return;
    }

    // Walking o ShellMoving: pisoton lo esconde/frena (con rebote); de
    // costado hace dano como cualquier enemigo.
    if (fromAbove) {
        koopa.stomp(&character);
        awardEnemyPoints(koopa.x(), koopa.y(), 100);
        character.setVelocityY(kKoopaStompBounce);
        character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
        audio_.play(Sfx::StompKill);
        return;
    }
    applyContactDamage(character);
}

void Game::checkFireballKoopaCollision(Fireball& fireball, Koopa& koopa) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!koopa.isDangerous()) return;

    const bool overlap =
        fireball.x() < koopa.x() + koopa.width() && fireball.x() + Fireball::kSize > koopa.x() &&
        fireball.y() < koopa.y() + koopa.height() && fireball.y() + Fireball::kSize > koopa.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= koopa.x();
    killEnemyFalling(koopa, flyRight);
    fireball.explode();
}

void Game::checkShellVsGoomba(Koopa& koopa, Goomba& goomba) {
    if (!koopa.isMovingShell()) return;
    if (!goomba.isDangerous()) return;

    const bool overlap =
        koopa.x() < goomba.x() + Goomba::kSize && koopa.x() + koopa.width() > goomba.x() &&
        koopa.y() < goomba.y() + Goomba::kSize && koopa.y() + koopa.height() > goomba.y();
    if (!overlap) return;

    const bool flyRight = goomba.x() >= koopa.x();
    setScorerFromKicker(koopa.kicker());
    killEnemyFalling(goomba, flyRight);
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
    audio_.play(Sfx::Kick);
    setScorerFromKicker(aMoving ? a.kicker() : b.kicker());

    if (aMoving && bMoving) {
        // Chocan de frente: mueren los dos, cada uno hacia el lado contrario del otro.
        const bool aFliesRight = a.x() >= b.x();
        killEnemyFalling(a, aFliesRight);
        killEnemyFalling(b, !aFliesRight);
    } else if (aMoving) {
        const bool bFliesRight = b.x() >= a.x();
        killEnemyFalling(b, bFliesRight);
    } else {
        const bool aFliesRight = a.x() >= b.x();
        killEnemyFalling(a, aFliesRight);
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
    setScorer(character);
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
        killEnemyFalling(buzzy, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

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
            audio_.play(Sfx::Kick);
            character.setVelocityY(0.0f);
            character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
        } else {
            const bool kickRight = character.x() <= buzzy.x();
            buzzy.kick(kickRight, &character);
            audio_.play(Sfx::Kick);
        }
        return;
    }

    // Walking o ShellMoving: pisoton lo esconde/frena (con rebote); de
    // costado hace dano como cualquier enemigo.
    if (fromAbove) {
        buzzy.stomp(&character);
        awardEnemyPoints(buzzy.x(), buzzy.y(), 100);
        character.setVelocityY(kKoopaStompBounce);
        character.setVelocityX(character.facingRight() ? kStompSidewaysSpeed : -kStompSidewaysSpeed);
        audio_.play(Sfx::StompKill);
        return;
    }
    applyContactDamage(character);
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
    audio_.play(Sfx::Kick);
    setScorerFromKicker(koopaMoving ? koopa.kicker() : buzzy.kicker());

    if (koopaMoving && buzzyMoving) {
        const bool koopaFliesRight = koopa.x() >= buzzy.x();
        killEnemyFalling(koopa, koopaFliesRight);
        killEnemyFalling(buzzy, !koopaFliesRight);
    } else if (koopaMoving) {
        const bool buzzyFliesRight = buzzy.x() >= koopa.x();
        killEnemyFalling(buzzy, buzzyFliesRight);
    } else {
        const bool koopaFliesRight = koopa.x() >= buzzy.x();
        killEnemyFalling(koopa, koopaFliesRight);
    }
}

void Game::checkBuzzyShellVsGoomba(BuzzyBeetle& buzzy, Goomba& goomba) {
    if (!buzzy.isMovingShell()) return;
    if (!goomba.isDangerous()) return;

    const bool overlap =
        buzzy.x() < goomba.x() + Goomba::kSize && buzzy.x() + buzzy.width() > goomba.x() &&
        buzzy.y() < goomba.y() + Goomba::kSize && buzzy.y() + buzzy.height() > goomba.y();
    if (!overlap) return;

    const bool flyRight = goomba.x() >= buzzy.x();
    setScorerFromKicker(buzzy.kicker());
    killEnemyFalling(goomba, flyRight);
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
    setScorer(character);
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
    applyContactDamage(character);
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

void Game::checkHammerBroCollision(Mario& character, HammerBro& bro, float prevY) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!bro.isDangerous()) return;

    const bool overlap = character.x() < bro.x() + bro.width() &&
                          character.x() + character.width() > bro.x() &&
                          character.y() < bro.y() + bro.height() &&
                          character.y() + character.height() > bro.y();
    if (!overlap) return;

    // Cualquier forma de matarlo usa la misma parabola (ver HammerBro.h):
    // no hay caparazon ni frame de achatado propio en el arte fuente.
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= bro.x();
        killEnemyFalling(bro, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

    const bool fromAbove = (prevY + character.height()) <= bro.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        const bool flyRight = character.x() <= bro.x();
        killEnemyFalling(bro, flyRight);
        character.setVelocityY(kStompBounce);
        return;
    }

    applyContactDamage(character);
}

void Game::checkFireballHammerBroCollision(Fireball& fireball, HammerBro& bro) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!bro.isDangerous()) return;

    const bool overlap = fireball.x() < bro.x() + bro.width() &&
                          fireball.x() + Fireball::kSize > bro.x() &&
                          fireball.y() < bro.y() + bro.height() &&
                          fireball.y() + Fireball::kSize > bro.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= bro.x();
    killEnemyFalling(bro, flyRight);
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

    applyContactDamage(character);
}

void Game::checkLakituCollision(Mario& character, float prevY) {
    setScorer(character);
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
        killEnemyFalling(lakitu_, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

    const bool fromAbove = (prevY + character.height()) <= lakitu_.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        const bool flyRight = character.x() <= lakitu_.x();
        killEnemyFalling(lakitu_, flyRight);
        character.setVelocityY(kStompBounce);
        return;
    }

    applyContactDamage(character);
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
    killEnemyFalling(lakitu_, flyRight);
    fireball.explode();
}

void Game::checkSpinyCollision(Mario& character, Spiny& spiny) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!spiny.isDangerous()) return;

    const bool overlap =
        character.x() < spiny.x() + Spiny::kSize && character.x() + character.width() > spiny.x() &&
        character.y() < spiny.y() + Spiny::kSize && character.y() + character.height() > spiny.y();
    if (!overlap) return;

    if (character.isInvincible()) {
        const bool flyRight = character.x() <= spiny.x();
        killEnemyFalling(spiny, flyRight);
        return;
    }

    // Sin pisoton (ver Spiny.h): cualquier contacto hace dano, sin importar
    // de que lado venga.
    applyContactDamage(character);
}

void Game::checkFireballSpinyCollision(Fireball& fireball, Spiny& spiny) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!spiny.isDangerous()) return;

    const bool overlap =
        fireball.x() < spiny.x() + Spiny::kSize && fireball.x() + Fireball::kSize > spiny.x() &&
        fireball.y() < spiny.y() + Spiny::kSize && fireball.y() + Fireball::kSize > spiny.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= spiny.x();
    killEnemyFalling(spiny, flyRight);
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
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!blooper.isDangerous()) return;

    const bool overlap = character.x() < blooper.x() + blooper.width() &&
                          character.x() + character.width() > blooper.x() &&
                          character.y() < blooper.y() + blooper.height() &&
                          character.y() + character.height() > blooper.y();
    if (!overlap) return;

    if (character.isInvincible()) {
        const bool flyRight = character.x() <= blooper.x();
        killEnemyFalling(blooper, flyRight);
        return;
    }

    // Sin pisoton (ver Blooper.h): cualquier contacto hace dano, sin
    // importar de que lado venga.
    applyContactDamage(character);
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
    killEnemyFalling(blooper, flyRight);
    fireball.explode();
}

void Game::checkCheepCheepCollision(Mario& character, CheepCheep& cheepCheep, float prevY) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!cheepCheep.isDangerous()) return;

    const bool overlap = character.x() < cheepCheep.x() + CheepCheep::kSize &&
                          character.x() + character.width() > cheepCheep.x() &&
                          character.y() < cheepCheep.y() + CheepCheep::kSize &&
                          character.y() + character.height() > cheepCheep.y();
    if (!overlap) return;

    // Cualquier forma de matarlo usa la misma parabola (ver CheepCheep.h):
    // no hay caparazon ni frame de achatado propio en el arte fuente en
    // ninguna de las dos variantes.
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= cheepCheep.x();
        killEnemyFalling(cheepCheep, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

    const bool fromAbove = (prevY + character.height()) <= cheepCheep.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        const bool flyRight = character.x() <= cheepCheep.x();
        killEnemyFalling(cheepCheep, flyRight);
        character.setVelocityY(kStompBounce);
        return;
    }

    applyContactDamage(character);
}

void Game::checkFireballCheepCheepCollision(Fireball& fireball, CheepCheep& cheepCheep) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!cheepCheep.isDangerous()) return;

    const bool overlap = fireball.x() < cheepCheep.x() + CheepCheep::kSize &&
                          fireball.x() + Fireball::kSize > cheepCheep.x() &&
                          fireball.y() < cheepCheep.y() + CheepCheep::kSize &&
                          fireball.y() + Fireball::kSize > cheepCheep.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= cheepCheep.x();
    killEnemyFalling(cheepCheep, flyRight);
    fireball.explode();
}

void Game::spawnNextCheepCheepJump() {
    for (CheepCheep& fish : cheepCheepJumps_) {
        if (fish.isAlive()) continue;
        const bool leaderIsLuigi = twoPlayerMode_ && luigi_.x() > mario_.x();
        const Mario& leader = leaderIsLuigi ? luigi_ : mario_;
        const float offset = randomRange(kCheepCheepJumpAheadOffsetMin, kCheepCheepJumpAheadOffsetMax);

        // De que lado salta: moneda 50/50 -- la inclinacion segun
        // caminando/corriendo (comportamiento real de SMB1) terminaba
        // saltando casi siempre de atras en este escenario largo, porque en
        // un nivel tan largo el personaje pasa la mayor parte del tiempo
        // corriendo (Action::Run sostenido) para cruzarlo. Se saca el sesgo
        // del todo para que se note bien mezclado sin importar la velocidad.
        const bool spawnBehind = randomRange(0.0f, 1.0f) < 0.5f;
        const bool travelingRight = leader.facingRight();
        const float signedAheadOffset = travelingRight ? offset : -offset;
        const float spawnX = leader.x() + (spawnBehind ? -signedAheadOffset : signedAheadOffset);

        // Deriva propia (izquierda o derecha, ver el comentario de
        // CheepCheep.h): alternada, no una moneda independiente cada vez --
        // con moneda independiente una racha de varias seguidas al mismo
        // lado es estadisticamente normal pero se nota mal en la practica
        // (parece que "solo saltan para un lado").
        cheepCheepJumpLastDriftRight_ = !cheepCheepJumpLastDriftRight_;
        fish.spawn(spawnX, kCheepCheepJumpSpawnY, cheepCheepJumpLastDriftRight_);
        return;
    }
}

void Game::checkBulletBillCollision(Mario& character, BulletBill& bulletBill, float prevY) {
    setScorer(character);
    // Ver el comentario en checkGoombaCollision.
    if (character.isInDeathSequence()) return;
    if (!bulletBill.isDangerous()) return;

    const bool overlap = character.x() < bulletBill.x() + BulletBill::kSize &&
                          character.x() + character.width() > bulletBill.x() &&
                          character.y() < bulletBill.y() + BulletBill::kSize &&
                          character.y() + character.height() > bulletBill.y();
    if (!overlap) return;

    // Con estrella: lo elimina de un toque, desde cualquier lado, sin
    // pisoton (ver el mismo criterio en checkGoombaCollision).
    if (character.isInvincible()) {
        const bool flyRight = character.x() <= bulletBill.x();
        killEnemyFalling(bulletBill, flyRight);
        return;
    }
    if (handleUnderwaterContactDamage(character)) return;

    // Pisoton: SI se puede pisar (a diferencia de Blooper/Spiny/Pirana
    // Plant, ver BulletBill.h) -- mismo criterio de "los pies ya estaban
    // por encima" que el resto del elenco.
    const bool fromAbove = (prevY + character.height()) <= bulletBill.y() + kStompLeewayPx &&
                            character.velocityY() > 0.0f;
    if (fromAbove) {
        const bool flyRight = character.x() <= bulletBill.x();
        killEnemyFalling(bulletBill, flyRight);
        character.setVelocityY(kStompBounce);
        return;
    }

    // De frente: dano (ver el comentario de la clase).
    applyContactDamage(character);
}

void Game::checkFireballBulletBillCollision(Fireball& fireball, BulletBill& bulletBill) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!bulletBill.isDangerous()) return;

    const bool overlap = fireball.x() < bulletBill.x() + BulletBill::kSize &&
                          fireball.x() + Fireball::kSize > bulletBill.x() &&
                          fireball.y() < bulletBill.y() + BulletBill::kSize &&
                          fireball.y() + Fireball::kSize > bulletBill.y();
    if (!overlap) return;

    const bool flyRight = fireball.x() <= bulletBill.x();
    killEnemyFalling(bulletBill, flyRight);
    fireball.explode();
}

void Game::checkPodobooCollision(Mario& character, Podoboo& podoboo) {
    if (character.isInDeathSequence()) return;
    if (!podoboo.isDangerous()) return;

    const bool overlap =
        character.x() < podoboo.x() + podoboo.width() && character.x() + character.width() > podoboo.x() &&
        character.y() < podoboo.y() + podoboo.height() && character.y() + character.height() > podoboo.y();
    if (!overlap) return;

    // Invencible de verdad (ver Podoboo.h): la estrella sigue protegiendo a
    // quien lo toca del dano como siempre, pero el Podoboo no desaparece --
    // por eso, a diferencia de cualquier otro chequeo de colision, esta
    // rama no llama nada sobre el Podoboo, solo corta antes del dano.
    if (character.isInvincible()) return;

    // Sin pisoton (ver el comentario de la clase): cualquier contacto hace
    // dano, sin importar de que lado venga.
    applyContactDamage(character);
}

void Game::checkFireballPodobooCollision(Fireball& fireball, Podoboo& podoboo) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!podoboo.isDangerous()) return;

    const bool overlap = fireball.x() < podoboo.x() + podoboo.width() &&
                          fireball.x() + Fireball::kSize > podoboo.x() &&
                          fireball.y() < podoboo.y() + podoboo.height() &&
                          fireball.y() + Fireball::kSize > podoboo.y();
    if (!overlap) return;

    // Invencible (ver Podoboo.h): la bola explota igual, pero al Podoboo no
    // le pasa nada.
    fireball.explode();
}

void Game::checkBowserCollision(Mario& character) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!bowser_.isDangerous()) return;

    const bool overlap =
        character.x() < bowser_.x() + bowser_.width() && character.x() + character.width() > bowser_.x() &&
        character.y() < bowser_.y() + bowser_.height() && character.y() + character.height() > bowser_.y();
    if (!overlap) return;

    // Ni la estrella lo derrota (ver Bowser.h): protege del dano como
    // siempre, pero a Bowser no le pasa nada -- solo killByFireball()
    // (checkFireballBowserCollision) o killByAxe() (checkAxeCollision) lo
    // matan de verdad.
    if (character.isInvincible()) return;

    applyContactDamage(character);
}

void Game::checkFireballBowserCollision(Fireball& fireball) {
    if (!fireball.isAlive() || fireball.isExploding()) return;
    if (!bowser_.isDangerous()) return;

    const bool overlap = fireball.x() < bowser_.x() + bowser_.width() &&
                          fireball.x() + Fireball::kSize > bowser_.x() &&
                          fireball.y() < bowser_.y() + bowser_.height() &&
                          fireball.y() + Fireball::kSize > bowser_.y();
    if (!overlap) return;

    // A diferencia del resto del elenco: no lo mata de una, solo cuenta
    // para las cinco que hacen falta (ver Bowser::killByFireball).
    const bool flyRight = fireball.x() <= bowser_.x();
    bowser_.killByFireball(flyRight);
    // El quinto fuego es el que lo derrota: ahi da los puntos.
    if (bowser_.isFalling()) awardEnemyPoints(bowser_.x(), bowser_.y(), 5000);
    fireball.explode();
}

void Game::checkBowserFireCollision(Mario& character, BowserFire& fire) {
    if (character.isInDeathSequence()) return;
    if (!fire.isAlive()) return;

    const bool overlap =
        character.x() < fire.x() + fire.width() && character.x() + character.width() > fire.x() &&
        character.y() < fire.y() + fire.height() && character.y() + character.height() > fire.y();
    if (!overlap) return;

    // No es un enemigo (no hace falta pisarlo ni matarlo) y no desaparece
    // al tocar a nadie (ver BowserFire.h): sigue de largo, mismo criterio
    // que Hammer/checkHammerCollision. Con estrella no hace dano.
    if (character.isInvincible()) return;

    applyContactDamage(character);
}

void Game::checkAxeCollision(Mario& character, Axe& axe) {
    setScorer(character);
    if (character.isInDeathSequence()) return;
    if (!axe.isAlive()) return;

    const bool overlap =
        character.x() < axe.x() + axe.width() && character.x() + character.width() > axe.x() &&
        character.y() < axe.y() + axe.height() && character.y() + character.height() > axe.y();
    if (!overlap) return;

    // Unica forma de derrotarlo de una sola vez sin ser de fuego (ver
    // Axe.h/Bowser.h): tambien vale si ya esta derrotado (por si llega
    // caminando despues de matarlo con fuego), no hace nada raro llamar
    // killByAxe() sobre un Bowser que ya no es "dangerous".
    axe.collect();
    levelEnding_ = true;
    levelFinisher_ = scorer_;
    castleMessage_ = true;
    if (level_.hasAnyBridge()) {
        startBridgeCollapse();
        return;
    }
    // Sin puente (castillo armado a mano): cae de una, como antes.
    const bool flyRight = character.x() <= bowser_.x();
    bowser_.killByAxe(flyRight);
    audio_.play(Sfx::BowserFall);
    castleClearTimer_ = kCastleClearDelay;
}

void Game::checkPipeWarp(Mario& character, Input& input) {
    if (!character.isGrounded()) return;
    if (!input.consumePress(Action::Down)) return;

    const int ts = level_.tileSize();
    // Fila donde estan parados los pies (grounded_ ya garantiza que hay algo
    // solido justo ahi debajo) y las dos columnas que ocupa el hitbox --
    // alcanza con que cualquiera de las dos caiga en el tope de una tuberia
    // configurada, no hace falta estar perfectamente centrado.
    const int row = static_cast<int>(std::floor((character.y() + character.height()) / ts));
    const int col0 = static_cast<int>(std::floor(character.x() / ts));
    const int col1 = static_cast<int>(std::floor((character.x() + character.width() - 1.0f) / ts));

    for (const PipeWarp& warp : pipeWarps_) {
        if (warp.side || warp.row != row) continue;
        if (col1 < warp.col || col0 > warp.col + 1) continue;
        if (!level_.isPipeTopTile(warp.col, warp.row)) continue;  // por si el nivel cambio
        // Como el original: hay que estar parado en el medio del cano.
        const float pipeCenter = static_cast<float>(warp.col * ts + ts);
        const float center = character.x() + static_cast<float>(character.width()) / 2.0f;
        if (std::abs(center - pipeCenter) > kPipeCenterTolerance) continue;
        startPipeEnter(character, warp);
        return;
    }
}

void Game::checkSideWarp(Mario& character) {
    if (character.isInDeathSequence() || pipeTransitionTimer_ > 0.0f) return;
    const float ts = static_cast<float>(level_.tileSize());
    for (const PipeWarp& warp : pipeWarps_) {
        if (!warp.side) continue;
        // Adentro de la boca (2x2 tiles): metido al menos medio tile y con
        // el cuerpo entero entre el techo y el piso de la boca.
        const float left = static_cast<float>(warp.col) * ts;
        const float top = static_cast<float>(warp.row) * ts;
        const bool inside = character.x() >= left + ts / 2.0f && character.x() < left + 2.0f * ts &&
                            character.y() >= top - 1.0f && character.y() + character.height() <= top + 2.0f * ts + 1.0f;
        if (!inside) continue;
        if (cutscene_) {
            // Intro: se queda adentro (tapado por la tuberia) mientras suena
            // el jingle, y despues aparece en el nivel.
            pendingWarp_ = warp;
            pipeTransitionTimer_ = kIntroPipeDuration;
            audio_.playMusic(Music::IntoPipe, false);
        } else {
            startPipeEnter(character, warp);
        }
        return;
    }
}

void Game::startPipeEnter(Mario& character, const PipeWarp& warp) {
    audio_.play(Sfx::Pipe);
    pipeAnim_ = PipeAnim{};
    pipeAnim_.active = true;
    pipeAnim_.character = &character;
    pipeAnim_.warp = warp;
    pipeAnim_.timer = kPipeEnterDuration;
    character.setVelocityX(0.0f);
    character.setVelocityY(0.0f);
    character.setGrounded(true);
    if (warp.side) {
        // Lateral: sigue caminando hacia adentro hasta quedar tapado.
        character.setFacingRight(true);
        pipeAnim_.velX = static_cast<float>(level_.tileSize() + character.width()) / kPipeEnterDuration;
    } else {
        // Vertical: centrado en el cano, se hunde su propio alto.
        const float ts = static_cast<float>(level_.tileSize());
        character.nudgeX(static_cast<float>(warp.col) * ts + ts - static_cast<float>(character.width()) / 2.0f -
                         character.x());
        pipeAnim_.velY = static_cast<float>(character.height()) / kPipeEnterDuration;
    }
}

void Game::updatePipeAnim(float dt) {
    Mario& character = *pipeAnim_.character;
    character.nudgeX(pipeAnim_.velX * dt);
    character.nudgeY(pipeAnim_.velY * dt);
    character.setGrounded(true);
    pipeAnim_.timer -= dt;
    if (pipeAnim_.timer > 0.0f) return;

    if (pipeAnim_.exiting) {
        // Termino de salir: sigue el juego.
        pipeAnim_ = PipeAnim{};
        return;
    }
    performWarp(character, pipeAnim_.warp);
    // Si el destino esta sobre una tuberia, sale de ella (arranca adentro
    // y sube su propio alto); si no, aparece normal (por ejemplo cayendo en
    // un cuarto subterraneo).
    const int ts = level_.tileSize();
    const int centerX = static_cast<int>(character.x()) + character.width() / 2;
    int col = centerX / ts;
    for (int row = static_cast<int>(character.y()) / ts; row < level_.heightTiles(); ++row) {
        if (!level_.isSolidTile(col, row)) continue;
        if (level_.isPipeTopTile(col, row)) {
            const int left = level_.isPipeLeftHalf(col, row) ? col : col - 1;
            const float pipeTop = static_cast<float>(row * ts);
            character.spawn(static_cast<float>(left * ts + ts) - static_cast<float>(character.width()) / 2.0f,
                            pipeTop);
            character.setGrounded(true);
            audio_.play(Sfx::Pipe);
            pipeAnim_.exiting = true;
            pipeAnim_.velX = 0.0f;
            pipeAnim_.velY = -static_cast<float>(character.height()) / kPipeExitDuration;
            pipeAnim_.timer = kPipeExitDuration;
            return;
        }
        break;
    }
    pipeAnim_ = PipeAnim{};
}

void Game::keepCharacterInsideArea(Mario& character) {
    if (character.isInDeathSequence() || character.isOnFlagpole()) return;
    const std::vector<LevelArea>& areas = level_.areas();
    const float ts = static_cast<float>(level_.tileSize());
    float minX = 0.0f;
    float maxX = static_cast<float>(level_.widthPixels());
    const int index = areaIndexAt(character.x() + static_cast<float>(character.width()) / 2.0f);
    if (index >= 0 && index < static_cast<int>(areas.size())) {
        minX = static_cast<float>(areas[static_cast<size_t>(index)].startCol) * ts;
        maxX = static_cast<float>(areas[static_cast<size_t>(index)].endCol + 1) * ts;
    }
    maxX -= static_cast<float>(character.width());
    if (character.x() < minX) {
        character.nudgeX(minX - character.x());
        if (character.velocityX() < 0.0f) character.setVelocityX(0.0f);
    } else if (character.x() > maxX) {
        character.nudgeX(maxX - character.x());
        if (character.velocityX() > 0.0f) character.setVelocityX(0.0f);
    }
}

void Game::finishIntroPipe() {
    cutscene_ = false;
    input1_.setScripted(false);
    input2_.setScripted(false);
    performWarp(mario_, pendingWarp_);
}

void Game::performWarp(Mario& character, const PipeWarp& warp) {
    character.spawn(warp.destX, warp.destY);
    // En 2P el companero viaja con el (si no, keepCharactersOnScreen() no
    // tendria pantalla que los contenga a los dos): aparece al lado, del
    // primer costado que no quede metido en una pared.
    Mario& partner = &character == &mario_ ? luigi_ : mario_;
    if (twoPlayerMode_ && !partner.isInDeathSequence()) {
        const float ts = static_cast<float>(level_.tileSize());
        float partnerX = warp.destX;
        for (const float offset : {-ts, ts}) {
            if (isAreaFree(warp.destX + offset, warp.destY, partner.width(), partner.height())) {
                partnerX = warp.destX + offset;
                break;
            }
        }
        partner.spawn(partnerX, warp.destY);
    }
    cameraSnapPending_ = true;
}

void Game::checkBrickBlockHit(Mario& character) {
    setScorer(character);
    int row = 0, col0 = 0, col1 = 0;
    if (!character.headBumpedThisFrame(row, col0, col1)) return;

    for (int col = col0; col <= col1; ++col) {
        if (!level_.isBrickTile(col, row)) continue;
        // Chico: el golpe ya freno el salto en moveAndCollide(); esto solo
        // agrega la sacudida visual, no rompe el tile.
        // Golpe "sordo" de cualquier bloque desde abajo: suena siempre,
        // ademas del sonido especifico de lo que pase despues (romperse,
        // dar un item) -- pedido del usuario.
        audio_.play(Sfx::Bump);
        if (character.size() == MarioSize::Small) {
            bumpBrickBlock(col, row);
        } else {
            breakBrickBlock(col, row);
        }
        collectTopCoinIfAny(col, row);
        return;
    }
}

void Game::collectTopCoinIfAny(int col, int row) {
    if (!level_.hasCoinOnTop(col, row)) return;
    level_.collectTopCoin(col, row);
    collectCoin();
    const int ts = level_.tileSize();
    const float x = static_cast<float>(col * ts) +
                    (static_cast<float>(ts) - static_cast<float>(BlockCoin::kWidth)) / 2.0f;
    const float y = static_cast<float>(row * ts) - static_cast<float>(BlockCoin::kHeight);
    blockCoin_.spawn(x, y);
    audio_.play(Sfx::Coin);
}

void Game::breakBrickBlock(int col, int row) {
    level_.breakTile(col, row);
    addScore(50);
    audio_.play(Sfx::Brick);

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
    for (Goomba& goomba : goombas_) {
        if (goomba.isDangerous() && onTop(goomba.x(), goomba.y(), Goomba::kSize, Goomba::kSize)) {
            killEnemyFalling(goomba, true);
        }
    }
    for (Koopa* koopa : allKoopas()) {
        if (koopa->isDangerous() && onTop(koopa->x(), koopa->y(), koopa->width(), koopa->height())) {
            koopa->killFalling(true);
        }
    }
    for (BuzzyBeetle& buzzy : buzzyBeetles_) {
        if (buzzy.isDangerous() && onTop(buzzy.x(), buzzy.y(), BuzzyBeetle::kSize, BuzzyBeetle::kSize)) {
            killEnemyFalling(buzzy, true);
        }
    }
    for (HammerBro& bro : hammerBros_) {
        if (bro.isDangerous() && onTop(bro.x(), bro.y(), HammerBro::kWidth, HammerBro::kHeight)) {
            killEnemyFalling(bro, true);
        }
    }
    for (Spiny& spiny : spinies_) {
        if (spiny.isDangerous() && onTop(spiny.x(), spiny.y(), Spiny::kSize, Spiny::kSize)) {
            killEnemyFalling(spiny, true);
        }
    }
}

BlockBump* Game::startBlockBump(int col, int row) {
    // Si esta misma celda ya esta sacudiendose, reinicia el timer en vez de
    // agregar una segunda (un solo golpe a la vez tiene sentido visualmente).
    for (BlockBump& bump : blockBumps_) {
        if (bump.active && bump.col == col && bump.row == row) {
            bump.timer = kBumpDuration;
            return &bump;
        }
    }
    for (BlockBump& bump : blockBumps_) {
        if (bump.active) continue;
        bump.active = true;
        bump.col = col;
        bump.row = row;
        bump.timer = kBumpDuration;
        bump.hasPendingItem = false;
        bump.pendingMovingRight = false;
        return &bump;
    }
    // Pool lleno (con kMaxBlockBumps=2 y un solo Mario/Luigi a la vez
    // golpeando bloques, no debería pasar): se pierde esta sacudida, no es
    // grave, solo visual.
    return nullptr;
}

void Game::bumpBrickBlock(int col, int row) {
    killEnemyOnTopOfTile(col, row);
    startBlockBump(col, row);
}

void Game::checkQuestionBlockHit(Mario& character) {
    setScorer(character);
    int row = 0, col0 = 0, col1 = 0;
    if (!character.headBumpedThisFrame(row, col0, col1)) return;

    for (int col = col0; col <= col1; ++col) {
        if (level_.isHiddenBlockTile(col, row)) level_.revealHiddenBlock(col, row);
        const bool isQuestion = level_.isQuestionTile(col, row);
        // Ladrillo dispensador ('D', ver Tilemap::isDispenserTile): misma
        // logica de golpe/item que el bloque de interrogacion, solo cambia
        // el sprite (se dibuja como ladrillo normal) y de donde saca el
        // item (mismo campo "blocks" del nivel, ver findBlockSpawn).
        const bool isDispenser = level_.isDispenserTile(col, row);
        if (!isQuestion && !isDispenser) continue;
        // Golpe "sordo" de cualquier bloque desde abajo (ver
        // checkBrickBlockHit): suena SIEMPRE, incluso ya usado/gastado (a
        // diferencia de la sacudida/item, que se cortan del todo una vez
        // usado, ver mas abajo) -- pedido del usuario.
        audio_.play(Sfx::Bump);
        const bool used = isQuestion ? level_.isQuestionUsed(col, row) : level_.isDispenserUsed(col, row);
        // Una vez usado (ya tiro su ultimo item) deja de reaccionar del
        // todo (nada de sacudida ni item) -- a pedido del usuario, distinto
        // de como arranco esto (que se sacudia siempre, este comentario
        // viejo asumia que asi era en el original).
        if (used) return;
        BlockBump* bump = startBlockBump(col, row);
        // maxHits (ver QuestionBlockSpawn): 1 en casi todos, pero un bloque
        // "de N monedas" (campo "hits" del bloque en el nivel, item Coin)
        // sigue reaccionando hasta agotar sus golpes -- recien en el ultimo
        // se marca usado para siempre (mismo criterio que antes para
        // maxHits=1: el primer golpe YA es el ultimo).
        const int maxHits = findBlockSpawn(col, row).maxHits;
        const int hitsSoFar = isQuestion ? level_.questionHitCount(col, row) : level_.dispenserHitCount(col, row);
        if (isQuestion) {
            level_.incrementQuestionHit(col, row);
            if (hitsSoFar + 1 >= maxHits) level_.markQuestionUsed(col, row);
        } else {
            level_.incrementDispenserHit(col, row);
            if (hitsSoFar + 1 >= maxHits) level_.markDispenserUsed(col, row);
        }
        killEnemyOnTopOfTile(col, row);
        // Estilo Super Mario Bros. 3: el item sale hacia el lado OPUESTO
        // de donde vino el golpe (si el personaje esta a la izquierda
        // del centro del bloque, el item sale hacia la derecha, y
        // viceversa) -- no siempre a la derecha como en el original de
        // 1985.
        const float blockCenterX = static_cast<float>(col * level_.tileSize()) +
                                    static_cast<float>(level_.tileSize()) / 2.0f;
        const float characterCenterX = character.x() + static_cast<float>(character.width()) / 2.0f;
        const bool movingRight = characterCenterX < blockCenterX;
        // El item recien sale cuando termina de sacudirse (ver el update()
        // del pool de blockBumps_), no al toque -- a pedido del usuario. Si
        // el pool estaba lleno (startBlockBump devolvio nullptr, no deberia
        // pasar con kMaxBlockBumps=2 y un golpe genuino a la vez) sale de
        // una en vez de perderse.
        if (bump != nullptr) {
            bump->hasPendingItem = true;
            bump->pendingMovingRight = movingRight;
            bump->pendingHitterBig = character.size() != MarioSize::Small;
            bump->hitter = scorer_;
        } else {
            spawnItemFromQuestionBlock(col, row, movingRight, character.size() != MarioSize::Small);
        }
        return;
    }
}

void Game::spawnItemFromQuestionBlock(int col, int row, bool movingRight, bool hitterBig) {
    BlockItem item = findBlockSpawn(col, row).item;
    if (item == BlockItem::PowerUp) item = hitterBig ? BlockItem::FireFlower : BlockItem::Mushroom;

    const int ts = level_.tileSize();
    const float blockX = static_cast<float>(col * ts);
    const float blockTopY = static_cast<float>(row * ts);

    // BlockCoin es aparte (ver el comentario de la clase): tiene su propio
    // salto en vez de pasar por itemEmerge_, y ademas suma al contador --
    // no es la Coin normal (esa va quieta puesta en el piso del escenario,
    // ver kCoinTestX/Y).
    if (item == BlockItem::Coin) {
        collectCoin();
        blockCoin_.spawn(blockX + (static_cast<float>(ts) - static_cast<float>(BlockCoin::kWidth)) / 2.0f,
                          blockTopY);
        audio_.play(Sfx::Coin);
        return;
    }
    audio_.play(Sfx::Item);

    // Mushroom/FireFlower/Star: ninguno tiene animacion de emerger propia
    // (ningun otro item/enemigo del elenco la tiene tampoco), asi que
    // itemEmerge_ la simula ella misma en Game::update -- arranca a la
    // altura del bloque (tapado por el Z-order, ver el comentario de
    // ItemEmerge) y sube hasta su altura final de descanso.
    itemEmerge_.active = true;
    itemEmerge_.item = item;
    itemEmerge_.x = blockX;
    itemEmerge_.startY = blockTopY;
    itemEmerge_.timer = kItemEmergeDuration;
    itemEmerge_.movingRight = movingRight;
    switch (item) {
        case BlockItem::Mushroom:
            itemEmerge_.endY = blockTopY - static_cast<float>(Mushroom::kSize);
            mushroom_.spawn(itemEmerge_.x, itemEmerge_.startY, itemEmerge_.movingRight);
            break;
        case BlockItem::OneUp:
            // Mismo hongo, misma fisica (ver Mushroom::isOneUp): solo cambia
            // el sprite y el efecto al agarrarlo (checkMushroomPickup).
            itemEmerge_.endY = blockTopY - static_cast<float>(Mushroom::kSize);
            mushroom_.spawn(itemEmerge_.x, itemEmerge_.startY, itemEmerge_.movingRight, /*oneUp=*/true);
            break;
        case BlockItem::FireFlower:
            itemEmerge_.endY = blockTopY - static_cast<float>(FireFlower::kSize);
            fireFlower_.spawn(itemEmerge_.x, itemEmerge_.startY);
            break;
        case BlockItem::Star:
            itemEmerge_.endY = blockTopY - static_cast<float>(StarItem::kSize);
            star_.spawn(itemEmerge_.x, itemEmerge_.startY, itemEmerge_.movingRight);
            break;
        case BlockItem::Coin:
        case BlockItem::PowerUp:
            break;  // ya se resolvieron arriba, no llegan aca
    }
}

void Game::spawnScorePopup(float x, float y, const std::string& text) {
    // Los popups numericos son puntos de verdad ("1UP" no): se suman aca
    // para no repetir el valor en cada lugar que muestra uno.
    if (!text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        addScore(std::atoi(text.c_str()));
    }
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
            // El piso mas bajo de la columna con lugar libre arriba para el
            // personaje: buscando de abajo hacia arriba (de arriba hacia
            // abajo, en un subterraneo encontraba primero el techo y lo
            // dejaba parado encima). Si no hay ninguno, la columna es un
            // pozo y se sigue buscando hacia el costado.
            const int rowsNeeded = (characterHeight + ts - 1) / ts;
            for (int row = level_.heightTiles() - 1; row >= rowsNeeded; --row) {
                if (!level_.isSolidTile(col, row)) continue;
                bool roomAbove = true;
                for (int k = 1; k <= rowsNeeded; ++k) {
                    if (level_.isSolidTile(col, row - k)) roomAbove = false;
                }
                if (!roomAbove) continue;
                return SDL_Point{col * ts, row * ts - characterHeight};
            }
        }
    }
    // No deberia pasar (implicaria un nivel sin piso en ningun lado).
    return SDL_Point{static_cast<int>(kMarioSpawnX), static_cast<int>(kMarioSpawnY)};
}

void Game::respawnNearestPlatform(Mario& character) {
    // Preferentemente en el centro de la pantalla (en coordenadas de nivel:
    // hay que sumarle cuanto scrolleo la camara, si no siempre apunta al
    // centro de la PRIMERA pantalla aunque ya se haya avanzado de ahi), no
    // en donde murio.
    const SDL_Point point = findRespawnPoint(cameraX_ + gLogicalWidth / 2.0f, character.height());
    character.spawn(static_cast<float>(point.x), static_cast<float>(point.y));
    grantRespawnInvincibilityForWorld(character);
}

void Game::placeCharactersAtStart() {
    if (checkpointCol_ < 0) {
        // Parados sobre el piso de entrada (no cayendo desde arriba).
        const SDL_Point marioPoint = findRespawnPoint(kMarioSpawnX + 1.0f, mario_.height());
        mario_.spawn(static_cast<float>(marioPoint.x), static_cast<float>(marioPoint.y));
        const SDL_Point luigiPoint = findRespawnPoint(kLuigiSpawnX + 1.0f, luigi_.height());
        luigi_.spawn(static_cast<float>(luigiPoint.x), static_cast<float>(luigiPoint.y));
    } else {
        // Parados sobre el piso de la columna del checkpoint (o la mas
        // cercana con piso, ver findRespawnPoint), Luigi un poco atras.
        const float ts = static_cast<float>(level_.tileSize());
        const float x = static_cast<float>(checkpointCol_) * ts + ts / 2.0f;
        const SDL_Point marioPoint = findRespawnPoint(x, mario_.height());
        mario_.spawn(static_cast<float>(marioPoint.x), static_cast<float>(marioPoint.y));
        const SDL_Point luigiPoint = findRespawnPoint(x - 1.5f * ts, luigi_.height());
        luigi_.spawn(static_cast<float>(luigiPoint.x), static_cast<float>(luigiPoint.y));
    }
    // Puede quedar lejos de la camara actual: si se acercara de a poco,
    // keepCharactersOnScreen() lo arrastraria hacia la pantalla vieja.
    cameraSnapPending_ = true;
}

void Game::grantRespawnInvincibilityForWorld(Mario& character) {
    if (character.worldType() == WorldType::Underwater) {
        // Bajo el agua Mario nada mucho mas lento que corriendo en tierra
        // (Mario::kSwimMaxSpeed vs kMaxRunSpeed) -- con la duracion normal
        // (2s) no le alcanza para alejarse de un enemigo antes de que se le
        // acabe la invulnerabilidad, y muere de nuevo apenas se corta el
        // parpadeo si sigue pegado (reportado por el usuario). El doble de
        // tiempo le da margen real para nadar lejos.
        character.grantRespawnInvincibility(kUnderwaterRespawnInvincibleDuration);
    } else {
        character.grantRespawnInvincibility();
    }
}

void Game::updateCamera(float dt) {
    // 1P: centrada en Mario. 2P: centrada en el punto medio entre los dos,
    // asi los dos entran en pantalla mientras esten a menos de un ancho de
    // pantalla (keepCharactersOnScreen() se encarga de que nunca se separen
    // mas que eso). El que esta cayendo muerto no cuenta: la camara sigue
    // solo al que quedo vivo hasta que el otro reaparece.
    // Tampoco cuenta el que ya se bajo de la bandera y camina al castillo
    // (o ya entro): sale de pantalla a proposito.
    auto followable = [](const Mario& character) {
        return !character.isInDeathSequence() && (!character.isOnFlagpole() || character.isHoldingFlagpole());
    };
    const bool marioActive = followable(mario_);
    const bool luigiActive = twoPlayerMode_ && followable(luigi_);
    float focusX = mario_.x();
    if (marioActive && luigiActive) {
        focusX = (mario_.x() + luigi_.x()) / 2.0f;
    } else if (luigiActive) {
        focusX = luigi_.x();
    }
    // Sin mostrar mas alla de los bordes del nivel -- si el nivel entero
    // entra en una pantalla (los escenarios viejos, sin scroll),
    // maxCameraX da <= 0 y se clampea a 0 siempre.
    // Solo dentro del area donde esta el foco (ver Tilemap::areas): un
    // cuarto bonus pegado al nivel no se asoma por el borde de la pantalla.
    const int areaIndex = areaIndexAt(focusX);
    const LevelArea& area = level_.areas()[static_cast<size_t>(areaIndex)];
    const float ts = static_cast<float>(level_.tileSize());
    const float areaLeft = static_cast<float>(area.startCol) * ts;
    const float areaWidth = static_cast<float>(area.endCol - area.startCol + 1) * ts;
    // Si el area entra entera en pantalla (los escenarios de prueba de una
    // sola pantalla de 256px, el cuarto bonus) queda centrada: la camara
    // arranca antes del area y lo que sobra a los costados lo tapa
    // renderOutsideAreaMask.
    const float minCameraX = areaWidth < static_cast<float>(gLogicalWidth)
        ? areaLeft - (static_cast<float>(gLogicalWidth) - areaWidth) / 2.0f
        : areaLeft;
    const float maxCameraX = std::max(minCameraX, areaLeft + areaWidth - static_cast<float>(gLogicalWidth));
    if (areaIndex != currentArea_) {
        // Recien entro (o volvio) a esta area: cambia el tipo de mundo
        // (fondo/paletas) y la camara salta directo, sin barrido.
        currentArea_ = areaIndex;
        worldType_ = parseWorldType(area.world, levelWorldType_);
        mario_.setWorldType(worldType_);
        luigi_.setWorldType(worldType_);
        cameraSnapPending_ = true;
    }
    const float targetX = std::clamp(focusX - static_cast<float>(gLogicalWidth) / 2.0f, minCameraX, maxCameraX);
    const float previousCameraX = cameraX_;
    if (!twoPlayerMode_ || cameraSnapPending_) {
        cameraX_ = targetX;
    } else {
        // En 2P el objetivo salta de golpe cuando uno muere (pasa a seguir
        // solo al otro) o reaparece: se acerca a velocidad limitada en vez
        // de cortar de un cuadro al otro. kCameraCatchUpSpeed es mas del
        // doble de kMaxRunSpeed, asi que siguiendo normal nunca se queda
        // atras.
        const float step = kCameraCatchUpSpeed * dt;
        cameraX_ += std::clamp(targetX - cameraX_, -step, step);
    }
    cameraSnapPending_ = false;
    // Ver el comentario de cameraScrollSpeed_ en Game.h (equivalente a
    // ScrollAmount en el original): 0 en vez de un valor real cuando la
    // camara esta pegada a un borde del nivel (clampeada), aunque el
    // personaje se siga moviendo -- ahi no esta "empujando" la camara de
    // verdad aunque corra.
    cameraScrollSpeed_ = dt > 0.0f ? (cameraX_ - previousCameraX) / dt : 0.0f;
}

void Game::keepCharactersOnScreen() {
    if (!twoPlayerMode_) return;
    for (Mario* character : {&mario_, &luigi_}) {
        // El que cae muerto sale de pantalla a proposito (y despues
        // reaparece), no se lo frena.
        if (character->isInDeathSequence()) continue;
        // Idem el que esta en la secuencia de la bandera (la camara ya lo
        // tenia en pantalla al agarrarla, y despues se va al castillo).
        if (character->isOnFlagpole()) continue;
        const float minX = cameraX_;
        const float maxX = cameraX_ + static_cast<float>(gLogicalWidth - character->width());
        if (character->x() < minX) {
            character->nudgeX(minX - character->x());
            if (character->velocityX() < 0.0f) character->setVelocityX(0.0f);
        } else if (character->x() > maxX) {
            character->nudgeX(maxX - character->x());
            if (character->velocityX() > 0.0f) character->setVelocityX(0.0f);
        }
    }
}

bool Game::isAreaFree(float x, float y, int width, int height) const {
    const int ts = level_.tileSize();
    const int col0 = static_cast<int>(std::floor(x / ts));
    const int col1 = static_cast<int>(std::floor((x + width - 1.0f) / ts));
    const int row0 = static_cast<int>(std::floor(y / ts));
    const int row1 = static_cast<int>(std::floor((y + height - 1.0f) / ts));
    if (col0 < 0 || col1 >= level_.widthTiles()) return false;
    for (int col = col0; col <= col1; ++col) {
        for (int row = row0; row <= row1; ++row) {
            if (level_.isSolidTile(col, row)) return false;
        }
    }
    return true;
}

void Game::update(float dt) {
    if (appState_ == AppState::LevelIntro || appState_ == AppState::GameOver) {
        appStateTimer_ -= dt;
        if (appStateTimer_ > 0.0f) return;
        if (appState_ == AppState::LevelIntro) {
            appState_ = AppState::Playing;
            // Lo apretado durante la pantalla negra no cuenta.
            input1_.clearPresses();
            input2_.clearPresses();
        } else {
            // GAME OVER: de cero; en una partida, desde el primer nivel del
            // mundo (seccion 6).
            if (campaign_) {
                campaignLevel_ = 1;
                customLevelPath_ = campaignLevelPath(campaignWorld_, campaignLevel_);
            }
            startScenario(scenario_);
        }
        return;
    }
    if (appState_ != AppState::Playing) return;
    updateRumble();
    updateMusic(dt);
    if (pipeTransitionTimer_ > 0.0f) {
        // Intro: Mario ya se metio en la tuberia, todo quieto hasta que
        // termina el jingle.
        pipeTransitionTimer_ -= dt;
        if (pipeTransitionTimer_ <= 0.0f) finishIntroPipe();
        return;
    }
    if (bridgeCollapsing_) {
        updateBridgeCollapse(dt);
        return;
    }
    if (pipeAnim_.active) {
        updatePipeAnim(dt);
        updateCamera(dt);
        return;
    }

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
        mario_.consumeDeathFinished();
        if (twoPlayerMode_) {
            luigi_.updateDamageSequence(dt);
            luigi_.updateGrowSequence(dt);
            luigi_.updateDeathSequence(dt);
            luigi_.consumeDeathFinished();
        }
        // Recien cuando terminaron TODAS las secuencias de muerte (en 2P
        // uno pudo haber muerto un poco despues que el otro).
        const bool stillDying = mario_.isInDeathSequence() || (twoPlayerMode_ && luigi_.isInDeathSequence());
        if (everyoneDying && !stillDying) onAllPlayersDied();
        return;
    }

    updateLevelTimer(dt);
    updatePlatforms(dt);

    // Guardados de antes de mover: para saber si un enemigo lo pisa "de
    // arriba" hace falta ver si los pies YA estaban por encima antes de
    // este frame, no solo cuanto se superponen despues de moverse (eso se
    // presta a confusion con golpes en diagonal o caidas rapidas que saltan
    // varios pixeles en un solo frame -- ver checkGoombaCollision/checkKoopaCollision).
    const float marioPrevY = mario_.y();
    const float luigiPrevY = luigi_.y();

    mario_.update(dt, input1_, level_);
    checkBrickBlockHit(mario_);
    checkQuestionBlockHit(mario_);
    checkPipeWarp(mario_, input1_);
    checkSideWarp(mario_);
    if (mario_.consumeJumpRequest()) {
        audio_.play(mario_.size() == MarioSize::Small ? Sfx::JumpSmall : Sfx::Jump);
    }
    // stompswim.wav (Sfx::StompKill) es el mismo sonido para pisar/matar un
    // enemigo Y cada brazada nadando (ver el comentario en Game::init()) --
    // esta es la parte de nadar, no implementada hasta ahora.
    if (mario_.consumeSwimStrokeSoundRequest()) audio_.play(Sfx::StompKill);
    // Solo se llega aca muriendo en 2P con el otro todavia vivo (en 1P o en
    // "murieron los dos" ya se atendio arriba): reaparece en la plataforma
    // mas cercana en vez del spawn del nivel, sin pausar nada.
    if (mario_.consumeDeathFinished()) {
        respawnNearestPlatform(mario_);
    }

    if (twoPlayerMode_) {
        luigi_.update(dt, input2_, level_);
        checkBrickBlockHit(luigi_);
        checkQuestionBlockHit(luigi_);
        checkPipeWarp(luigi_, input2_);
        checkSideWarp(luigi_);
        if (luigi_.consumeJumpRequest()) {
            audio_.play(luigi_.size() == MarioSize::Small ? Sfx::JumpSmall : Sfx::Jump);
        }
        if (luigi_.consumeSwimStrokeSoundRequest()) audio_.play(Sfx::StompKill);
        if (luigi_.consumeDeathFinished()) {
            respawnNearestPlatform(luigi_);
        }
        resolveCharacterCollision();
    }

    updatePlatformRiders(mario_, marioPlatform_);
    if (twoPlayerMode_) updatePlatformRiders(luigi_, luigiPlatform_);
    checkPitDeath(mario_);
    if (twoPlayerMode_) checkPitDeath(luigi_);
    checkLavaDeath(mario_);
    if (twoPlayerMode_) checkLavaDeath(luigi_);
    updateCheckpoints();

    updateCamera(dt);
    keepCharactersOnScreen();
    keepCharacterInsideArea(mario_);
    if (twoPlayerMode_) keepCharacterInsideArea(luigi_);
    spawnLevelEntities();

    // Emergiendo de un bloque (ver ItemEmerge en Game.h): la reposiciona a
    // mano interpolando startY->endY en vez de dejarla actualizar su propia
    // fisica -- recien cuando termina la ventana vuelve a caminar/animarse
    // sola mas abajo.
    if (itemEmerge_.active) {
        itemEmerge_.timer -= dt;
        const float progress = std::clamp(1.0f - itemEmerge_.timer / kItemEmergeDuration, 0.0f, 1.0f);
        const float currentY = itemEmerge_.startY + (itemEmerge_.endY - itemEmerge_.startY) * progress;
        switch (itemEmerge_.item) {
            case BlockItem::Mushroom:
                mushroom_.spawn(itemEmerge_.x, currentY, itemEmerge_.movingRight);
                break;
            case BlockItem::OneUp:
                mushroom_.spawn(itemEmerge_.x, currentY, itemEmerge_.movingRight, /*oneUp=*/true);
                break;
            case BlockItem::FireFlower:
                fireFlower_.spawn(itemEmerge_.x, currentY);
                break;
            case BlockItem::Star:
                star_.spawn(itemEmerge_.x, currentY, itemEmerge_.movingRight);
                break;
            case BlockItem::Coin:
                break;  // BlockCoin no pasa por aca, ver spawnItemFromQuestionBlock
        }
        if (itemEmerge_.timer <= 0.0f) {
            itemEmerge_.active = false;
        }
    }

    // OneUp comparte la misma instancia mushroom_ (ver Mushroom::isOneUp),
    // asi que tiene que frenar update()/checkMushroomPickup mientras emerge
    // igual que Mushroom -- si no, se lo agarra (y desaparece) apenas
    // aparece, antes de terminar de salir del bloque.
    if (!(itemEmerge_.active &&
          (itemEmerge_.item == BlockItem::Mushroom || itemEmerge_.item == BlockItem::OneUp))) {
        mushroom_.update(dt, level_, level_.widthPixels());
        checkMushroomPickup(mario_);
        if (twoPlayerMode_) checkMushroomPickup(luigi_);
    }
    cullIfLeftBehindCollect(mushroom_, static_cast<float>(Mushroom::kSize), cameraX_);

    if (!(itemEmerge_.active && itemEmerge_.item == BlockItem::FireFlower)) {
        fireFlower_.update(dt);
        checkFireFlowerPickup(mario_);
        if (twoPlayerMode_) checkFireFlowerPickup(luigi_);
    }

    if (!(itemEmerge_.active && itemEmerge_.item == BlockItem::Star)) {
        star_.update(dt, level_, level_.widthPixels());
        checkStarPickup(mario_);
        if (twoPlayerMode_) checkStarPickup(luigi_);
    }
    cullIfLeftBehindCollect(star_, static_cast<float>(StarItem::kSize), cameraX_);

    coin_.update(dt);
    checkCoinPickup(mario_);
    if (twoPlayerMode_) checkCoinPickup(luigi_);
    checkCoinTiles(mario_);
    if (twoPlayerMode_) checkCoinTiles(luigi_);

    blockCoin_.update(dt);

    for (Goomba& goomba : goombas_) {
        goomba.update(dt, level_, kWorldViewBottom);
        checkGoombaCollision(mario_, goomba, marioPrevY);
        if (twoPlayerMode_) checkGoombaCollision(luigi_, goomba, luigiPrevY);
        cullIfLeftBehindReset(goomba, static_cast<float>(Goomba::kSize), cameraX_);
    }

    for (BuzzyBeetle& buzzy : buzzyBeetles_) {
        buzzy.update(dt, level_, kWorldViewBottom);
        checkBuzzyBeetleCollision(mario_, buzzy, marioPrevY);
        if (twoPlayerMode_) checkBuzzyBeetleCollision(luigi_, buzzy, luigiPrevY);
        for (Goomba& goomba : goombas_) {
            checkBuzzyShellVsGoomba(buzzy, goomba);
            checkGoombaBuzzyBeetleBounce(goomba, buzzy);
        }
        cullIfLeftBehindReset(buzzy, static_cast<float>(BuzzyBeetle::kSize), cameraX_);
    }

    for (PiranhaPlant& plant : piranhaPlants_) {
        plant.update(dt, isSafeForPiranhaPlant(plant));
        checkPiranhaPlantCollision(mario_, plant);
        if (twoPlayerMode_) checkPiranhaPlantCollision(luigi_, plant);
        cullIfLeftBehindReset(plant, static_cast<float>(PiranhaPlant::kWidth), cameraX_);
    }

    for (HammerBro& bro : hammerBros_) {
        bro.update(dt, level_, kWorldViewBottom);
        // Mira al personaje mas cercano (ver el comentario de faceTowards),
        // independiente de hacia donde este caminando.
        if (twoPlayerMode_ && std::abs(luigi_.x() - bro.x()) < std::abs(mario_.x() - bro.x())) {
            bro.faceTowards(luigi_.x());
        } else {
            bro.faceTowards(mario_.x());
        }
        checkHammerBroCollision(mario_, bro, marioPrevY);
        if (twoPlayerMode_) checkHammerBroCollision(luigi_, bro, luigiPrevY);
        if (bro.consumeThrowRequest()) {
            for (Hammer& hammer : hammers_) {
                if (hammer.isAlive()) continue;
                hammer.spawn(bro.throwOriginX(), bro.throwOriginY(), bro.throwVelocityX(), bro.throwVelocityY());
                break;
            }
        }
        cullIfLeftBehindReset(bro, static_cast<float>(HammerBro::kWidth), cameraX_);
    }
    for (Hammer& hammer : hammers_) {
        hammer.update(dt, level_, level_.widthPixels(), kWorldViewBottom);
        checkHammerCollision(mario_, hammer);
        if (twoPlayerMode_) checkHammerCollision(luigi_, hammer);
        cullIfLeftBehindReset(hammer, static_cast<float>(Hammer::kSize), cameraX_);
    }

    // Mismo criterio que HammerBro::faceTowards: sigue/mira al personaje
    // mas cercano, esta clase tampoco conoce a Mario/Luigi.
    const float lakituTargetX = (twoPlayerMode_ && std::abs(luigi_.x() - lakitu_.x()) < std::abs(mario_.x() - lakitu_.x()))
                                     ? luigi_.x()
                                     : mario_.x();
    lakitu_.update(dt, lakituTargetX, cameraScrollSpeed_, level_.widthPixels(), kWorldViewBottom);
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
        spiny.update(dt, level_, kWorldViewBottom);
        checkSpinyCollision(mario_, spiny);
        if (twoPlayerMode_) checkSpinyCollision(luigi_, spiny);
        cullIfLeftBehindReset(spiny, static_cast<float>(Spiny::kSize), cameraX_);
    }
    for (size_t i = 0; i < spinies_.size(); ++i) {
        for (size_t j = i + 1; j < spinies_.size(); ++j) {
            checkSpinyBounce(spinies_[i], spinies_[j]);
        }
    }

    for (Blooper& blooper : bloopers_) {
        // Mismo criterio que Lakitu (targetX): apunta al personaje mas
        // cercano, esta clase tampoco conoce a Mario/Luigi. Ademas necesita
        // Y (el chorro apunta en las dos direcciones, no solo en X).
        const bool luigiCloser = twoPlayerMode_ &&
            std::hypot(luigi_.x() - blooper.x(), luigi_.y() - blooper.y()) <
                std::hypot(mario_.x() - blooper.x(), mario_.y() - blooper.y());
        const float blooperTargetX = luigiCloser ? luigi_.x() : mario_.x();
        const float blooperTargetY = luigiCloser ? luigi_.y() : mario_.y();
        blooper.update(dt, level_, blooperTargetX, blooperTargetY, kWorldViewBottom);
        checkBlooperCollision(mario_, blooper);
        if (twoPlayerMode_) checkBlooperCollision(luigi_, blooper);
        cullIfLeftBehindReset(blooper, static_cast<float>(Blooper::kWidth), cameraX_);
    }

    cheepCheepSwim_.update(dt, level_.widthPixels(), kWorldViewBottom);
    checkCheepCheepCollision(mario_, cheepCheepSwim_, marioPrevY);
    if (twoPlayerMode_) checkCheepCheepCollision(luigi_, cheepCheepSwim_, luigiPrevY);
    cullIfLeftBehindReset(cheepCheepSwim_, static_cast<float>(CheepCheep::kSize), cameraX_);
    cheepCheepSwimOsc_.update(dt, level_.widthPixels(), kWorldViewBottom);
    checkCheepCheepCollision(mario_, cheepCheepSwimOsc_, marioPrevY);
    if (twoPlayerMode_) checkCheepCheepCollision(luigi_, cheepCheepSwimOsc_, luigiPrevY);
    cullIfLeftBehindReset(cheepCheepSwimOsc_, static_cast<float>(CheepCheep::kSize), cameraX_);

    for (CheepCheep& fish : levelCheeps_) {
        fish.update(dt, level_.widthPixels(), kWorldViewBottom);
        checkCheepCheepCollision(mario_, fish, marioPrevY);
        if (twoPlayerMode_) checkCheepCheepCollision(luigi_, fish, luigiPrevY);
        cullIfLeftBehindReset(fish, static_cast<float>(CheepCheep::kSize), cameraX_);
    }

    // Cheep Cheep saltarinas: en todo el escenario de prueba, o en un nivel
    // mientras el personaje esta dentro de una zona "cheep_jump".
    if (scenario_ == TestScenario::CheepCheepJump || inCheepJumpZone()) {
        cheepCheepJumpSpawnTimer_ -= dt;
        if (cheepCheepJumpSpawnTimer_ <= 0.0f) {
            spawnNextCheepCheepJump();
            cheepCheepJumpSpawnTimer_ = randomRange(kCheepCheepJumpIntervalMin, kCheepCheepJumpIntervalMax);
        }
    }
    for (CheepCheep& fish : cheepCheepJumps_) {
        fish.update(dt, level_.widthPixels(), kWorldViewBottom);
        checkCheepCheepCollision(mario_, fish, marioPrevY);
        if (twoPlayerMode_) checkCheepCheepCollision(luigi_, fish, luigiPrevY);
        cullIfLeftBehindReset(fish, static_cast<float>(CheepCheep::kSize), cameraX_);
    }

    updateCannons(dt);
    for (BulletBill& bill : bulletBills_) {
        bill.update(dt, level_.widthPixels(), kWorldViewBottom);
        checkBulletBillCollision(mario_, bill, marioPrevY);
        if (twoPlayerMode_) checkBulletBillCollision(luigi_, bill, luigiPrevY);
        // Los que se alejan de la pantalla (para cualquier lado) se apagan.
        if (bill.isAlive() && (bill.x() < cameraX_ - 64.0f || bill.x() > cameraX_ + gLogicalWidth + 64.0f)) {
            bill.reset();
        }
    }

    for (Podoboo& podoboo : podoboos_) {
        podoboo.update(dt);
        checkPodobooCollision(mario_, podoboo);
        if (twoPlayerMode_) checkPodobooCollision(luigi_, podoboo);
    }
    updateFirebars(dt);
    updateLakituZone();

    // Mismo criterio que HammerBro::faceTowards: apunta al personaje mas
    // cercano, esta clase tampoco conoce a Mario/Luigi -- independiente de
    // hacia donde este caminando (ver el comentario de Bowser.h).
    bowser_.update(dt, kWorldViewBottom);
    if (twoPlayerMode_ && std::abs(luigi_.x() - bowser_.x()) < std::abs(mario_.x() - bowser_.x())) {
        bowser_.faceTowards(luigi_.x());
    } else {
        bowser_.faceTowards(mario_.x());
    }
    checkBowserCollision(mario_);
    if (twoPlayerMode_) checkBowserCollision(luigi_);
    if (bowser_.consumeFireRequest()) {
        for (BowserFire& fire : bowserFires_) {
            if (fire.isAlive()) continue;
            fire.spawn(bowser_.fireOriginX(), bowser_.fireOriginY(), bowser_.fireVelocityX(), bowser_.fireMaxY());
            audio_.play(Sfx::BowserFire);
            break;
        }
    }
    if (bowser_.consumeHammerThrowRequest()) {
        // Mismo pool que HammerBro (hammers_): el update()/checkHammerCollision
        // de mas abajo ya recorre este pool sin importar quien tiro cada uno.
        for (Hammer& hammer : hammers_) {
            if (hammer.isAlive()) continue;
            hammer.spawn(bowser_.hammerOriginX(), bowser_.hammerOriginY(), bowser_.hammerVelocityX(),
                         bowser_.hammerVelocityY());
            break;
        }
    }
    for (BowserFire& fire : bowserFires_) {
        fire.update(dt, level_.widthPixels(), kWorldViewBottom);
        checkBowserFireCollision(mario_, fire);
        if (twoPlayerMode_) checkBowserFireCollision(luigi_, fire);
        cullIfLeftBehindReset(fire, static_cast<float>(BowserFire::kWidth), cameraX_);
    }
    checkAxeCollision(mario_, axe_);
    if (twoPlayerMode_) checkAxeCollision(luigi_, axe_);
    checkFlagpoleGrab(mario_);
    if (twoPlayerMode_) checkFlagpoleGrab(luigi_);
    updateFlagpoleSequence(dt);
    updateLevelClear(dt);

    // Los 5 Koopa de prueba (verde piso x2 + verde Alado saltarin + rojo
    // piso + rojo Alado volador -- ver KoopaState/KoopaColor, solo estas dos
    // combinaciones de Alado existen en el juego original de 1985): en
    // array solo para no tener que escribir a mano todas las combinaciones
    // de cada chequeo de a pares (caparazon-vs-caparazon, rebote-vs-Goomba)
    // cada vez que se agrega una instancia de prueba mas.
    const std::vector<Koopa*> koopas = allKoopas();
    for (Koopa* koopa : koopas) {
        koopa->update(dt, level_, kLogicalHeight);
        // Solo el caparazon en movimiento rebotando contra una pared suena
        // (caminando/saltando tambien rebotan, pero en silencio, como en el
        // original) -- pedido del usuario.
        if (koopa->consumeWallBounceRequest() && koopa->isMovingShell()) {
            audio_.play(Sfx::Bump);
        }
        checkKoopaCollision(mario_, *koopa, marioPrevY);
        if (twoPlayerMode_) checkKoopaCollision(luigi_, *koopa, luigiPrevY);
        for (Goomba& goomba : goombas_) {
            checkShellVsGoomba(*koopa, goomba);
            checkGoombaKoopaBounce(goomba, *koopa);
        }
        for (BuzzyBeetle& buzzy : buzzyBeetles_) {
            checkShellVsBuzzyBeetle(*koopa, buzzy);
            checkKoopaBuzzyBeetleBounce(*koopa, buzzy);
        }
        cullIfLeftBehindReset(*koopa, static_cast<float>(koopa->width()), cameraX_);
    }
    for (size_t i = 0; i < koopas.size(); ++i) {
        for (size_t j = i + 1; j < koopas.size(); ++j) {
            checkShellVsKoopa(*koopas[i], *koopas[j]);
            checkKoopaKoopaBounce(*koopas[i], *koopas[j]);
        }
    }

    for (ScorePopup& popup : scorePopups_) popup.update(dt);
    for (BrickDebris& debris : brickDebris_) debris.update(dt, level_.widthPixels(), kWorldViewBottom);
    for (BlockBump& bump : blockBumps_) {
        if (!bump.active) continue;
        bump.timer -= dt;
        if (bump.timer <= 0.0f) {
            bump.active = false;
            // El item recien aparece aca, cuando termina de sacudirse (ver
            // checkQuestionBlockHit) -- BlockCoin/ItemEmerge arrancan recien
            // ahora, no al momento del golpe.
            if (bump.hasPendingItem) {
                bump.hasPendingItem = false;
                scorer_ = bump.hitter;
                spawnItemFromQuestionBlock(bump.col, bump.row, bump.pendingMovingRight, bump.pendingHitterBig);
            }
        }
    }
    // Brillo compartido por todos los bloques de interrogacion sin usar del
    // nivel (ver el comentario de questionBlockSprite_ en Game.h): mismo
    // criterio que el resto del elenco (Coin/FireFlower), 3 frames en fila
    // y vuelve a arrancar.
    questionBlockAnimTimer_ += dt;
    if (questionBlockAnimTimer_ >= kQuestionBlockAnimFrameDuration) {
        questionBlockAnimTimer_ -= kQuestionBlockAnimFrameDuration;
        questionBlockAnimFrame_ = (questionBlockAnimFrame_ + 1) % 3;
    }

    // Giro de las monedas paradas arriba de un ladrillo (ver
    // brickCoinAnimTimer_ en Game.h): mismo criterio que arriba, pero con la
    // duracion de frame de Coin::kFrameDuration (la moneda del piso) para
    // que se vean igual de rapidas.
    brickCoinAnimTimer_ += dt;
    if (brickCoinAnimTimer_ >= kBrickCoinAnimFrameDuration) {
        brickCoinAnimTimer_ -= kBrickCoinAnimFrameDuration;
        brickCoinAnimFrame_ = (brickCoinAnimFrame_ + 1) % 3;
    }

    scorer_ = 0;
    trySpawnFireball(mario_, marioFireballs_);
    for (Fireball& fireball : marioFireballs_) {
        fireball.update(dt, level_, level_.widthPixels());
        if (fireball.consumeWallHitRequest()) audio_.play(Sfx::Bump);
        for (Goomba& goomba : goombas_) checkFireballGoombaCollision(fireball, goomba);
        for (Koopa* koopa : koopas) checkFireballKoopaCollision(fireball, *koopa);
        for (BuzzyBeetle& buzzy : buzzyBeetles_) checkFireballBuzzyBeetleCollision(fireball, buzzy);
        for (PiranhaPlant& plant : piranhaPlants_) checkFireballPiranhaPlantCollision(fireball, plant);
        for (HammerBro& bro : hammerBros_) checkFireballHammerBroCollision(fireball, bro);
        checkFireballLakituCollision(fireball);
        for (Spiny& spiny : spinies_) checkFireballSpinyCollision(fireball, spiny);
        for (Blooper& blooper : bloopers_) checkFireballBlooperCollision(fireball, blooper);
        checkFireballCheepCheepCollision(fireball, cheepCheepSwim_);
        checkFireballCheepCheepCollision(fireball, cheepCheepSwimOsc_);
        for (CheepCheep& fish : levelCheeps_) checkFireballCheepCheepCollision(fireball, fish);
        for (CheepCheep& fish : cheepCheepJumps_) checkFireballCheepCheepCollision(fireball, fish);
        for (BulletBill& bill : bulletBills_) checkFireballBulletBillCollision(fireball, bill);
        for (Podoboo& podoboo : podoboos_) checkFireballPodobooCollision(fireball, podoboo);
        checkFireballBowserCollision(fireball);
    }

    if (twoPlayerMode_) {
        scorer_ = 1;
        trySpawnFireball(luigi_, luigiFireballs_);
        for (Fireball& fireball : luigiFireballs_) {
            fireball.update(dt, level_, level_.widthPixels());
        if (fireball.consumeWallHitRequest()) audio_.play(Sfx::Bump);
            for (Goomba& goomba : goombas_) checkFireballGoombaCollision(fireball, goomba);
            for (Koopa* koopa : koopas) checkFireballKoopaCollision(fireball, *koopa);
            for (BuzzyBeetle& buzzy : buzzyBeetles_) checkFireballBuzzyBeetleCollision(fireball, buzzy);
            for (PiranhaPlant& plant : piranhaPlants_) checkFireballPiranhaPlantCollision(fireball, plant);
            for (HammerBro& bro : hammerBros_) checkFireballHammerBroCollision(fireball, bro);
            checkFireballLakituCollision(fireball);
            for (Spiny& spiny : spinies_) checkFireballSpinyCollision(fireball, spiny);
            for (Blooper& blooper : bloopers_) checkFireballBlooperCollision(fireball, blooper);
            checkFireballCheepCheepCollision(fireball, cheepCheepSwim_);
            checkFireballCheepCheepCollision(fireball, cheepCheepSwimOsc_);
            for (CheepCheep& fish : levelCheeps_) checkFireballCheepCheepCollision(fireball, fish);
            for (CheepCheep& fish : cheepCheepJumps_) checkFireballCheepCheepCollision(fireball, fish);
            for (BulletBill& bill : bulletBills_) checkFireballBulletBillCollision(fireball, bill);
            for (Podoboo& podoboo : podoboos_) checkFireballPodobooCollision(fireball, podoboo);
            checkFireballBowserCollision(fireball);
        }
    }
}

void Game::render() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    if (appState_ == AppState::Title || appState_ == AppState::Settings || appState_ == AppState::Controls ||
        appState_ == AppState::Credits || appState_ == AppState::DemoEnd) {
        switch (appState_) {
            case AppState::Title: renderTitle(); break;
            case AppState::Settings: renderSettings(); break;
            case AppState::Controls: renderControls(); break;
            case AppState::Credits: renderCredits(); break;
            default: renderDemoEnd(); break;
        }
        renderFpsCorner();
        SDL_RenderPresent(renderer_);
        return;
    }
    if (appState_ == AppState::ScenarioSelect) {
        renderScenarioSelect();
        SDL_RenderPresent(renderer_);
        return;
    }
    if (appState_ == AppState::LevelIntro || appState_ == AppState::GameOver) {
        // Pantalla negra con el HUD arriba, como el original.
        renderHud();
        if (appState_ == AppState::LevelIntro) {
            renderLevelIntro();
        } else {
            renderGameOver();
        }
        SDL_RenderPresent(renderer_);
        return;
    }

    beginWorldRender();
    const int worldRow = static_cast<int>(worldType_);
    std::vector<SDL_Point> bumpingTiles;
    for (const BlockBump& bump : blockBumps_) {
        if (bump.active) bumpingTiles.push_back(SDL_Point{bump.col, bump.row});
    }
    // Fondo, escenografia (colinas, nubes, arbustos) y recien despues los
    // bloques: la escenografia queda siempre detras.
    {
        const SDL_Color bg = backgroundColorFor(worldType_);
        SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer_, nullptr);
    }
    renderDecorations(false);
    level_.renderTerrain(renderer_, backgroundColorFor(worldType_), terrainSprite_, worldRow, gLogicalWidth,
                         cameraX_, false);
    level_.renderScenery(renderer_, sceneryTilesSprite_, worldRow, gLogicalWidth, cameraX_);
    level_.renderHazards(renderer_, lavaSprite_, bridgeSprite_, gLogicalWidth, cameraX_);
    for (const MovingPlatform& platform : platforms_) platform.render(renderer_, liftSprite_, cameraX_);
    // Las Piranha Plant van ANTES que las tuberias: asi la tuberia las tapa
    // mientras asoman o se esconden.
    for (const PiranhaPlant& plant : piranhaPlants_) plant.render(renderer_, piranhaPlantSprite_, worldRow, cameraX_);
    // El que esta entrando/saliendo de una tuberia va detras de ella.
    if (pipeAnim_.active && pipeAnim_.character) pipeAnim_.character->render(renderer_, cameraX_);
    level_.renderPipes(renderer_, pipeSprite_, worldRow, cameraX_);
    flagpole_.render(renderer_, flagpoleSprite_, flagSprite_, worldRow, cameraX_);
    // Los items que pueden salir de un bloque van ANTES de dibujar los
    // bloques (ver el comentario del usuario): mientras esta "emergiendo"
    // (ItemEmerge) el bloque tiene que quedar tapandolo, no al reves. No
    // afecta a nada mas: una vez que salio del todo no vuelve a superponerse
    // con el bloque, da igual el orden.
    mushroom_.render(renderer_, mushroom_.isOneUp() ? oneUpSprite_ : mushroomSprite_, worldRow, cameraX_);
    fireFlower_.render(renderer_, fireFlowerSprite_, worldRow, cameraX_);
    star_.render(renderer_, starSprite_, worldRow, cameraX_);
    coin_.render(renderer_, coinSprite_, worldRow, cameraX_);
    // Una sola paleta (ver el comentario de BlockCoin.h): sin worldRow.
    blockCoin_.render(renderer_, blockCoinSprite_, cameraX_);
    level_.renderBlocks(renderer_, brickBlockSprite_, worldRow, cameraX_, bumpingTiles, &questionBlockSprite_,
                        questionBlockAnimFrame_, &coinSprite_, brickCoinAnimFrame_);
    const int camOffset = static_cast<int>(std::lround(cameraX_));
    for (const BlockBump& bump : blockBumps_) {
        if (!bump.active) continue;
        // Sube y vuelve a bajar en el mismo lapso (seno completo entre 0 y
        // pi): 0 al arrancar y al terminar, maximo a la mitad.
        const float progress = 1.0f - bump.timer / kBumpDuration;
        const int offsetY = -static_cast<int>(std::lround(std::sin(progress * 3.14159265f) * kBumpHeight));
        // Mismo pool para ladrillo y bloque de interrogacion (ver
        // startBlockBump): hay que fijarse cual de los dos es esta celda
        // para dibujarla con el sprite/frame que corresponde, en vez de
        // asumir siempre ladrillo.
        if (level_.isQuestionTile(bump.col, bump.row)) {
            const int frame = level_.isQuestionUsed(bump.col, bump.row) ? Tilemap::kQuestionUsedFrame
                                                                          : questionBlockAnimFrame_;
            questionBlockSprite_.draw(renderer_, worldRow, frame, bump.col * level_.tileSize() - camOffset,
                                       bump.row * level_.tileSize() + offsetY, false);
        } else if (level_.isDispenserTile(bump.col, bump.row) && level_.isDispenserUsed(bump.col, bump.row)) {
            // Mismo frame "vacio" que un bloque de interrogacion usado (ver
            // el comentario de Tilemap::renderBlocks) -- se marca usado
            // antes de que arranque esta sacudida (checkQuestionBlockHit),
            // asi que se ve asi durante todo el salto, no solo al terminar.
            questionBlockSprite_.draw(renderer_, worldRow, Tilemap::kQuestionUsedFrame,
                                       bump.col * level_.tileSize() - camOffset,
                                       bump.row * level_.tileSize() + offsetY, false);
        } else {
            brickBlockSprite_.draw(renderer_, worldRow, level_.brickFrame(bump.col, bump.row),
                                   bump.col * level_.tileSize() - camOffset,
                                    bump.row * level_.tileSize() + offsetY, false);
        }
    }
    for (const Goomba& goomba : goombas_) goomba.render(renderer_, goombaSprite_, worldRow, cameraX_);
    for (const Koopa& koopa : levelKoopas_) {
        koopa.render(renderer_, koopa.color() == KoopaColor::Red ? koopaRedSprite_ : koopaSprite_, worldRow,
                     cameraX_);
    }
    koopaA_.render(renderer_, koopaSprite_, worldRow, cameraX_);
    koopaB_.render(renderer_, koopaSprite_, worldRow, cameraX_);
    koopaJumping_.render(renderer_, koopaSprite_, worldRow, cameraX_);
    koopaRed_.render(renderer_, koopaRedSprite_, worldRow, cameraX_);
    koopaRedFlying_.render(renderer_, koopaRedSprite_, worldRow, cameraX_);
    for (const BuzzyBeetle& buzzy : buzzyBeetles_) buzzy.render(renderer_, buzzyBeetleSprite_, worldRow, cameraX_);
    for (const HammerBro& bro : hammerBros_) {
        bro.render(renderer_, hammerBroSprite_, worldRow, cameraX_);
        if (!bro.isWindingUp()) continue;
        // Amague antes de tirar (ver HammerBro::isWindingUp): el martillo
        // quieto sobre la cabeza un instante, reusando el sprite del
        // proyectil (frame 0, cualquiera sirve para la pose quieta) en vez
        // de necesitar un frame propio de HammerBro para esto.
        const int drawX = static_cast<int>(std::lround(bro.throwOriginX())) - Hammer::kSize / 2 - camOffset;
        const int drawY = static_cast<int>(std::lround(bro.y())) - Hammer::kSize + 4;
        hammerSprite_.draw(renderer_, worldRow, 0, drawX, drawY, false);
    }
    lakitu_.render(renderer_, lakituSprite_, worldRow, cameraX_);
    // Una sola paleta (ver el comentario de Spiny.h): fila 0 siempre, no worldRow.
    for (const Spiny& spiny : spinies_) spiny.render(renderer_, spinySprite_, 0, cameraX_);
    for (const Blooper& blooper : bloopers_) blooper.render(renderer_, blooperSprite_, worldRow, cameraX_);
    // Una sola paleta (ver el comentario de CheepCheep.h): sin worldRow.
    cheepCheepSwim_.render(renderer_, cheepCheepSprite_, cameraX_);
    cheepCheepSwimOsc_.render(renderer_, cheepCheepSprite_, cameraX_);
    for (const CheepCheep& fish : cheepCheepJumps_) fish.render(renderer_, cheepCheepSprite_, cameraX_);
    for (const CheepCheep& fish : levelCheeps_) fish.render(renderer_, cheepCheepSprite_, cameraX_);
    for (const BulletBill& bill : bulletBills_) bill.render(renderer_, bulletBillSprite_, worldRow, cameraX_);
    // Una sola paleta (ver el comentario de Podoboo.h): sin worldRow.
    for (const Podoboo& podoboo : podoboos_) podoboo.render(renderer_, podobooSprite_, cameraX_);
    renderFirebars();
    // El hacha primero, Bowser encima -- si llegaran a superponerse (no
    // deberia pasar con las posiciones de prueba) que se vea el jefe, no el
    // objeto quieto de fondo.
    axe_.render(renderer_, axeSprite_, questionBlockAnimFrame_, cameraX_);
    // El fuego ANTES que Bowser a proposito (a pedido del usuario): asi el
    // propio sprite de Bowser lo tapa mientras el origen (fireOriginX/Y)
    // todavia queda "dentro" de su silueta, y recien se ve salir una vez
    // que la boca lo deja atras -- sin depender de ajustar el origen
    // pixel-perfecto a donde cae la boca en cada frame de caminata.
    // Una sola paleta (el fuego no cambia de color por WorldType, ver el
    // comentario de BowserFire.h): sin worldRow.
    for (const BowserFire& fire : bowserFires_) fire.render(renderer_, bowserFireSprite_, cameraX_);
    // En castillos usa la paleta Overworld en vez de la Castle (nota de la
    // hoja de referencia: "By default, Bowser uses the Underground palette,
    // but a trigger... changes it into the Overworld palette" -- aca no
    // hay trigger de nivel real todavia, asi que directamente arranca con
    // la Overworld en Castle). En los demas mundos (F7 los sigue pudiendo
    // ciclar) usa su fila normal como el resto del elenco.
    const int bowserWorldRow =
        worldType_ == WorldType::Castle ? static_cast<int>(WorldType::Overworld) : worldRow;
    bowser_.render(renderer_, bowserSprite_, bowserWorldRow, cameraX_);
    if (!(pipeAnim_.active && pipeAnim_.character == &mario_)) renderCharacter(mario_, marioPlatform_);
    for (const Fireball& fireball : marioFireballs_) fireball.render(renderer_, fireballSprite_, fireballExplosionSprite_, cameraX_);
    if (twoPlayerMode_) {
        if (!(pipeAnim_.active && pipeAnim_.character == &luigi_)) renderCharacter(luigi_, luigiPlatform_);
        for (const Fireball& fireball : luigiFireballs_) fireball.render(renderer_, fireballSprite_, fireballExplosionSprite_, cameraX_);
    }
    renderDecorations(true);
    // Los martillos en vuelo van al final a proposito, adelante de todo
    // (personajes incluidos): que nunca queden tapados por nada mientras
    // vuelan.
    for (const Hammer& hammer : hammers_) hammer.render(renderer_, hammerSprite_, worldRow, cameraX_);
    for (const BrickDebris& debris : brickDebris_) debris.render(renderer_, brickDebrisSprite_, worldRow, cameraX_);

    for (const ScorePopup& popup : scorePopups_) popup.render(renderer_, cameraX_);

    if (castleMessage_ && castleClearTimer_ <= 0.0f) {
        // Castillo: el Toad (o la princesa, en el ultimo) mientras se cuenta
        // el tiempo que sobro.
        const SDL_Color white{255, 255, 255, 255};
        drawCentered(70, tr(Txt::ThankYouMario), 2, white);
        if (campaign_ && campaignWorld_ == 8) {
            drawCentered(100, tr(Txt::QuestOver1), 2, white);
            drawCentered(120, tr(Txt::QuestOver2), 2, white);
        } else {
            drawCentered(100, tr(Txt::PrincessElsewhere1), 2, white);
            drawCentered(120, tr(Txt::PrincessElsewhere2), 2, white);
        }
    } else if (levelClearTimer_ > 0.0f) {
        drawCentered(100, tr(Txt::CourseClear), 2, SDL_Color{255, 255, 255, 255});
    }

    renderOutsideAreaMask();
    endWorldRender();
    renderHud();

    // Debajo del HUD.
    if (showDebugOverlay_) {
        char fpsText[32];
        std::snprintf(fpsText, sizeof(fpsText), "FPS:%3d", currentFps_);
        DebugFont::drawText(renderer_, 4, 40, fpsText, 2, SDL_Color{255, 255, 255, 255});
        // Cartel de tipo de mundo (F7 lo cicla), para guiarse mientras se
        // prueban las paletas por WorldType de cada item/enemigo.
        DebugFont::drawText(renderer_, 4, 52, worldTypeName(worldType_), 1, SDL_Color{255, 255, 255, 255});
        char camText[48];
        std::snprintf(camText, sizeof(camText), "CAM:%d MX:%d A:%d", static_cast<int>(std::lround(cameraX_)),
                      static_cast<int>(mario_.x()), currentArea_);
        DebugFont::drawText(renderer_, 60, 52, camText, 1, SDL_Color{255, 255, 255, 255});
        // Que tema esta sonando (ver GameMusic.cpp).
        DebugFont::drawText(renderer_, 160, 52, std::string("MUS:") + musicName(audio_.currentMusic()), 1,
                            SDL_Color{255, 255, 255, 255});
        if (scenario_ == TestScenario::Bowser) {
            // Cartel del modo de ataque (tecla T lo cicla), para saber cual
            // se esta probando sin tener que adivinar mirando el patron.
            DebugFont::drawText(renderer_, 4, 60, bowserAttackModeName(bowser_.attackMode()), 1,
                                 SDL_Color{255, 255, 255, 255});
        }
        if (scenario_ == TestScenario::Items) {
            // Contador de monedas (tambien en el HUD, ver Game::collectCoin).
            char coinText[24];
            std::snprintf(coinText, sizeof(coinText), "COINS:%3d", coinCount_);
            DebugFont::drawText(renderer_, 4, 60, coinText, 1, SDL_Color{255, 255, 255, 255});
            // livesCount_: ver el comentario en Game.h, suma cuando se
            // agarra el hongo 1-UP (checkMushroomPickup).
            char livesText[24];
            std::snprintf(livesText, sizeof(livesText), "LIVES:%3d", livesCount_);
            DebugFont::drawText(renderer_, 4, 68, livesText, 1, SDL_Color{255, 255, 255, 255});
        }
    } else {
        renderFpsCorner();
    }

    if (appState_ == AppState::Paused) renderPauseMenu();
    SDL_RenderPresent(renderer_);
}

void Game::renderScenarioSelect() {
    // Fondo azul overworld fijo, no depende de worldType_ (todavia no se
    // eligio nivel). Fuente bitmap propia (DebugFont) -- placeholder hasta
    // tener fuentes reales para menus (seccion 10).
    SDL_SetRenderDrawColor(renderer_, 92, 148, 252, 255);
    SDL_Rect background{0, 0, gLogicalWidth, kLogicalHeight};
    SDL_RenderFillRect(renderer_, &background);

    const SDL_Color white{255, 255, 255, 255};
    const SDL_Color yellow{252, 216, 88, 255};
    // Layout armado para 256px, centrado en la pantalla real.
    const int ox = (gLogicalWidth - kClassicWidth) / 2;
    DebugFont::drawText(renderer_, ox + 84, 24, "TEST", 3, white);

    // Mismo orden que TestScenario (menuSelection_ es el indice).
    static const char* const kOptions[kMenuOptionCount] = {
        "1 ITEMS",     "2 ENEMIGOS",    "3 HAMMER BRO", "4 LAKITU",
        "5 BLOOPER",   "6 CHEEP CHEEP", "7 BILL BALA",  "8 CHEEP CHEEP SALTO",
        "9 PODOBOO",   "0 BOWSER",      "  META",       "  PLATAFORMAS", "  1-1",
    };
    for (int i = 0; i < kMenuOptionCount; ++i) {
        const int y = 46 + i * 13;
        const bool selected = i == menuSelection_;
        if (selected) DebugFont::drawText(renderer_, ox + 20, y, ">", 2, white);
        DebugFont::drawText(renderer_, ox + 32, y, kOptions[i], 2, selected ? white : yellow);
    }

    DebugFont::drawText(renderer_, ox + 8, 222, "1 2 3 4 5 6 7 8 9 0", 1, white);
}

void Game::moveMenuSelection(int delta) {
    menuSelection_ = (menuSelection_ + delta + kMenuOptionCount) % kMenuOptionCount;
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
    if (worldTarget_) SDL_DestroyTexture(worldTarget_);
    audio_.shutdown();  // antes de SDL_Quit() -- ver el comentario en SoundManager.h
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    SDL_Quit();
}

void Game::checkFlagpoleGrab(Mario& character) {
    setScorer(character);
    if (!flagpole_.isActive() || character.isOnFlagpole() || character.isInDeathSequence()) return;
    if (!flagpole_.touches(character.x(), character.y(), character.width(), character.height())) return;

    // Puntaje segun la altura del agarre, como en el original: cuanto mas
    // arriba (pies respecto del pie del mastil), mas puntos -- el tope
    // exacto da 5000.
    const float poleHeight = flagpole_.bottomY() - flagpole_.topY();
    const float grabHeight = flagpole_.bottomY() - (character.y() + static_cast<float>(character.height()));
    const float ratio = poleHeight > 0.0f ? grabHeight / poleHeight : 0.0f;
    const char* points = ratio >= 0.9f ? "5000" : ratio >= 0.65f ? "2000" : ratio >= 0.4f ? "800"
                       : ratio >= 0.2f ? "400" : "100";
    spawnScorePopup(flagpole_.poleCenterX() + 4.0f, character.y(), points);

    // La puerta del castillo: 5 tiles despues del mastil, sin pasarse del
    // final del nivel (todavia no hay castillo dibujado, ver Flagpole.h).
    const float ts = static_cast<float>(level_.tileSize());
    const float exitX = std::min(flagpole_.poleCenterX() + 5.0f * ts,
                                 static_cast<float>(level_.widthPixels()) - ts);
    character.startFlagpoleGrab(flagpole_.poleCenterX(), flagpole_.bottomY(), exitX);
    if (!levelEnding_) levelFinisher_ = scorer_;  // el primero que la agarra
    levelEnding_ = true;
    if (!flagpole_.isLowering()) {
        flagpole_.startLowering();
        audio_.play(Sfx::Flagpole);
    }
}

void Game::updateFlagpoleSequence(float dt) {
    if (!flagpole_.isActive()) return;
    flagpole_.update(dt);
    if (flagpole_.isFlagDown()) {
        if (mario_.isWaitingAtFlagpoleBottom()) mario_.releaseFromFlagpole();
        if (twoPlayerMode_ && luigi_.isWaitingAtFlagpoleBottom()) luigi_.releaseFromFlagpole();
    }

}

void Game::updateLevelClear(float dt) {
    if (castleClearTimer_ > 0.0f) {
        castleClearTimer_ -= dt;
        if (castleClearTimer_ <= 0.0f) {
            timeTallying_ = true;
            tallySoundTimer_ = 0.0f;
        }
    }
    if (levelClearTimer_ > 0.0f) {
        levelClearTimer_ -= dt;
        if (levelClearTimer_ <= 0.0f) onLevelCleared();
    } else if (timeTallying_) {
        // El tiempo que sobro pasa a puntos de a una unidad, rapido.
        const float before = std::ceil(timeRemaining_);
        timeRemaining_ = std::max(0.0f, timeRemaining_ - kTallyUnitsPerSecond * dt);
        scorer_ = levelFinisher_;
        addScore(static_cast<int>(before - std::ceil(timeRemaining_)) * kTallyPointsPerUnit);
        tallySoundTimer_ -= dt;
        if (tallySoundTimer_ <= 0.0f && timeRemaining_ > 0.0f) {
            tallySoundTimer_ = kTallySoundInterval;
            audio_.play(Sfx::TimeTally);
        }
        if (timeRemaining_ <= 0.0f) {
            timeTallying_ = false;
            levelClearTimer_ = kLevelClearDuration;
        }
    } else if (mario_.hasFinishedLevel() || (twoPlayerMode_ && luigi_.hasFinishedLevel())) {
        timeTallying_ = true;
        tallySoundTimer_ = 0.0f;
    }
}

void Game::collectCoin() {
    ++coinCount_;
    addScore(200);
    int& coins = playerCoins_[static_cast<size_t>(scorer_)];
    ++coins;
    if (coins >= 100) {
        coins -= 100;
        ++livesCount_;
        audio_.play(Sfx::OneUp);
    }
}

void Game::resetLevelTimer() {
    timeRemaining_ = static_cast<float>(level_.timeLimit());
    hurryUpPlayed_ = timeRemaining_ <= kHurryUpThreshold;
}

void Game::updateLevelTimer(float dt) {
    if (timeRemaining_ <= 0.0f || levelEnding_ || cutscene_) return;
    timeRemaining_ = std::max(0.0f, timeRemaining_ - dt / kTimeUnitSeconds);
    if (!hurryUpPlayed_ && timeRemaining_ <= kHurryUpThreshold) {
        hurryUpPlayed_ = true;
        audio_.play(Sfx::HurryUp);
        startHurryMusic();
    }
    if (timeRemaining_ <= 0.0f) {
        // Se acabo el tiempo: mueren todos, sin importar tamano/estrella.
        if (mario_.killInstantly()) audio_.play(Sfx::Death);
        if (twoPlayerMode_ && luigi_.killInstantly()) audio_.play(Sfx::Death);
    }
}


void Game::renderHud() {
    // Letras con borde negro: el HUD va encima del nivel y sin borde no se
    // lee sobre nubes ni sobre techos claros (castillo).
    // Mismo layout que el original (grilla de 8px, la fuente a escala 2 es
    // la del juego, 8x8): titulos arriba, valores en el renglon de abajo.
    // Las cuatro columnas del original (pensadas para 256px) se reparten en
    // el ancho real con la misma proporcion. En 2P la columna de las monedas
    // pasa a ser la de LUIGI, y las monedas de cada uno van debajo de su
    // puntaje.
    const SDL_Color white{255, 255, 255, 255};
    const SDL_Color marioRed{248, 56, 0, 255};
    const SDL_Color luigiGreen{0, 200, 0, 255};
    constexpr int kTitleY = 8;
    constexpr int kValueY = 16;
    constexpr int kCoinsY = 24;
    auto column = [](int classicX) { return classicX * gLogicalWidth / kClassicWidth; };
    const int marioX = column(24);
    const int coinX = column(96);
    const int luigiX = column(88);
    const int worldX = column(144);
    const int timeX = column(200);
    const bool onTitle = appState_ == AppState::Title;
    const int coinRow = static_cast<int>(onTitle ? WorldType::Overworld : worldType_);
    char text[16];

    // Moneda + "x00" (la moneda cicla de brillo junto con los bloques ?,
    // como el original).
    auto drawCoins = [&](int x, int y, int coins) {
        hudCoinSprite_.draw(renderer_, coinRow, questionBlockAnimFrame_, x, y, false);
        std::snprintf(text, sizeof(text), "x%02d", coins);
        DebugFont::drawTextOutlined(renderer_, x + 8, y, text, 2, white);
    };
    auto drawPlayer = [&](int x, const char* name, SDL_Color color, int score) {
        DebugFont::drawTextOutlined(renderer_, x, kTitleY, name, 2, color);
        std::snprintf(text, sizeof(text), "%06d", std::min(score, 999999));
        DebugFont::drawTextOutlined(renderer_, x, kValueY, text, 2, white);
    };

    if (twoPlayerMode_ && !onTitle) {
        drawPlayer(marioX, "MARIO", marioRed, playerScore_[0]);
        drawCoins(marioX, kCoinsY, playerCoins_[0]);
        drawPlayer(luigiX, "LUIGI", luigiGreen, playerScore_[1]);
        drawCoins(luigiX, kCoinsY, playerCoins_[1]);
    } else {
        drawPlayer(marioX, "MARIO", marioRed, score_);
        drawCoins(coinX, kValueY, playerCoins_[0]);
    }

    DebugFont::drawTextOutlined(renderer_, worldX, kTitleY, "WORLD", 2, white);
    if (onTitle) {
        std::snprintf(text, sizeof(text), "%d-%d", campaignWorld_, campaignLevel_);
        DebugFont::drawTextOutlined(renderer_, worldX + 8, kValueY, text, 2, white);
    } else {
        DebugFont::drawTextOutlined(renderer_, worldX + 8, kValueY, level_.name(), 2, white);
    }

    DebugFont::drawTextOutlined(renderer_, timeX, kTitleY, "TIME", 2, white);
    // En el titulo y las pantallas negras el tiempo no se muestra (como el original).
    if (appState_ == AppState::Playing || appState_ == AppState::Paused) {
        std::snprintf(text, sizeof(text), "%03d", static_cast<int>(std::ceil(timeRemaining_)));
        DebugFont::drawTextOutlined(renderer_, timeX + 8, kValueY, text, 2, white);
    }
}

void Game::onAllPlayersDied() {
    --livesCount_;
    if (livesCount_ <= 0) {
        livesCount_ = 0;
        saveTopScore();
        appState_ = AppState::GameOver;
        appStateTimer_ = kGameOverDuration;
        audio_.playMusic(Music::GameOver, false);
        return;
    }
    // Reintento: el nivel vuelve a su estado inicial (bloques, enemigos,
    // timer) y se arranca desde el ultimo checkpoint, conservando puntaje
    // y monedas como el original.
    loadScenario(scenario_);
}

void Game::updateCheckpoints() {
    const float ts = static_cast<float>(level_.tileSize());
    for (const Mario* character : {&mario_, &luigi_}) {
        if (character == &luigi_ && !twoPlayerMode_) continue;
        if (character->isInDeathSequence()) continue;
        for (int col : level_.checkpointCols()) {
            if (col > checkpointCol_ && character->x() >= static_cast<float>(col) * ts) checkpointCol_ = col;
        }
    }
}

void Game::checkPitDeath(Mario& character) {
    if (character.isInDeathSequence()) return;
    if (character.y() <= static_cast<float>(level_.heightPixels())) return;
    if (character.killInstantly(false)) audio_.play(Sfx::Death);
}

void Game::renderLevelIntro() {
    const SDL_Color white{255, 255, 255, 255};
    const std::string world = "WORLD " + level_.name();
    DebugFont::drawText(renderer_, (gLogicalWidth - static_cast<int>(world.size()) * 8) / 2, 80, world, 2, white);
    // Mario chico parado, una "x" y las vidas (como el original).
    const int x = gLogicalWidth / 2 - 28;
    introMarioSprite_.draw(renderer_, 0, 0, x, 108, false);
    char text[16];
    std::snprintf(text, sizeof(text), "x  %d", livesCount_);
    DebugFont::drawText(renderer_, x + 24, 112, text, 2, white);
}

void Game::renderGameOver() {
    DebugFont::drawText(renderer_, (gLogicalWidth - 9 * 8) / 2, 112, "GAME OVER", 2, SDL_Color{255, 255, 255, 255});
}

void Game::awardEnemyPoints(float x, float y, int points) {
    spawnScorePopup(x, y, std::to_string(points));
}

const QuestionBlockSpawn& Game::findBlockSpawn(int col, int row) const {
    for (const QuestionBlockSpawn& entry : blockSpawns_) {
        if (entry.col == col && entry.row == row) return entry;
    }
    static const QuestionBlockSpawn kDefaultCoin{0, 0, BlockItem::Coin, 1};
    return kDefaultCoin;
}

namespace {

BlockItem parseBlockItem(const std::string& name) {
    if (name == "powerup") return BlockItem::PowerUp;
    if (name == "mushroom") return BlockItem::Mushroom;
    if (name == "fire_flower") return BlockItem::FireFlower;
    if (name == "star") return BlockItem::Star;
    if (name == "one_up") return BlockItem::OneUp;
    if (name != "coin") SDL_Log("Item de bloque desconocido '%s', se usa moneda", name.c_str());
    return BlockItem::Coin;
}

}  // namespace

void Game::loadLevelData() {
    blockSpawns_.clear();
    for (const LevelBlock& block : level_.blocks()) {
        blockSpawns_.push_back(QuestionBlockSpawn{block.col, block.row, parseBlockItem(block.item), block.hits});
    }
    const float ts = static_cast<float>(level_.tileSize());
    pipeWarps_.clear();
    for (const LevelWarp& warp : level_.warps()) {
        pipeWarps_.push_back(PipeWarp{warp.col, warp.row, warp.destCol * ts, warp.destRow * ts, warp.dir == "right"});
    }
    entitySpawned_.assign(level_.entities().size(), false);

    cannons_.clear();
    for (int col = 0; col < level_.widthTiles(); ++col) {
        for (int row = 0; row < level_.heightTiles(); ++row) {
            if (level_.isCannonTopTile(col, row)) cannons_.push_back(Cannon{col, row, 1.0f});
        }
    }
    firebars_.clear();
    cheepJumpZones_.clear();
    lakituEndX_ = -1.0f;
    for (const LevelEntity& entity : level_.entities()) {
        if (entity.type == "firebar") {
            firebars_.push_back(Firebar{(static_cast<float>(entity.col) + 0.5f) * ts,
                                        (static_cast<float>(entity.row) + 0.5f) * ts, entity.length, 0.0f,
                                        entity.speed});
        } else if (entity.type == "cheep_jump") {
            const int endCol = entity.endCol >= 0 ? entity.endCol : level_.widthTiles();
            cheepJumpZones_.emplace_back(static_cast<float>(entity.col) * ts, static_cast<float>(endCol) * ts);
        } else if (entity.type == "lakitu" && entity.endCol >= 0) {
            lakituEndX_ = static_cast<float>(entity.endCol) * ts;
        }
    }
}

void Game::spawnLevelEntities() {
    const float ts = static_cast<float>(level_.tileSize());
    const std::vector<LevelEntity>& entities = level_.entities();
    for (size_t i = 0; i < entities.size(); ++i) {
        if (entitySpawned_[i]) continue;
        const LevelEntity& entity = entities[i];
        const float x = static_cast<float>(entity.col) * ts;
        if (x + ts < cameraX_) {
            entitySpawned_[i] = true;  // quedo atras de la camara: no aparece
            continue;
        }
        // Un tile mas alla del borde derecho: aparece justo fuera de vista.
        if (x > cameraX_ + static_cast<float>(gLogicalWidth) + ts) continue;

        // row es la fila del enemigo: los pies sobre el borde de abajo.
        const float feetY = static_cast<float>(entity.row + 1) * ts;
        bool spawned = false;
        if (entity.type == "goomba") {
            spawned = spawnGoomba(x, feetY - static_cast<float>(Goomba::kSize), entity.movingRight);
        } else if (entity.type == "koopa") {
            const KoopaColor color = entity.color == "red" ? KoopaColor::Red : KoopaColor::Green;
            const KoopaState state = entity.state == "jumping" ? KoopaState::Jumping
                                     : entity.state == "flying" ? KoopaState::Flying
                                                                : KoopaState::Walking;
            spawned = spawnLevelKoopa(x, feetY - static_cast<float>(Koopa::kWalkHeight), entity.movingRight, color, state);
        } else if (entity.type == "buzzy") {
            spawned = spawnInPool(buzzyBeetles_, [&](BuzzyBeetle& buzzy) {
                buzzy.spawn(x, feetY - static_cast<float>(BuzzyBeetle::kSize), entity.movingRight);
            });
        } else if (entity.type == "piranha") {
            // col/row: tope izquierdo de la tuberia (2 tiles de ancho).
            spawned = spawnInPool(piranhaPlants_, [&](PiranhaPlant& plant) {
                plant.spawn(x + ts - static_cast<float>(PiranhaPlant::kWidth) / 2.0f,
                            static_cast<float>(entity.row) * ts);
            });
        } else if (entity.type == "hammer_bro") {
            spawned = spawnInPool(hammerBros_, [&](HammerBro& bro) {
                bro.spawn(x, feetY - static_cast<float>(HammerBro::kHeight), entity.movingRight);
            });
        } else if (entity.type == "lakitu") {
            if (!lakitu_.isAlive()) lakitu_.spawn(x, static_cast<float>(entity.row) * ts, entity.movingRight);
            spawned = true;
        } else if (entity.type == "blooper") {
            spawned = spawnInPool(bloopers_, [&](Blooper& blooper) {
                blooper.spawn(x, static_cast<float>(entity.row) * ts);
            });
        } else if (entity.type == "cheep") {
            spawned = spawnInPool(levelCheeps_, [&](CheepCheep& fish) {
                fish.setVariant(CheepCheepVariant::Swimming);
                fish.setSwimPattern(entity.color == "red" ? SwimPattern::Oscillating : SwimPattern::Straight);
                fish.spawn(x, static_cast<float>(entity.row) * ts, entity.movingRight);
            });
        } else if (entity.type == "podoboo") {
            spawned = spawnInPool(podoboos_, [&](Podoboo& podoboo) {
                podoboo.spawn(x, static_cast<float>(entity.row) * ts);
            });
        } else if (entity.type == "bowser") {
            bowser_.spawn(x, feetY - static_cast<float>(Bowser::kHeight), entity.movingRight);
            spawned = true;
        } else if (entity.type == "axe") {
            axe_.spawn(x, static_cast<float>(entity.row) * ts);
            spawned = true;
        } else if (entity.type == "firebar" || entity.type == "cheep_jump") {
            spawned = true;  // ya armados en loadLevelData
        } else {
            SDL_Log("Entidad de nivel desconocida '%s' (columna %d)", entity.type.c_str(), entity.col);
            spawned = true;  // no reintentar cada cuadro
        }
        // Pool lleno: se reintenta el cuadro siguiente.
        if (spawned) entitySpawned_[i] = true;
    }
}

bool Game::spawnGoomba(float x, float y, bool movingRight) {
    for (Goomba& goomba : goombas_) {
        if (goomba.isAlive()) continue;
        goomba.spawn(x, y, movingRight);
        return true;
    }
    return false;
}

bool Game::spawnLevelKoopa(float x, float y, bool movingRight, KoopaColor color, KoopaState state) {
    for (Koopa& koopa : levelKoopas_) {
        if (koopa.isAlive()) continue;
        koopa.setColor(color);
        koopa.spawn(x, y, movingRight, state);
        return true;
    }
    return false;
}

std::vector<Koopa*> Game::allKoopas() {
    std::vector<Koopa*> koopas{&koopaA_, &koopaB_, &koopaJumping_, &koopaRed_, &koopaRedFlying_};
    for (Koopa& koopa : levelKoopas_) koopas.push_back(&koopa);
    return koopas;
}

int Game::areaIndexAt(float x) const {
    const std::vector<LevelArea>& areas = level_.areas();
    const int col = static_cast<int>(std::floor(x / static_cast<float>(level_.tileSize())));
    for (size_t i = 0; i < areas.size(); ++i) {
        if (col >= areas[i].startCol && col <= areas[i].endCol) return static_cast<int>(i);
    }
    return 0;
}

void Game::updateCannons(float dt) {
    const float ts = static_cast<float>(level_.tileSize());
    for (Cannon& cannon : cannons_) {
        const float x = static_cast<float>(cannon.col) * ts;
        // Solo dispara en pantalla, como el original.
        if (x + ts < cameraX_ || x > cameraX_ + static_cast<float>(gLogicalWidth)) continue;
        cannon.timer -= dt;
        if (cannon.timer > 0.0f) continue;
        cannon.timer = randomRange(kCannonIntervalMin, kCannonIntervalMax);
        const Mario& target =
            (twoPlayerMode_ && std::abs(luigi_.x() - x) < std::abs(mario_.x() - x)) ? luigi_ : mario_;
        // Pegado al canon no dispara (si no seria imposible pasarlo).
        if (std::abs(target.x() - x) < kCannonSafeDistance) continue;
        const bool right = target.x() > x;
        for (BulletBill& bill : bulletBills_) {
            if (bill.isAlive()) continue;
            bill.spawn(right ? x + ts : x - static_cast<float>(BulletBill::kSize),
                       static_cast<float>(cannon.row) * ts, right);
            audio_.play(Sfx::BillFirework);
            break;
        }
    }
}

void Game::updateFirebars(float dt) {
    for (Firebar& bar : firebars_) {
        bar.angle = std::fmod(bar.angle + bar.speed * dt + 360.0f, 360.0f);
        const float radians = bar.angle * 3.14159265f / 180.0f;
        for (int i = 0; i < bar.length; ++i) {
            // Una bola de 8 px cada 8 px desde el centro del bloque.
            const float cx = bar.centerX + std::cos(radians) * static_cast<float>(i * Fireball::kSize);
            const float cy = bar.centerY + std::sin(radians) * static_cast<float>(i * Fireball::kSize);
            for (Mario* character : {&mario_, &luigi_}) {
                if (character == &luigi_ && !twoPlayerMode_) continue;
                if (character->isInDeathSequence() || character->isInvincible()) continue;
                // Hitbox un poco mas chica que el sprite (como el original).
                const float half = static_cast<float>(Fireball::kSize) / 2.0f - 1.0f;
                const bool overlap = character->x() < cx + half && character->x() + character->width() > cx - half &&
                                     character->y() < cy + half && character->y() + character->height() > cy - half;
                if (overlap) applyContactDamage(*character);
            }
        }
    }
}

void Game::renderFirebars() {
    const int frame = static_cast<int>(SDL_GetTicks() / 100) % 4;
    for (const Firebar& bar : firebars_) {
        const float radians = bar.angle * 3.14159265f / 180.0f;
        for (int i = 0; i < bar.length; ++i) {
            const float cx = bar.centerX + std::cos(radians) * static_cast<float>(i * Fireball::kSize);
            const float cy = bar.centerY + std::sin(radians) * static_cast<float>(i * Fireball::kSize);
            fireballSprite_.draw(renderer_, 0, frame,
                                 static_cast<int>(std::lround(cx - cameraX_)) - Fireball::kSize / 2,
                                 static_cast<int>(std::lround(cy)) - Fireball::kSize / 2, false);
        }
    }
}

bool Game::inCheepJumpZone() const {
    const float x = mario_.x();
    for (const auto& [start, end] : cheepJumpZones_) {
        if (x >= start && x <= end) return true;
    }
    return false;
}

void Game::updateLakituZone() {
    // Pasado el final de su zona, el Lakitu se va (deja de perseguir).
    if (lakituEndX_ >= 0.0f && lakitu_.isAlive() && mario_.x() > lakituEndX_) lakitu_.reset();
}

void Game::renderDecorations(bool front) {
    const int ts = level_.tileSize();
    const int camOffset = static_cast<int>(std::lround(cameraX_));
    for (const LevelDecoration& decoration : level_.decorations()) {
        if (decoration.front != front) continue;
        for (int i = 0; i < kDecorationCount; ++i) {
            if (decoration.type != kDecorations[i].type) continue;
            const int x = decoration.col * ts - camOffset;
            if (x + kDecorations[i].width < 0 || x > gLogicalWidth) break;
            decorationSprites_[i].draw(renderer_, 0, 0, x, decoration.row * ts, false);
            break;
        }
    }
}

void Game::checkCoinTiles(Mario& character) {
    setScorer(character);
    if (character.isInDeathSequence() || character.hasFinishedLevel()) return;
    const float ts = static_cast<float>(level_.tileSize());
    const int col0 = static_cast<int>(std::floor(character.x() / ts));
    const int col1 = static_cast<int>(std::floor((character.x() + character.width() - 1.0f) / ts));
    const int row0 = static_cast<int>(std::floor(character.y() / ts));
    const int row1 = static_cast<int>(std::floor((character.y() + character.height() - 1.0f) / ts));
    for (int row = row0; row <= row1; ++row) {
        for (int col = col0; col <= col1; ++col) {
            if (!level_.isCoinTile(col, row)) continue;
            level_.collectCoinTile(col, row);
            collectCoin();
            audio_.play(Sfx::Coin);
        }
    }
}

void Game::startBridgeCollapse() {
    // De la punta del hacha (derecha) hacia atras, como el original.
    bridgeTilesToCollapse_.clear();
    for (int col = level_.widthTiles() - 1; col >= 0; --col) {
        for (int row = 0; row < level_.heightTiles(); ++row) {
            if (level_.isBridgeTile(col, row)) bridgeTilesToCollapse_.push_back(SDL_Point{col, row});
        }
    }
    bridgeCollapsing_ = true;
    bridgeCollapseTimer_ = kBridgeCollapseInterval;
}

void Game::updateBridgeCollapse(float dt) {
    bridgeCollapseTimer_ -= dt;
    if (bridgeCollapseTimer_ > 0.0f) return;
    bridgeCollapseTimer_ = kBridgeCollapseInterval;
    if (!bridgeTilesToCollapse_.empty()) {
        const SDL_Point tile = bridgeTilesToCollapse_.front();
        bridgeTilesToCollapse_.erase(bridgeTilesToCollapse_.begin());
        level_.collapseBridgeTile(tile.x, tile.y);
        audio_.play(Sfx::Bump);
        return;
    }
    // Ya no queda puente: Bowser cae a la lava y el juego sigue.
    bridgeCollapsing_ = false;
    if (bowser_.isDangerous()) {
        bowser_.killByAxe(false);
        audio_.play(Sfx::BowserFall);
    }
    castleClearTimer_ = kCastleClearDelay;
}

void Game::resetCastleSequence() {
    level_.restoreBridges();
    bridgeCollapsing_ = false;
    bridgeTilesToCollapse_.clear();
    castleClearTimer_ = 0.0f;
    levelEnding_ = false;
    timeTallying_ = false;
    levelClearTimer_ = 0.0f;
}

void Game::checkLavaDeath(Mario& character) {
    if (character.isInDeathSequence()) return;
    // Unos px adentro de la lava (no al rozar la superficie con los pies).
    constexpr float kSinkPx = 4.0f;
    const float ts = static_cast<float>(level_.tileSize());
    const int row = static_cast<int>(std::floor((character.y() + character.height() - kSinkPx) / ts));
    const int col0 = static_cast<int>(std::floor(character.x() / ts));
    const int col1 = static_cast<int>(std::floor((character.x() + character.width() - 1.0f) / ts));
    for (int col = col0; col <= col1; ++col) {
        if (!level_.isLavaTile(col, row)) continue;
        if (character.killInstantly(false)) audio_.play(Sfx::Death);
        return;
    }
}

void Game::updatePlatforms(float dt) {
    for (MovingPlatform& platform : platforms_) platform.update(dt, level_.heightPixels());
    // Arrastra a los que iban parados encima (antes de que se muevan ellos).
    auto carry = [&](Mario& character, int riding) {
        if (riding < 0 || character.isInDeathSequence()) return;
        const MovingPlatform& platform = platforms_[static_cast<size_t>(riding)];
        if (!platform.isActive()) return;
        character.nudgeX(platform.lastDx());
        character.setY(character.y() + platform.lastDy());
    };
    carry(mario_, marioPlatform_);
    if (twoPlayerMode_) carry(luigi_, luigiPlatform_);

    platformRects_.resize(platforms_.size());
    for (size_t i = 0; i < platforms_.size(); ++i) {
        const MovingPlatform& platform = platforms_[i];
        platformRects_[i] = platform.isActive()
            ? SDL_FRect{platform.x(), platform.y(), static_cast<float>(platform.width()),
                        static_cast<float>(MovingPlatform::kHeight)}
            : SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
    }
    mario_.setPlatforms(&platformRects_);
    luigi_.setPlatforms(&platformRects_);
}

void Game::updatePlatformRiders(Mario& character, int& riding) {
    riding = character.isInDeathSequence() ? -1 : character.platformIndex();
    if (riding >= 0) platforms_[static_cast<size_t>(riding)].onStoodOn();
}

void Game::renderCharacter(const Mario& character, int riding) {
    if (riding < 0 || !platforms_[static_cast<size_t>(riding)].isActive()) {
        character.render(renderer_, cameraX_);
        return;
    }
    // X en pantalla = la de la plataforma (mismo redondeo que
    // MovingPlatform::render) + la distancia redondeada hasta ella. Se
    // pasa como una camara ajustada para que Mario::render, que redondea
    // x - camara, llegue exactamente a ese entero.
    const MovingPlatform& platform = platforms_[static_cast<size_t>(riding)];
    const long screenX = std::lround(platform.x() - cameraX_) + std::lround(character.x() - platform.x());
    character.render(renderer_, character.x() - static_cast<float>(screenX));
}

void Game::renderOutsideAreaMask() {
    if (currentArea_ < 0) return;
    const LevelArea& area = level_.areas()[static_cast<size_t>(currentArea_)];
    const int ts = level_.tileSize();
    const int camOffset = static_cast<int>(std::lround(cameraX_));
    const int left = area.startCol * ts - camOffset;
    const int right = (area.endCol + 1) * ts - camOffset;
    const SDL_Color color = backgroundColorFor(worldType_);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, 255);
    if (left > 0) {
        const SDL_Rect band{0, 0, left, kWorldViewBottom};
        SDL_RenderFillRect(renderer_, &band);
    }
    if (right < gLogicalWidth) {
        const SDL_Rect band{right, 0, gLogicalWidth - right, kWorldViewBottom};
        SDL_RenderFillRect(renderer_, &band);
    }
}

void Game::toggleFullscreen() {
    const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (SDL_SetWindowFullscreen(window_, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
        SDL_Log("No se pudo cambiar a pantalla completa: %s", SDL_GetError());
        return;
    }
    SDL_ShowCursor(fullscreen ? SDL_ENABLE : SDL_DISABLE);
    // Alt+Enter tambien queda guardado como la opcion "Pantalla" del menu.
    if (settings_.fullscreen == fullscreen) {
        settings_.fullscreen = !fullscreen;
        settings_.save();
    }
}

void Game::startWithLevelFile(const std::string& path, int startCol) {
    customLevelPath_ = path;
    customStartCol_ = startCol;
    startScenario(TestScenario::CustomLevel);
}

void Game::beginWorldRender() {
    // El mundo se dibuja en una textura del alto del nivel (16 filas) y a la
    // pantalla van las 15 de abajo (ver kWorldViewOffsetY).
    int width = 0;
    if (worldTarget_) SDL_QueryTexture(worldTarget_, nullptr, nullptr, &width, nullptr);
    if (!worldTarget_ || width != gLogicalWidth) {
        if (worldTarget_) SDL_DestroyTexture(worldTarget_);
        worldTarget_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, gLogicalWidth,
                                         kWorldViewBottom);
        if (worldTarget_) SDL_SetTextureScaleMode(worldTarget_, SDL_ScaleModeNearest);
    }
    if (!worldTarget_) return;  // sin soporte de render targets: se dibuja directo (sin el corrimiento)
    SDL_SetRenderTarget(renderer_, worldTarget_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
}

void Game::endWorldRender() {
    if (!worldTarget_) return;
    SDL_SetRenderTarget(renderer_, nullptr);
    const SDL_Rect src{0, kWorldViewOffsetY, gLogicalWidth, kLogicalHeight};
    const SDL_Rect dst{0, 0, gLogicalWidth, kLogicalHeight};
    SDL_RenderCopy(renderer_, worldTarget_, &src, &dst);
}
