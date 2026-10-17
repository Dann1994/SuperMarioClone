#pragma once

#include <SDL.h>

#include <array>
#include <set>
#include <memory>
#include <functional>
#include <vector>
#include <string>

#include "../audio/SoundManager.h"
#include "../entities/Axe.h"
#include "../entities/BlockCoin.h"
#include "../entities/Blooper.h"
#include "../entities/Bowser.h"
#include "../entities/BowserFire.h"
#include "../entities/BrickDebris.h"
#include "../entities/BulletBill.h"
#include "../entities/BuzzyBeetle.h"
#include "../entities/CheepCheep.h"
#include "../entities/Coin.h"
#include "../entities/FireFlower.h"
#include "../entities/Fireball.h"
#include "../entities/Flagpole.h"
#include "../entities/Goomba.h"
#include "../entities/Hammer.h"
#include "../entities/HammerBro.h"
#include "../entities/Koopa.h"
#include "../entities/Lakitu.h"
#include "../entities/Mario.h"
#include "../entities/MovingPlatform.h"
#include "../entities/Mushroom.h"
#include "../entities/PiranhaPlant.h"
#include "../entities/Podoboo.h"
#include "../entities/ScorePopup.h"
#include "../entities/Spiny.h"
#include "../entities/StarItem.h"
#include "../render/SpriteSheet.h"
#include "../world/Tilemap.h"
#include "../world/WorldType.h"
#include "Input.h"
#include "Settings.h"
#include "Text.h"

// Resolucion interna: el alto del NES (240, asi los niveles no cambian) con
// ancho 16:9 (240 * 16 / 9 = 426.67 -> 426) o 4:3 (256, el original),
// segun la opcion "Proporcion" del menu de configuracion (seccion 10.2, ver
// Game::applySettings) -- por eso es una variable y no una constante. La
// ventana puede ser mas grande, SDL2 escala automaticamente (este valor
// logico no cambia con el tamano de la ventana). Los niveles mas angostos
// que la pantalla se centran (ver Game::updateCamera/renderOutsideAreaMask).
constexpr int kWideLogicalWidth = 426;
inline int gLogicalWidth = kWideLogicalWidth;
constexpr int kLogicalHeight = 240;
// Los niveles miden 16 filas (256 px) y la pantalla 240: la vista muestra
// las 15 de abajo (como el original, con el HUD encima de las primeras
// filas de cielo), asi se ven las dos filas de piso. La fila 0 queda fuera
// de cuadro. Ver Game::beginWorldRender/endWorldRender.
constexpr int kWorldViewOffsetY = 16;
constexpr int kWorldViewBottom = kLogicalHeight + kWorldViewOffsetY;
// Ancho del layout original (HUD, menu): se reparte/centra en gLogicalWidth.
constexpr int kClassicWidth = 256;

// Maximo de bolas de fuego en pantalla por personaje, como en el original.
constexpr int kMaxFireballsPerPlayer = 2;

// Maximo de martillos en pantalla a la vez (pool, mismo criterio que
// Fireball), compartido entre HammerBro (2 alcanzan de sobra, ver
// HammerBro::kThrowInterval) y la rafaga de Bowser (ver
// Bowser::kHammerBurstCount -- con intervalo corto entre cada uno,
// facilmente hay varios en pantalla a la vez antes de que los primeros
// salgan por el borde).
constexpr int kMaxHammers = 8;

// Huevos de Spiny en pantalla a la vez (pool, mismo criterio que Hammer):
// a diferencia del martillo, el Spiny no desaparece solo al eclosionar y
// puede quedar caminando bastante tiempo, asi que hace falta mas margen que
// los 2 del Hammer Bro para que el Lakitu de prueba no se quede sin tirar
// mientras los anteriores siguen vivos.
constexpr int kMaxSpinies = 4;

// Cheep Cheep Jumping en pantalla a la vez (pool, mismo criterio que
// Spiny/Hammer): con el intervalo entre apariciones (ver
// kCheepCheepJumpIntervalMin/Max en Game.cpp) alcanza para que se vean
// varias saltando mientras el personaje camina sin que el timer se quede
// sin lugar en el pool.
constexpr int kMaxCheepCheepJumps = 3;

// Fuegos de Bowser en pantalla a la vez (pool, mismo criterio que
// Hammer/hammers_): alcanza con 2 dado el intervalo entre ataques (ver
// Bowser::kAttackIntervalMin/Max).
constexpr int kMaxBowserFires = 2;

// Pedazos volando a la vez (pool): 4 por ladrillo roto (una esquina cada
// uno), con margen para dos roturas casi juntas sin robarse el pool.
constexpr int kMaxBrickDebris = 8;

// Sacudidas de ladrillo (chico golpeandolo sin romperlo) activas a la vez.
constexpr int kMaxBlockBumps = 2;
// Cuanto sube y cuanto dura la sacudida (sube y vuelve a bajar en este lapso).
constexpr float kBumpDuration = 0.22f;
constexpr float kBumpHeight = 6.0f;

// Una sacudida en curso: que tile y cuanto le queda (ver Game::bumpBrickBlock).
// hasPendingItem/pendingMovingRight: solo lo usa checkQuestionBlockHit (ver
// ese comentario) para que el item recien aparezca cuando termina de
// sacudirse el bloque, en vez de al toque -- bumpBrickBlock (ladrillo comun,
// sin item) nunca los toca, quedan en false.
struct BlockBump {
    bool active = false;
    int col = 0;
    int row = 0;
    float timer = 0.0f;
    bool hasPendingItem = false;
    bool pendingMovingRight = false;
    // Si el que lo golpeo ya era grande: decide hongo o flor en un bloque
    // BlockItem::PowerUp (como el original).
    bool pendingHitterBig = false;
    // Quien lo golpeo (0 Mario, 1 Luigi): a el van la moneda o los puntos.
    int hitter = 0;
};

// Que item le toca a cada bloque de interrogacion (seccion 5.3) en el
// escenario de Items: uno por item de la fase inicial (5.4), asignado por
// posicion de tile en vez de una entidad de bloque propia por instancia --
// el bloque en si (Tilemap::isQuestionTile/isQuestionUsed) no sabe nada de
// items, es Game::checkQuestionBlockHit quien busca aca cual le toca a la
// celda que golpearon.
enum class BlockItem {
    Mushroom,
    FireFlower,
    Star,
    Coin,
    OneUp,
    // El bloque de power-up clasico: hongo si el que lo golpea es chico,
    // flor si ya es grande (se resuelve al salir, ver
    // Game::spawnItemFromQuestionBlock).
    PowerUp,
    // Enredadera: crece hacia arriba desde el bloque (ver Game::Vine).
    Vine,
    // Flores de los trajes (no son del original): martillo y bumeran.
    HammerFlower,
    BoomerangFlower,
};

struct QuestionBlockSpawn {
    int col;
    int row;
    BlockItem item;
    // Cuantas veces se puede golpear antes de quedar "usado" para siempre
    // (ver Tilemap::questionHitCount/dispenserHitCount): 1 por defecto
    // (todos los items excepto Coin), asi las entradas existentes no
    // necesitan tocarse. Un bloque "de N monedas" clasico (pedido del
    // usuario, investigado: en el original de 1985 en realidad es por
    // tiempo entre golpes, no por cantidad fija -- se simplifica aca a un
    // numero fijo de golpes, mas facil de parametrizar y de probar) pone
    // esto en N con item = Coin.
    int maxHits = 1;
    // Vine: adonde lleva (en tiles, -1 = a ningun lado) y fila de los pies
    // al llegar trepando.
    int destCol = -1;
    int destRow = -1;
};

// A que tuberia le corresponde cada entrada valida (Tilemap::isPipeTopTile)
// del nivel, y a donde manda -- ver Game::checkPipeWarp. Sale del campo
// "warps" del JSON del nivel (ver loadLevelData).
struct PipeWarp {
    int col;      // columna izquierda del tope de la tuberia (donde hay que pararse)
    int row;      // fila del tope
    float destX;
    float destY;
    // Boca horizontal (tuberia en L): col/row es la esquina superior
    // izquierda de la boca (2x2 tiles) y se entra caminando a la derecha.
    bool side = false;
    // Warp zone: nivel de destino ("4-1"), vacio si es un warp normal.
    std::string level;
};

// Cuanto tarda en "salir" del todo un item que emerge de un bloque (ver
// ItemEmerge mas abajo) -- Mushroom/FireFlower/Star no tienen animacion de
// emerger propia (ningun otro item/enemigo del elenco la tiene tampoco), asi
// que Game.cpp la simula moviendolo el mismo de golpeY_ inicial (a la altura
// del bloque, tapado por el Z-order) hasta su altura final, sin dejar que
// actualice su propia fisica mientras tanto (ver Game::update). BlockCoin es
// la excepcion: tiene su propio salto (ver BlockCoin.h), no pasa por aca.
constexpr float kItemEmergeDuration = 0.3f;

// Item emergiendo de un bloque en este momento (ver el comentario de
// kItemEmergeDuration) -- uno solo a la vez alcanza para este escenario de
// prueba (un personaje no puede golpear dos bloques en el mismo instante).
struct ItemEmerge {
    bool active = false;
    BlockItem item = BlockItem::Mushroom;
    float x = 0.0f;
    float startY = 0.0f;  // altura del bloque: ahi arranca, tapado por el Z-order
    float endY = 0.0f;    // altura final de descanso, arriba del bloque
    float timer = 0.0f;
    bool movingRight = true;
};

// Escenarios de prueba (para no amontonar items y enemigos en un solo nivel
// mientras no existe el editor de niveles real, seccion 7): se eligen desde
// una pantalla simple al arrancar (ver Game::renderScenarioSelect).
enum class TestScenario {
    Items,
    Enemies,
    HammerBro,
    LakituSpiny,
    Blooper,
    CheepCheep,
    BulletBill,
    // Escenario largo con scroll horizontal (a diferencia de todos los
    // demas, de una sola pantalla): el personaje camina normal (worldType_
    // Overworld) mientras Cheep Cheep Jumping van saltando desde abajo del
    // borde inferior de pantalla a medida que avanza (ver
    // cheepCheepJumps_/cheepCheepJumpSpawnTimer_).
    CheepCheepJump,
    Podoboo,
    Bowser,
    // Tramo final de un nivel (escalera + bandera de meta, como el 1-1),
    // test_flagpole.json. Sin tecla numerica: se elige con la flecha.
    Flagpole,
    // Plataformas moviles de todos los tipos (test_platforms.json). Sin
    // tecla numerica: se elige con la flecha.
    Platforms,
    // El primer nivel real: levels/world1/1-1.json, todo sale del JSON
    // (enemigos, bloques, tuberias, cuarto bonus, escenografia).
    World1_1,
    // Un archivo de nivel cualquiera, pasado con --level (lo usa el editor
    // de niveles para probar lo que se esta editando). No aparece en el
    // selector.
    CustomLevel,
};

// Cantidad de opciones del selector de escenario (las anteriores a CustomLevel).
constexpr int kMenuOptionCount = static_cast<int>(TestScenario::World1_1) + 1;

enum class AppState {
    // Pantalla de titulo con el menu principal (seccion 10.1).
    Title,
    // Modo prueba: el selector de escenarios de prueba.
    ScenarioSelect,
    // Pantalla negra "WORLD x-y / MARIO x N" antes de cada intento, como el
    // original (al empezar el nivel y despues de perder una vida).
    LevelIntro,
    Playing,
    // Se acabaron las vidas: cartel de GAME OVER y el nivel arranca de cero.
    GameOver,
    // Menu de pausa (seccion 10.3), con el juego congelado atras.
    Paused,
    // Configuracion (seccion 10.2) y su submenu de controles.
    Settings,
    Controls,
    Credits,
    // Despues del ultimo nivel que existe: "fin de la demo".
    DemoEnd,
    // Al terminar la partida: el resumen, el nombre para la tabla y la tabla.
    Summary,
    // La tabla de puntajes (PUNTAJES, desde el menu principal).
    HighScores,
};

// Caja de choque enemigo-jugador del original (ver originalEnemyBox en Game.cpp).
enum class EnemyBox { Normal, Small, HammerBro };

class LevelEditor;

class Game {
public:
    // Definidos en Game.cpp (editor_ es un unique_ptr de un tipo que aca
    // solo esta declarado).
    Game();
    ~Game();
    bool init();
    // Arranca directo en este archivo de nivel (--level, ver main.cpp) en
    // vez de en el selector. Llamar antes de run().
    // startCol >= 0: arranca en esa columna como si fuera un checkpoint
    // ("probar desde aca" del editor).
    void startWithLevelFile(const std::string& path, int startCol, int startRow = -1);
    // Herramienta de revision: al empezar el nivel guarda el mapa entero
    // (escenario y bloques, sin personajes) como BMP y cierra el juego.
    void setDumpMap(const std::string& path) { dumpMapPath_ = path; }
    void run();
    void shutdown();

private:
    void processEvents();
    void update(float dt);
    void render();
    void renderScenarioSelect();
    void moveMenuSelection(int delta);
    // Partida nueva en el escenario elegido: puntaje, monedas, vidas y
    // checkpoint de cero, y despues loadScenario (con la pantalla de intro).
    void startScenario(TestScenario scenario);
    // Carga el nivel del escenario y deja en pie solo las entidades de
    // prueba que le corresponden (las del otro escenario quedan
    // desactivadas, aunque el objeto siga vivo en memoria). Los
    // personajes arrancan en el ultimo checkpoint alcanzado (o en el spawn
    // del nivel si no hay). No toca puntaje/monedas/vidas: lo reusa el
    // reintento despues de perder una vida.
    void loadScenario(TestScenario scenario);
    // Murieron todos (en 1P, Mario): pierde una vida y reintenta desde el
    // checkpoint, o GAME OVER si no quedan.
    void onAllPlayersDied();
    // Marca como alcanzado el checkpoint mas adelantado que haya pasado
    // algun personaje vivo.
    void updateCheckpoints();
    // Caer por debajo del nivel (un pozo) mata, sin importar el tamano.
    void checkPitDeath(Mario& character);
    void renderLevelIntro();
    void renderGameOver();
    void trySpawnFireball(Mario& shooter, std::array<Fireball, kMaxFireballsPerPlayer>& pool);
    void resolveCharacterCollision();
    void catchBoomerang(const Mario& thrower, Fireball& fireball);
    void checkMushroomPickup(Mario& character);
    void checkFireFlowerPickup(Mario& character);
    void checkStarPickup(Mario& character);
    void checkCoinPickup(Mario& character);
    // Monedas sueltas del nivel (tiles 'o', ver Tilemap::isCoinTile): las
    // que toque el hitbox se agarran.
    void checkCoinTiles(Mario& character);
    // Bajo el agua Mario nada libremente (no cae por gravedad como en
    // tierra), asi que la geometria del pisoton -- que necesita "venir
    // cayendo desde arriba" -- casi nunca se da nadando: por pedido
    // explicito del usuario, ahi se desactiva el pisoton del todo y
    // cualquier contacto hace dano como un toque de costado (en el resto de
    // los mundos el pisoton sigue igual que siempre). Devuelve true si ya
    // aplico el dano y el llamador tiene que cortar ahi (no seguir con la
    // logica de pisoton); false si no es Underwater y el llamador sigue
    // normal. Un solo lugar para este criterio en vez de repetirlo en cada
    // checkXCollision.
    bool handleUnderwaterContactDamage(Mario& character);
    // Dano de contacto "normal" (tocar a un enemigo/hazard de costado, no
    // pisandolo ni con estrella): chico muere, grande/fuego se achica -- con
    // su sonido correspondiente (Sfx::Death/Sfx::PowerDown), solo si
    // realmente arranco la secuencia (startDeath()/takeDamage() devuelven
    // false si ya estaba en una o ya era invulnerable). Un solo lugar para
    // este mismo if/else que se repetia en cada checkXCollision.
    void applyContactDamage(Mario& character);
    // Mata a un enemigo "volteandolo" (fuego, estrella, caparazon, o un
    // pisoton en los que no tienen animacion de achatado propia -- ver el
    // comentario de la clase de cada uno): reproduce Sfx::Kick una sola vez,
    // solo si killFalling() realmente arranco la secuencia esta vez (false
    // si ya estaba cayendo, evita que suene varias veces seguidas si algo lo
    // toca de nuevo mientras todavia esta en el aire). Investigado en el
    // disassembly original: es el mismo sonido/mecanismo ahi tambien
    // ("ShellOrBlockDefeat"), sin importar la causa. Template porque cada
    // clase de enemigo es un tipo distinto, todas comparten la firma
    // bool killFalling(bool).
    // Suma los puntos del enemigo (popup incluido, ver enemyPoints) la
    // misma vez que suena.
    template <typename T>
    void killEnemyFalling(T& enemy, bool flyRight) {
        if (!enemy.killFalling(flyRight)) return;
        noteEnemyKilled();
        audio_.play(Sfx::Kick);
        awardEnemyPoints(enemy.x(), enemy.y(), enemyPoints(enemy));
    }
    // Puntos por enemigo, valores del original: 100 por defecto, los
    // "grandes" valen mas.
    template <typename T>
    static int enemyPoints(const T&) { return 100; }
    static int enemyPoints(const HammerBro&) { return 1000; }
    static int enemyPoints(const Lakitu&) { return 800; }
    static int enemyPoints(const Blooper&) { return 1000; }
    static int enemyPoints(const CheepCheep&) { return 200; }
    static int enemyPoints(const BulletBill&) { return 200; }
    // Popup + puntaje (spawnScorePopup los suma) sobre el enemigo.
    void awardEnemyPoints(float x, float y, int points);
    // Puntos de la tabla del original por indice: 1=100, 2=200, 3=400,
    // 4=500, 5=800, 6=1000, 7=2000, 8=4000, 9=5000, 10=8000, 11 o mas = 1-UP.
    void awardIndexedPoints(float x, float y, int index);
    // Pisoton: la cadena sube con cada enemigo pisado sin tocar el piso
    // (100, 200, 400...); se reinicia al apoyarse (ver Game::update).
    void awardStompPoints(Mario& character, float x, float y);
    // Patear un caparazon: 400, salvo que el Koopa este por salir (500,
    // 1000 y 8000 en el ultimo instante, como el original).
    int kickShellPointsIndex(float idleRemaining) const;
    // Contacto personaje-Koopa/Buzzy Beetle con las reglas del original
    // (ver checkKoopaCollision).
    template <typename Shell>
    void resolveShellEnemyContact(Mario& character, Shell& enemy, bool shellIdle);
    // Cuando se puede pisar a un enemigo (PlayerEnemyCollision del original):
    // Falling = solo cayendo (Goomba, Koopa, Hammer Bro); FallingOrBelow =
    // cayendo, o si el enemigo esta claramente mas abajo aunque el jugador
    // suba (Lakitu, Bill Bala, Cheep Cheep, Blooper); Never = siempre lastima.
    enum class StompRule { Falling, FallingOrBelow, Never };
    static bool isStompContact(const Mario& character, float enemyTop, StompRule rule);
    // Agachado con el traje de Hammer Bro: lo que le cae por arriba (el
    // borde de abajo en la mitad de arriba del caparazon) no lo lastima.
    static bool shellBlocksFromAbove(const Mario& character, float objectBottom) {
        return character.isHammerShell() &&
               objectBottom <= character.y() + static_cast<float>(character.height()) / 2.0f;
    }
    // Contacto generico con un enemigo que al pisarlo cae volteado
    // (killFalling): estrella lo voltea, pisoton lo voltea con rebote,
    // si no lastima. Un contacto por encuentro (ver beginContact).
    template <typename T>
    void resolveStompableContact(Mario& character, T& enemy, StompRule rule, float bounce,
                                 EnemyBox box = EnemyBox::Normal);
    // Contacto unico por encuentro (como el original): true solo el primer
    // cuadro en que se tocan; endContact cuando dejan de tocarse.
    bool beginContact(const void* enemy, const void* character);
    void endContact(const void* enemy, const void* character);
    // Un enemigo muerto por un caparazon: puntos de la cadena del caparazon.
    template <typename T, typename Shell>
    void killByShell(T& enemy, bool flyRight, Shell& shell) {
        if (!enemy.killFalling(flyRight)) return;
        noteEnemyKilled();
        audio_.play(Sfx::Kick);
        setScorerFromKicker(shell.kicker());
        awardIndexedPoints(enemy.x(), enemy.y(), shell.takeShellChain() + 4);
    }
    // Pisoton (achata y hace rebotar) o dano de costado (como el hazard de
    // prueba), segun de donde venga el contacto.
    // prevY: character.y() de ANTES de moverse este frame (ver el comentario
    // en Game::update junto a marioPrevY) -- asi se sabe si venia de arriba
    // sin importar cuanto se haya hundido este frame en particular.
    void checkGoombaCollision(Mario& character, Goomba& goomba, float prevY);
    // Fuego: misma muerte que la estrella (se da vuelta y cae, ver
    // Goomba::killFalling), y la bola explota ahi en vez de seguir volando.
    void checkFireballGoombaCollision(Fireball& fireball, Goomba& goomba);
    // Segun el estado del Koopa: Walking -> pisoton lo esconde (con rebote)
    // o dano de costado; ShellIdle -> pisoton o costado lo patean por igual
    // (de costado hacia el lado contrario a quien lo toco, de arriba hacia
    // donde mira el personaje), sin rebote (para no matar a Mario/Luigi por
    // seguir cayendo sobre el caparazon ya pateado, ver la nota en el .cpp);
    // ShellMoving -> pisoton lo frena (con rebote), costado hace dano.
    // Estrella lo mata (killFalling) en cualquier estado, como al Goomba.
    void checkKoopaCollision(Mario& character, Koopa& koopa, float prevY);
    // Fuego: mata al Koopa en cualquier estado (igual que la estrella), y la
    // bola explota ahi en vez de seguir volando.
    void checkFireballKoopaCollision(Fireball& fireball, Koopa& koopa);
    // Un caparazon en movimiento mata a cualquier otro enemigo que toque y
    // sigue de largo -- este chequeo es especificamente contra el Goomba de
    // prueba (checkShellVsKoopa mas abajo cubre Koopa contra Koopa).
    void checkShellVsGoomba(Koopa& koopa, Goomba& goomba);
    // Si los dos son caparazon en movimiento, se matan entre si; si solo uno
    // lo es, mata al otro (en cualquier estado) y sigue de largo.
    void checkShellVsKoopa(Koopa& a, Koopa& b);
    // Si NINGUNO es caparazon en movimiento (los dos caminando, o uno/ambos
    // caparazon quieto), no se matan: se cruzan como si chocaran una pared,
    // cada uno invierte su propia direccion. Mismo criterio para agregar
    // cuando haya mas de un Goomba, o mas tipos de enemigo.
    void checkGoombaKoopaBounce(Goomba& goomba, Koopa& koopa);
    void checkKoopaKoopaBounce(Koopa& a, Koopa& b);
    // Buzzy Beetle: misma logica que checkKoopaCollision (incluida la regla
    // estricta del caparazon en movimiento). El caso de fuego es aparte
    // (ver checkFireballBuzzyBeetleCollision): es inmune, no hay pisoton por
    // fuego.
    void checkBuzzyBeetleCollision(Mario& character, BuzzyBeetle& buzzy, float prevY);
    // Es inmune al fuego (ver BuzzyBeetle.h): la bola explota igual al
    // tocarlo (mismo feedback visual que contra cualquier otro enemigo),
    // pero no le hace nada -- sigue en el estado en el que estaba.
    void checkFireballBuzzyBeetleCollision(Fireball& fireball, BuzzyBeetle& buzzy);
    // Un caparazon en movimiento (de cualquiera de los dos) mata al otro;
    // si los dos lo son, mueren ambos -- mismo criterio que checkShellVsKoopa.
    void checkShellVsBuzzyBeetle(Koopa& koopa, BuzzyBeetle& buzzy);
    // El caparazon del Buzzy Beetle contra el Goomba de prueba (Goomba no
    // tiene caparazon propio para reciprocar, igual que en checkShellVsGoomba).
    void checkBuzzyShellVsGoomba(BuzzyBeetle& buzzy, Goomba& goomba);
    void checkGoombaBuzzyBeetleBounce(Goomba& goomba, BuzzyBeetle& buzzy);
    void checkKoopaBuzzyBeetleBounce(Koopa& koopa, BuzzyBeetle& buzzy);
    // Sin pisoton (ver PiranhaPlant.h): cualquier contacto mientras esta
    // asomada hace dano, salvo estrella que la mata.
    void checkPiranhaPlantCollision(Mario& character, PiranhaPlant& plant);
    // Fuego si la mata (a diferencia del Buzzy Beetle): desaparece de una,
    // sin parabola (no tiene adonde volar), y la bola explota ahi.
    void checkFireballPiranhaPlantCollision(Fireball& fireball, PiranhaPlant& plant);
    // No se asoma si algun personaje esta cerca del cano -- esta clase no
    // conoce a Mario/Luigi, asi que Game.cpp resuelve la distancia cada
    // frame y le pasa el resultado a PiranhaPlant::update().
    bool isSafeForPiranhaPlant(const PiranhaPlant& plant) const;
    // Cualquier forma de morir lo mata igual (ver HammerBro.h): pisoton,
    // estrella o fuego todos disparan la misma parabola de caida.
    void checkHammerBroCollision(Mario& character, HammerBro& bro, float prevY);
    void checkFireballHammerBroCollision(Fireball& fireball, HammerBro& bro);
    // El martillo en vuelo hace dano a cualquier personaje que toque (sin
    // pisoton, como la Pirana Plant); con estrella no lo mata -- no es un
    // enemigo, solo desaparece sin hacer dano (ver Hammer::reset).
    void checkHammerCollision(Mario& character, Hammer& hammer);
    // Cualquier forma de matarlo usa la misma parabola (ver Lakitu.h): a
    // diferencia de la Pirana Plant, aca el pisoton SI lo mata.
    void checkLakituCollision(Mario& character, float prevY);
    void checkFireballLakituCollision(Fireball& fireball);
    // Sin pisoton (ver Spiny.h): cualquier contacto hace dano, en Egg o en
    // Walking por igual, sin importar de que lado venga -- solo estrella o
    // fuego lo eliminan.
    void checkSpinyCollision(Mario& character, Spiny& spiny);
    void checkFireballSpinyCollision(Fireball& fireball, Spiny& spiny);
    // Si los dos estan caminando (no huevo en el aire, ver SpinyState) y se
    // cruzan, rebotan como si chocaran una pared -- mismo criterio que
    // checkGoombaKoopaBounce, no se matan entre si (Spiny no tiene
    // caparazon que pueda matar a otro enemigo).
    void checkSpinyBounce(Spiny& a, Spiny& b);
    // Sin pisoton (ver Blooper.h): cualquier contacto hace dano, solo
    // estrella o fuego lo matan.
    void checkBlooperCollision(Mario& character, Blooper& blooper);
    void checkFireballBlooperCollision(Fireball& fireball, Blooper& blooper);
    // Pisoton, estrella o fuego: cualquiera lo mata igual (ver CheepCheep.h),
    // en las dos variantes.
    void checkCheepCheepCollision(Mario& character, CheepCheep& cheepCheep, float prevY);
    void checkFireballCheepCheepCollision(Fireball& fireball, CheepCheep& cheepCheep);
    // Busca un slot libre en cheepCheepJumps_ y lo spawnea desde abajo del
    // borde de pantalla, cerca del personaje mas adelantado: adelante si
    // esta caminando, atras si esta corriendo (comportamiento real de
    // World 2-3/7-3/8-4 de SMB1, ver el comentario de CheepCheep.h) con una
    // distancia al azar (kCheepCheepJumpAheadOffsetMin/Max). No hace nada si
    // el pool esta lleno. Lo llaman tanto el timer automatico
    // (Game::update) como la tecla J de prueba.
    void spawnNextCheepCheepJump();
    // A diferencia del resto del elenco reciente (Blooper/Spiny/Pirana
    // Plant, sin pisoton), el Bill Bala SI se puede pisar como el elenco
    // clasico (ver BulletBill.h): pisoton, estrella o fuego lo matan igual.
    void checkBulletBillCollision(Mario& character, BulletBill& bulletBill, float prevY);
    void checkFireballBulletBillCollision(Fireball& fireball, BulletBill& bulletBill);
    // Unico enemigo del elenco sin ningun metodo de muerte (ver Podoboo.h):
    // cualquier contacto mientras esta afuera de la lava hace dano, sin
    // pisoton ni excepcion por estrella/fuego (la estrella sigue protegiendo
    // a Mario/Luigi del dano como siempre, pero el Podoboo no desaparece).
    void checkPodobooCollision(Mario& character, Podoboo& podoboo);
    // La bola explota igual al tocarlo (mismo feedback que contra Buzzy
    // Beetle), pero no le hace nada.
    void checkFireballPodobooCollision(Fireball& fireball, Podoboo& podoboo);
    // Sin pisoton, ni siquiera la estrella lo derrota (ver Bowser.h): la
    // estrella sigue protegiendo del dano como siempre, pero a Bowser no le
    // pasa nada -- solo killByFireball()/killByAxe() (ver mas abajo) lo
    // matan de verdad.
    void checkBowserCollision(Mario& character);
    // A diferencia de checkFireballXCollision del resto del elenco: SI
    // cuenta para derrotarlo (Bowser::killByFireball, cinco seguidas), en
    // vez de matarlo de una.
    void checkFireballBowserCollision(Fireball& fireball);
    // El fuego que escupe: mismo criterio que Hammer/checkHammerCollision,
    // sin pisoton y sigue de largo (no desaparece al tocar).
    void checkBowserFireCollision(Mario& character, BowserFire& fire);
    // Tocar el hacha derrota a Bowser de una sola vez (ver Axe.h) y hace
    // desaparecer el hacha misma.
    void checkAxeCollision(Mario& character, Axe& axe);
    // Puente de Bowser (ver Tilemap::isBridgeTile): al tocar el hacha todo
    // se congela y el puente se derrumba de a un tramo, de la punta del
    // hacha hacia atras; al terminar Bowser cae a la lava, el juego sigue y
    // a los kCastleClearDelay segundos arranca el cierre del nivel (cuenta
    // del tiempo + cartel, ver updateLevelClear).
    void startBridgeCollapse();
    // Pelea terminada (puente caido o hacha con Bowser ya derrotado): la
    // musica del castillo y la caminata hasta el Toad / la princesa.
    void finishCastleFight();
    // Al terminar de caer el puente: el que quedo sobre el hueco (toco el
    // hacha saltando) va parado al piso del hacha.
    void landOnAxeGround();
    void updateBridgeCollapse(float dt);
    // Vuelve el castillo de prueba al estado inicial (teclas Y/T).
    void resetCastleSequence();
    // Tocar la lava mata (sin el saltito, como caer a un pozo).
    void checkLavaDeath(Mario& character);
    // Plataformas moviles (ver MovingPlatform.h): las mueve, arrastra a los
    // que estaban parados encima el cuadro anterior y actualiza
    // platformRects_ (lo que Mario::moveAndCollide usa para apoyarse).
    // Despues de mover a los personajes, updatePlatformRiders anota en cual
    // quedo cada uno (riding, -1 si en ninguna) y avisa a la que cae.
    void updatePlatforms(float dt);
    void updatePlatformRiders(Mario& character, int& riding);
    // Cuenta del tiempo que sobro + cartel de nivel completo, sea por la
    // bandera o por el hacha del castillo.
    void updateLevelClear(float dt);
    bool isFinalCastle() const;
    void finishFinalEnding();
    // Bandera de meta (seccion 5.3, ver Flagpole.h): si el personaje toca el
    // mastil lo agarra (Mario::startFlagpoleGrab), suena flagpole.wav, sale
    // el puntaje segun la altura del agarre y la bandera empieza a bajar.
    void checkFlagpoleGrab(Mario& character);
    // Avanza la bandera, suelta a los que esperan abajo cuando ya bajo, y
    // arranca el cierre del nivel (levelClearTimer_) cuando el primero
    // entra al castillo.
    void updateFlagpoleSequence(float dt);

    // Puntaje/monedas/timer (seccion 6) y el HUD clasico que los muestra
    // (renderHud: MARIO + puntaje, monedas, WORLD + nombre del nivel, TIME).
    // Suma al total (record, 1P) y a la cuenta del jugador que los gano
    // (scorer_, ver setScorer): en 2P el HUD muestra la de cada uno.
    void addScore(int points) {
        score_ += points;
        playerScore_[static_cast<size_t>(scorer_)] += points;
    }
    // A quien se le anotan los puntos y monedas que siguen: lo llaman las
    // funciones que actuan sobre un personaje (agarrar, pisar, golpear).
    void setScorer(const Mario& character) { scorer_ = &character == &luigi_ ? 1 : 0; }
    // Lo que mata un caparazon va para el que lo pateo (ver Koopa::kicker).
    void setScorerFromKicker(const void* kicker) { scorer_ = kicker == &luigi_ ? 1 : 0; }
    // Suma una moneda (+200 puntos, como el original) y cada 100 da una
    // vida extra -- unico lugar donde se cuentan, sea cual sea la moneda
    // (la del piso, la que salta de un bloque o la parada arriba de un
    // ladrillo).
    void collectCoin();
    // Vuelve el timer al tiempo limite del nivel (al empezarlo, o al
    // reaparecer en el checkpoint despues de que mueren todos).
    void resetLevelTimer();
    // Descuenta el tiempo en juego normal: al llegar a 100 avisa con
    // hurryup.wav, y en 0 mueren todos (Mario::killInstantly). Se frena en
    // cuanto alguien agarra la bandera.
    void updateLevelTimer(float dt);
    void renderHud();
    // Tuberia (seccion 5.3): si el personaje esta parado (grounded) justo
    // arriba del TOPE de una tuberia (Tilemap::isPipeTopTile) y aprieta
    // abajo, lo teletransporta al destino configurado en el campo "warps" del nivel (pipeWarps_) -- sin
    // animacion de "hundirse" (simplificacion), aparece de una en el
    // destino. input es la de ESE personaje (input1_/input2_), no la
    // recibe Mario/Luigi (no le hace falta saber de tuberias).
    void checkPipeWarp(Mario& character, Input& input);
    // Tuberias de boca horizontal (PipeWarp::side): se entra metiendose
    // caminando. En la escena de introduccion arranca la transicion (jingle
    // y despues el nivel); si no, viaja de una como las verticales.
    void checkSideWarp(Mario& character);
    // Mueve al personaje (y al companero en 2P) al destino del warp.
    void performWarp(Mario& character, const PipeWarp& warp);
    // Animacion de tuberia (como el original): el personaje se hunde en
    // la vertical o entra caminando en la lateral, con su sonido; despues
    // del viaje, si el destino esta sobre una tuberia, sale de ella. Todo
    // lo demas queda quieto mientras tanto.
    void startPipeEnter(Mario& character, const PipeWarp& warp);
    void updatePipeAnim(float dt);
    // Los bordes del area actual son paredes (no se puede salir del nivel).
    void keepCharacterInsideArea(Mario& character);
    void finishIntroPipe();
    // Ladrillo rompible (seccion 5.3): si Mario/Luigi saltando chocan de
    // cabeza contra un tile 'B' (ver Mario::headBumpedThisFrame), grande o
    // fuego lo rompe (breakBrickBlock); chico solo lo sacude sin romperlo
    // (bumpBrickBlock) -- moveAndCollide() ya se encarga de frenarle el
    // salto, esta funcion decide cual de las dos reacciones toca.
    void checkBrickBlockHit(Mario& character);
    // Lo rompe de verdad: saca el tile y tira 4 BrickDebris desde el pool.
    // El golpe en si (matar al enemigo de encima, si hay) es comun a este
    // caso y al de bumpBrickBlock, ver killEnemyOnTopOfTile.
    void breakBrickBlock(int col, int row);
    // No lo rompe: arranca la animacion corta de "sacudida" (sube unos px y
    // vuelve a bajar, ver blockBumps_/kBumpDuration) sin sacar el tile.
    void bumpBrickBlock(int col, int row);
    // Si el ladrillo (col, row) tiene una moneda parada arriba
    // (Tilemap::hasCoinOnTop, tile 'C') o una moneda suelta ('o') justo
    // encima (tambien de un '?' o bloque con item), la agarra: suma al contador y la
    // hace saltar con la animacion de BlockCoin, igual que una moneda de
    // bloque de interrogacion -- se llama sea que el ladrillo se rompio o
    // solo se sacudio, da igual (pedido del usuario).
    void collectTopCoinIfAny(int col, int row);
    // Comun a romper y sacudir (en el original, CUALQUIER golpe a un bloque
    // -- se rompa o no -- mata al enemigo que este parado justo encima):
    // mata a cualquier enemigo de prueba parado justo encima del tile
    // (col, row), con la misma parabola que usa el fuego/la estrella.
    void killEnemyOnTopOfTile(int col, int row);
    // Arranca (o reinicia si ya estaba en curso) la sacudida visual de
    // blockBumps_ para (col, row) -- extraido de bumpBrickBlock() para que
    // el bloque de interrogacion/dispensador lo pueda reusar sin duplicar la
    // logica del pool. Devuelve el slot usado (o nullptr si el pool estaba
    // lleno) para que checkQuestionBlockHit le pueda dejar cargado el item
    // pendiente (ver hasPendingItem en BlockBump).
    BlockBump* startBlockBump(int col, int row);

    // Bloque de interrogacion (seccion 5.3, tambien cubre el dispensador
    // 'D'): mismo mecanismo de deteccion que el ladrillo
    // (Mario::headBumpedThisFrame), se sacude y tira el item que le
    // corresponde a esa celda (ver findBlockSpawn)
    // la primera vez que lo golpean, quedando "usado" para siempre
    // (Tilemap::markQuestionUsed/markDispenserUsed) -- un golpe mas tarde no
    // hace nada, ni se sacude. El item recien aparece (spawnItemFromQuestionBlock)
    // cuando termina la sacudida, no al toque -- ver hasPendingItem en BlockBump.
    void checkQuestionBlockHit(Mario& character);
    // Que item le toca a la celda (col, row) segun findBlockSpawn, y lo
    // hace aparecer saliendo del bloque (arriba de el, ver el comentario de
    // la clase) usando la instancia de prueba correspondiente
    // (mushroom_/fireFlower_/star_/coin_) -- no hay pool, un item por bloque
    // alcanza para este escenario de prueba. movingRight: hacia donde arranca
    // movi/endose Mushroom/Star (FireFlower/BlockCoin no se mueven, lo
    // ignoran) -- estilo Super Mario Bros. 3 en vez del original de 1985
    // (que siempre iba a la derecha, sin excepcion): sale hacia el lado
    // OPUESTO de donde vino el golpe, lo decide checkQuestionBlockHit segun
    // de que lado del bloque esta el personaje que lo golpeo.
    void spawnItemFromQuestionBlock(int col, int row, bool movingRight, bool hitterBig);
    // Contenido del bloque (col, row) segun el campo "blocks" del nivel; un
    // '?'/'D' que no figura ahi tiene una moneda (lo mas comun en el
    // original).
    const QuestionBlockSpawn& findBlockSpawn(int col, int row) const;
    // Traduce los datos del nivel recien cargado (blocks/warps/entities del
    // JSON) a las estructuras de Game y deja todas las entidades sin
    // aparecer todavia (ver spawnLevelEntities).
    void loadLevelData();
    // Hace aparecer las entidades del nivel cuando la camara se les acerca
    // por la derecha (como el original: no estan "vivas" desde el
    // principio, si no un Goomba caminaria hacia la izquierda y se caeria
    // en un pozo antes de que el jugador llegue). Las que ya quedaron atras
    // de la camara al cargar (reaparecer en un checkpoint) no aparecen.
    void spawnLevelEntities();
    // Llama a spawnFn con el primer lugar libre del pool (false si esta lleno).
    template <typename T, size_t N, typename Fn>
    static bool spawnInPool(std::array<T, N>& pool, Fn spawnFn) {
        for (T& entity : pool) {
            if (entity.isAlive()) continue;
            spawnFn(entity);
            return true;
        }
        return false;
    }
    // Goomba/Koopa en el primer lugar libre de su pool (false si esta lleno).
    bool spawnGoomba(float x, float y, bool movingRight);
    bool spawnLevelKoopa(float x, float y, bool movingRight, KoopaColor color, KoopaState state,
                         bool horizontalFly = false);
    // Los 5 Koopa de prueba + los del pool del nivel, para los chequeos que
    // recorren todos.
    std::vector<Koopa*> allKoopas();
    // Canones de Bill Bala (tiles 'K', ver Tilemap::isCannonTopTile):
    // disparan hacia el personaje cada tanto mientras estan en pantalla y
    // nadie esta pegado a ellos.
    void updateCannons(float dt);
    // Barras de fuego de los castillos (entidad "firebar"): giran alrededor
    // de su bloque; cualquier bola que toque hace dano.
    void updateFirebars(float dt);
    void renderFirebars();
    // El personaje esta dentro de alguna zona "cheep_jump" del nivel.
    bool inCheepJumpZone() const;
    // El Lakitu del nivel se va al pasar su end_col.
    void updateLakituZone();
    // Area del nivel (Tilemap::areas) que contiene ese X en px.
    int areaIndexAt(float x) const;
    // front: las que van delante de los personajes (LevelDecoration::front).
    void renderDecorations(bool front);
    void beginWorldRender();
    void endWorldRender();
    // Alt+Enter: alterna entre ventana y pantalla completa "de escritorio"
    // (sin bordes, a la resolucion actual de la pantalla, sin cambiar el
    // modo de video -- Alt+Tab no parpadea). La imagen se escala sola
    // (SDL_RenderSetLogicalSize) manteniendo 16:9. El cursor se esconde en
    // pantalla completa.
    void toggleFullscreen();

    // --- Menus (seccion 10, implementados en GameMenus.cpp) ---
    // Navegacion comun a teclado y mando (seccion 10.4).
    enum class MenuInput { None, Up, Down, Left, Right, Confirm, Back };
    bool isMenuState() const;
    void handleMenuEvent(const SDL_Event& event);
    MenuInput menuInputFromEvent(const SDL_Event& event);
    void openTitle();
    void openScenarioSelect();
    void openPause();
    void openSettings(AppState returnTo);
    void handleTitleInput(MenuInput input);
    void handlePauseInput(MenuInput input);
    void handleSettingsInput(MenuInput input);
    void handleControlsInput(MenuInput input);
    // Cambia el valor de la opcion `index` del menu de configuracion
    // (direction -1/+1; Confirm usa +1).
    void changeSetting(int index, int direction);
    // Aplica settings_ a la ventana, el audio, el idioma y los controles.
    void applySettings();
    void applyWindowSize();
    void renderTitle();
    void renderPauseMenu();
    void renderSettings();
    void renderControls();
    void renderCredits();
    void renderDemoEnd();
    void drawCentered(int y, const std::string& text, int scale, SDL_Color color);
    // Lista vertical con flecha en la seleccion; disabled grisa la opcion.
    void drawMenuList(int x, int y, int step, const std::vector<std::string>& items, int selection,
                      const std::vector<bool>& disabled = {});

    // --- Partida (1 o 2 jugadores): niveles levels/world{W}/{W}-{L}.json en
    // orden; despues del ultimo que exista, "fin de la demo".
    void startCampaign(bool twoPlayers, bool asLuigi = false);
    // Centrado con la fuente del juego, en dos renglones si no entra.
    void drawCenteredFit(int y, const std::string& text, SDL_Color color);
    static std::string campaignLevelPath(int world, int level);
    // Ya se paso ese nivel de la campana (ELEGIR MUNDO).
    bool levelCleared(int world, int level) const {
        const std::string id = std::to_string(world) + "-" + std::to_string(level);
        return std::find(settings_.clearedLevels.begin(), settings_.clearedLevels.end(), id) !=
               settings_.clearedLevels.end();
    }
    void markLevelCleared();
    void countHiddenOneUps();
    void noteOneUpFound(int col, int row);
    // VIDAS INFINITAS (comprado y activado): en las partidas no se pierden vidas.
    bool infiniteLives() const {
        return campaign_ && settings_.extraOwned("infinite_lives") && settings_.extraEnabled("infinite_lives");
    }
    void onLevelCleared();
    // Vibracion al recibir dano o morir (opcion "Vibracion del mando").
    // Vibracion leve al pisar un enemigo (si la vibracion esta activada).
    void stompRumble(const Mario& character);
    // Le rebota un enemigo en el caparazon del traje de Hammer Bro.
    void shellBounceFeedback(const Mario& character);
    // Vibraciones con ritmo (una secuencia de pulsos por jugador, la nueva
    // reemplaza a la que estaba sonando): la de agarrar un item sigue las
    // notas de powerup.wav / 1up.wav, la de ser golpeado las de
    // pipepowerdown.wav; romper un ladrillo, fuerte; golpear uno que no se
    // rompe (o cualquier bloque), leve.
    struct RumbleStep {
        float at;          // segundos desde que arranca
        Uint16 strength;
        Uint32 ms;
    };
    enum class RumbleKind { Item, OneUp, Hurt, Death, BrickBreak, BlockBump };
    void startRumble(const Mario& character, RumbleKind kind);
    std::array<std::vector<RumbleStep>, 2> rumbleSteps_{};
    std::array<float, 2> rumbleClock_{};
    std::array<size_t, 2> rumbleNext_{};
    // Bajando por el mastil: vibra todo el deslizamiento (se renueva el
    // pulso cada tanto mientras dure).
    std::array<float, 2> flagRumbleTimer_{};
    // ~50%: mas bajo muchos mandos no llegan a mover el motor.
    static constexpr Uint16 kStompRumbleStrength = 0x8000;
    static constexpr Uint32 kStompRumbleMs = 100;
    void updateRumble(float dt);
    // Guarda el puntaje como record (TOP-) si lo supera.
    void saveTopScore();

    // --- Musica (GameMusic.cpp) ---
    void loadMusic();
    // El tema que corresponde ahora: el del tipo de mundo, o la estrella
    // (version rapida si queda poco tiempo).
    Music levelMusic() const;
    // Al (re)cargar el nivel: corta la musica y arranca de cero.
    void resetLevelMusic();
    // Llega a 100 de tiempo: corta, aviso de apuro y tema acelerado.
    void startHurryMusic();
    void updateMusic(float dt);
    static const char* musicName(Music music);
    void renderFpsCorner();
    // Tapa con el color de fondo lo que se ve fuera del area actual (la
    // pantalla 16:9 es mas ancha que los niveles de una sola pantalla y que
    // el cuarto bonus: sin esto se veria el nivel de al lado).
    void renderOutsideAreaMask();
    // Dibuja al personaje; si va parado en una plataforma movil (riding),
    // redondea su posicion relativa a ella en vez de a la camara: los dos
    // se mueven en px fraccionarios y redondeando cada uno por su lado el
    // personaje "tiembla" 1px contra la plataforma.
    void renderCharacter(const Mario& character, int riding);
    // Popup de puntaje flotante (seccion 6): busca un slot libre del pool.
    void spawnScorePopup(float x, float y, const std::string& text);
    // Punto de reaparicion en la plataforma solida mas cercana a aroundX,
    // buscando hacia los costados (para no reaparecer sobre un pozo).
    SDL_Point findRespawnPoint(float aroundX, int characterHeight) const;
    // Muerte de uno solo (el otro sigue jugando): reaparece en la
    // plataforma mas cercana, parpadeando invulnerable un rato.
    void respawnNearestPlatform(Mario& character);
    // Al (re)cargar el nivel: deja a Mario/Luigi en el ultimo checkpoint
    // alcanzado (checkpointCol_), o en el spawn del nivel si no hay.
    void placeCharactersAtStart();
    // Bajo el agua usa una invulnerabilidad post-respawn mas larga que en
    // tierra (ver kUnderwaterRespawnInvincibleDuration en Game.cpp) -- un
    // solo lugar para este criterio en vez de repetirlo en las dos
    // funciones de respawn de arriba.
    void grantRespawnInvincibilityForWorld(Mario& character);
    // Recalcula cameraX_ para que siga a Mario (1P) o al punto medio entre
    // Mario y Luigi (2P, sin contar al que esta cayendo muerto), sin dejar
    // ver mas alla de los bordes del nivel (se clampea a [0, ancho del
    // nivel - pantalla] -- 0 si el nivel entero entra en una pantalla, como
    // los escenarios viejos que no scrollean). Tambien actualiza
    // cameraScrollSpeed_ (por eso necesita dt). Ver el comentario de
    // cameraX_/cameraScrollSpeed_.
    void updateCamera(float dt);
    // 2P: los bordes de la pantalla son paredes para los dos, asi ninguno
    // puede alejarse tanto del otro que la camara no los contenga -- el que
    // va adelante queda frenado en el borde derecho hasta que el otro lo
    // alcance (mismo criterio que el co-op de los Mario modernos).
    void keepCharactersOnScreen();
    // true si una caja de ese tamano en (x, y) no toca ningun tile solido
    // ni se sale del nivel por los costados.
    bool isAreaFree(float x, float y, int width, int height) const;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = false;
    bool showDebugOverlay_ = false;   // F3: FPS, camara, musica (para probar)
    int currentFps_ = 0;
    AppState appState_ = AppState::Title;
    TestScenario scenario_ = TestScenario::Items;
    // Opcion marcada con la flecha en el selector (indice en el orden de
    // TestScenario). Se mueve con cruceta/stick o flechas/W/S y se confirma
    // con A/Start o Enter/Espacio; los numeros siguen eligiendo directo.
    int menuSelection_ = 0;
    // El stick genera un evento por cada cambio minimo de eje: esto deja
    // pasar un solo paso de menu por cada inclinacion, hasta que vuelve al
    // centro.
    bool menuStickHeld_ = false;
    bool menuStickHeldX_ = false;

    Settings settings_;
    int titleSelection_ = 0;      // submenu JUGAR
    int mainSelection_ = 0;       // menu principal
    int extrasSelection_ = 0;     // submenu EXTRAS
    int editorSelection_ = 0;     // submenu EDITOR DE NIVELES
    int plusSelection_ = 0;       // submenu PARTIDA + (1 o 2 jugadores)
    int titleMenu_ = 0;           // 0 principal, 1 JUGAR, 2 EXTRAS, 3 EDITOR DE NIVELES
    // Editor de niveles dentro del juego (solo los niveles del jugador, en
    // la carpeta de datos del usuario): se crea la primera vez y queda vivo
    // para volver a el despues de PROBAR.
    std::unique_ptr<LevelEditor> editor_;
    bool editorReturn_ = false;   // probando un nivel del editor: al salir vuelve a el
    enum class EditorEntry { Resume, New, Pick };
    // true si arranco la prueba de un nivel (ya no esta en el titulo).
    bool enterEditor(EditorEntry entry);
    bool confirmingQuit_ = false; // cartel "esta seguro que quiere cerrar el juego?"
    int quitChoice_ = 1;          // 0 SI, 1 NO
    int pauseSelection_ = 0;
    int settingsSelection_ = 0;
    int controlsSelection_ = 0;
    AppState settingsReturnState_ = AppState::Title;
    // Esperando la tecla/boton nuevo de una fila del menu de controles.
    bool waitingBinding_ = false;
    // Partida en curso (vs. modo prueba o --level).
    bool campaign_ = false;
    int campaignWorld_ = 1;
    int campaignLevel_ = 1;
    // Para la vibracion: si el cuadro anterior ya estaba herido/muriendo.
    // Escena de introduccion (Tilemap::introArea): Mario camina solo hasta
    // la tuberia, sin tiempo ni musica. pipeTransitionTimer_ > 0: ya se
    // metio, suena el jingle y despues aparece en pendingWarp_.
    bool cutscene_ = false;
    float pipeTransitionTimer_ = 0.0f;
    PipeWarp pendingWarp_{};
    struct PipeAnim {
        bool active = false;
        bool exiting = false;  // false: entrando; true: saliendo en el destino
        Mario* character = nullptr;
        float velX = 0.0f;     // px/s
        float velY = 0.0f;
        float timer = 0.0f;
        PipeWarp warp{};
    };
    PipeAnim pipeAnim_;
    // 2 a la vez: el companero espera escondido adentro del cano de destino
    // y sale despues del que entro (uno a la vez, por el mismo cano).
    Mario* pipePartner_ = nullptr;
    // Escena del cano (x-2) de a 2: el que no entro primero sigue caminando
    // y se mete detras (en vez de quedar congelado afuera).
    Mario* introPartner_ = nullptr;
    bool introPartnerInside_ = false;
    void updateIntroPartner(float dt);
    // Dibuja a alguien entrando de costado a un cano: lo que paso la boca,
    // recortado al interior (ver renderCharacters).
    void renderEnteringSidePipe(const Mario& character, const PipeWarp& warp);
    bool hurryMusic_ = false;
    float hurryJingleTimer_ = 0.0f;
    // El tema de fin de nivel (bandera o castillo) ya sono.
    bool endJinglePlayed_ = false;
    bool marioWasHurt_ = false;
    bool luigiWasHurt_ = false;
    WorldType worldType_ = WorldType::Overworld;
    bool characterCollisionEnabled_ = true;
    // Desplazamiento horizontal de camara en px de nivel (seccion 3): se le
    // resta a la X de todo lo que se dibuja (ver render() y
    // updateCamera()), la logica de juego (fisica, colisiones) sigue
    // trabajando siempre en coordenadas de nivel absolutas, esto es
    // puramente de dibujado.
    float cameraX_ = 0.0f;
    // Cuanto se movio cameraX_ en el ultimo cuadro, en px/s (ver
    // updateCamera()) -- el original de 1985 engancha la velocidad del
    // Lakitu a esto (ScrollAmount en la disassembly, ver el comentario de
    // Lakitu.h), no a la posicion cruda del personaje: mientras la camara
    // esta scrolleando de verdad (el personaje ya empujo mas alla del
    // centro/borde del nivel) el Lakitu acelera, si no, va lento aunque el
    // personaje se mueva (todavia no llego a mover la camara).
    float cameraScrollSpeed_ = 0.0f;
    // Pide que el proximo updateCamera() salte directo al objetivo en vez
    // de acercarse de a poco (2P): al empezar un nivel, al pasar por una
    // tuberia o al volver al checkpoint, donde la camara anterior ya no
    // tiene nada que ver con donde estan los personajes.
    bool cameraSnapPending_ = true;
    // Modo 1 jugador (solo Mario) vs 2 jugadores simultaneos (ver seccion
    // "Ideas anotadas" del README): cambia como se maneja la muerte -- en 1P
    // Mario muriendo congela y vuelve al checkpoint (como el juego clasico,
    // es "el unico jugador"); en 2P, mientras uno siga vivo el otro
    // reaparece en la plataforma mas cercana sin pausar nada.
    bool twoPlayerMode_ = false;

    Input input1_{ControlProfile::Player1};
    Input input2_{ControlProfile::Player2};
    Tilemap level_;
    Mario mario_;
    Mario luigi_;

    SpriteSheet fireballSprite_;
    SpriteSheet fireballExplosionSprite_;
    SpriteSheet fireballGreenSprite_;            // el fuego de Luigi
    SpriteSheet fireballExplosionGreenSprite_;
    bool loadLuigiSprites();
    std::array<Fireball, kMaxFireballsPerPlayer> marioFireballs_;
    std::array<Fireball, kMaxFireballsPerPlayer> luigiFireballs_;

    // Hongo de prueba: sale de su bloque de interrogacion (ver
    // checkQuestionBlockHit), F11 lo repone igual (sin bloque, cae directo
    // en su posicion de prueba de siempre) por si hace falta probarlo suelto.
    SpriteSheet mushroomSprite_;
    Mushroom mushroom_;

    // Variante 1-UP del hongo (misma entidad Mushroom, ver
    // Mushroom::isOneUp): solo cambia el sprite (verde en vez de naranja,
    // recoloreado a partir de mushroomSprite_) y el efecto al agarrarlo
    // (suma una vida en vez de crecer, ver checkMushroomPickup).
    SpriteSheet oneUpSprite_;

    // Flor de fuego de prueba, mismo criterio que el hongo (F12 la repone).
    SpriteSheet fireFlowerSprite_;
    FireFlower fireFlower_;

    // Estrella de prueba, mismo criterio que el hongo (F4 la repone).
    SpriteSheet starSprite_;
    StarItem star_;

    // Moneda de prueba: a diferencia de los otros tres, esta NO sale de
    // ningun bloque (ver el comentario de spawnItemFromQuestionBlock) --
    // queda quieta puesta en el piso del escenario de Items, se agarra
    // caminando. Tecla C la repone en el mismo lugar.
    SpriteSheet coinSprite_;
    Coin coin_;

    // Bloques de interrogacion de prueba (seccion 5.3, escenario de Items):
    // uno por item de la fase inicial (ver QuestionBlockSpawn/findBlockSpawn
    // en Game.cpp). Animacion compartida por todos los bloques sin usar del
    // nivel (mismo ciclo, sincronizados, como en el original) en vez de un
    // timer por celda. itemEmerge_ maneja el "salir del bloque" de
    // Mushroom/FireFlower/Star (ver el comentario de ItemEmerge).
    SpriteSheet questionBlockSprite_;
    float questionBlockAnimTimer_ = 0.0f;
    int questionBlockAnimFrame_ = 0;
    ItemEmerge itemEmerge_;

    // Animacion (giro de 3 frames) compartida por todas las monedas paradas
    // arriba de un ladrillo (Tilemap::hasCoinOnTop, tile 'C') sin agarrar
    // todavia -- mismo criterio que questionBlockAnimTimer_/frame_ de
    // arriba, reusa coinSprite_ (la misma moneda del piso, Coin.h) en vez de
    // un sprite nuevo.
    float brickCoinAnimTimer_ = 0.0f;
    int brickCoinAnimFrame_ = 0;

    // La moneda que salta de un bloque (BlockCoin.h, distinta de Coin.h de
    // arriba -- ver el comentario de spawnItemFromQuestionBlock): tiene su
    // propio salto, no pasa por itemEmerge_. coinCount_ es el contador de
    // monedas del HUD (seccion 6, ver collectCoin: cada 100 da una vida).
    // livesCount_ es la otra pieza minima de la
    // seccion 6, sumada por checkMushroomPickup cuando el hongo agarrado es
    // el 1-UP (ver BlockItem::OneUp).
    SpriteSheet blockCoinSprite_;
    BlockCoin blockCoin_;
    int coinCount_ = 0;
    // Puntaje y monedas de cada jugador (0 Mario, 1 Luigi) y quien esta
    // sumando ahora; el que termino el nivel se lleva la cuenta del tiempo.
    std::array<int, 2> playerScore_{};
    std::array<int, 2> playerCoins_{};
    int scorer_ = 0;
    int levelFinisher_ = 0;
    // Cadena de pisotones de cada jugador (se reinicia al apoyarse).
    std::array<int, 2> stompChain_{};
    // Pares (enemigo, personaje) que se estan tocando ahora (ver beginContact).
    std::vector<std::pair<const void*, const void*>> enemyContacts_;
    int livesCount_ = 0;
    // 2 a la vez: las vidas de Luigi (las de Mario son livesCount_).
    int luigiLives_ = 0;
    // 2 a la vez, murieron los dos: el que termino su animacion de muerte
    // antes que el otro queda esperando (si no, el juego seguia, volvia a
    // morir por el pozo una y otra vez y el nivel no se reiniciaba).
    std::array<bool, 2> deathPending_{};
    bool coopLives() const { return twoPlayerMode_ && !turnMode_; }
    int& livesOf(const Mario& character) { return (coopLives() && &character == &luigi_) ? luigiLives_ : livesCount_; }
    // Vida extra para el que la gano (scorer_).
    void addLife() { (coopLives() && scorer_ == 1) ? ++luigiLives_ : ++livesCount_; }
    // Murio uno con el otro vivo: pierde una vida; reaparece o queda afuera.
    void onCoopPlayerDied(Mario& character);
    int score_ = 0;
    // Columna del checkpoint mas adelantado alcanzado en este nivel (-1 =
    // ninguno, se arranca del principio). Sobrevive a perder una vida,
    // se borra al empezar el nivel de cero (startScenario, GAME OVER).
    int checkpointCol_ = -1;
    // Cuenta regresiva de la pantalla actual (LevelIntro/GameOver).
    float appStateTimer_ = 0.0f;
    // Tiempo restante en unidades del timer clasico (cada unidad son 24
    // cuadros del original, ver kTimeUnitSeconds en Game.cpp): el HUD
    // muestra la parte entera redondeada para arriba.
    float timeRemaining_ = 0.0f;
    bool hurryUpPlayed_ = false;
    // Fin de nivel: el tiempo que sobro se pasa a puntos (50 por unidad)
    // antes del cartel de nivel completo, como en el original.
    bool timeTallying_ = false;
    float tallySoundTimer_ = 0.0f;

    // Tuberia (seccion 5.3): sprite propio, 2 columnas (0=tope, 1=cuerpo) x
    // una fila por WorldType, cada celda de 32x16 (dos tiles de ancho de una
    // sola vez, ver Tilemap::isPipeTile). checkPipeWarp es quien decide
    // cuando entra por ella (ver el comentario en Game.cpp).
    SpriteSheet pipeSprite_;
    // Variantes de nieve (DaySnow/NightSnowPaletteData: la paleta 0 pasa a
    // blanco y grises): tuberias, escenografia, colinas/arbustos y mastil.
    SpriteSheet pipeSnowSprite_;
    SpriteSheet pipeMushroomSprite_;   // areas de hongos (kMushroomColors)
    SpriteSheet sceneryTilesSnowSprite_;
    SpriteSheet flagpoleSnowSprite_;
    // Paleta de castillo en superficie (6-3).
    SpriteSheet terrainGraySprite_;
    SpriteSheet sceneryTilesGraySprite_;
    // Paleta del area actual ("" si es la normal) y lo que implica.
    const std::string& areaPalette() const;
    bool areaSnow() const;
    SDL_Color skyColor() const;

    // Efectos de sonido (seccion 8.2): ver SoundManager.h para la API y el
    // comentario en Game::init() (donde se cargan) para el mapeo
    // evento->archivo.
    SoundManager audio_;

    // Goomba de prueba (seccion 5.2, primer enemigo), solo en el escenario
    // de enemigos: ese nivel tiene paredes en los dos bordes (ver
    // test_enemies.json) para que el rebote contra pared se pueda ver sin
    // que se termine cayendo del nivel por un borde sin pared. Tecla G lo
    // repone si se pisa o se cae.
    SpriteSheet goombaSprite_;
    // Pool: el de prueba (tecla G) y los que pone el nivel (campo
    // "entities", ver spawnLevelEntities) salen de aca.
    static constexpr int kMaxGoombas = 16;
    std::array<Goomba, kMaxGoombas> goombas_;

    // Koopa Troopa verde de prueba (seccion 5.2), dos instancias en el piso
    // para poder probar que un caparazon en movimiento mate a otro
    // (checkShellVsKoopa), mas un "Koopa Alado" saltarin (KoopaState::Jumping)
    // para probar que un pisoton lo deje caminando derecho. En el juego
    // original de 1985 el color fija cual de los dos Alado le toca: el verde
    // va a los saltos, el rojo se hamaca en un punto fijo (ver koopaRedFlying_
    // mas abajo) -- no existe un verde volador ni un rojo saltarin.
    // Mismo criterio que el Goomba: solo en el escenario de enemigos, tecla
    // K los repone si mueren o se caen.
    SpriteSheet koopaSprite_;
    Koopa koopaA_;
    Koopa koopaB_;
    Koopa koopaJumping_;
    // Koopa rojo de prueba (ver KoopaColor): sprite propio (una sola
    // paleta, no una fila por WorldType como el verde) y setColor(Red) se
    // llama una sola vez en init(). Arranca sobre una plataforma angosta
    // agregada a test_enemies.json para poder ver que da la vuelta en el
    // borde en vez de caerse. Mas su Koopa Alado (volador, ver el comentario
    // de koopaJumping_ arriba), comparte koopaRedSprite_.
    SpriteSheet koopaRedSprite_;
    Koopa koopaRed_;
    Koopa koopaRedFlying_;
    // Koopas que pone el nivel (campo "entities"), aparte de los 5 de
    // prueba de arriba. El color se fija al aparecer (Koopa::setColor).
    static constexpr int kMaxLevelKoopas = 8;
    std::array<Koopa, kMaxLevelKoopas> levelKoopas_;
    // Datos del nivel actual traducidos (ver loadLevelData).
    std::vector<QuestionBlockSpawn> blockSpawns_;
    std::vector<PipeWarp> pipeWarps_;
    // Una entrada por Tilemap::entities(): true si ya aparecio (o se salteo).
    std::vector<bool> entitySpawned_;
    // Orden en que se leen los enemigos (por X, como los datos del original).
    std::vector<size_t> spawnOrder_;
    // Pantalla del original (256 px) para la aparicion de enemigos: su borde
    // izquierdo, solo avanza; spawnArea_ = -1 la reinicia.
    float spawnScreenLeft_ = 0.0f;
    // Donde estaba el foco de la camara el paso anterior (ScrollHandler).
    float cameraFocusPrevX_ = 0.0f;
    int spawnArea_ = -1;
    void updateSpawnScreen();
    int occupiedEnemySlots() const;
    bool enemySlotFree() const { return occupiedEnemySlots() < kEnemySlots; }
    // Lugares de enemigos del original (objetos 0-4).
    static constexpr int kEnemySlots = 5;
    static constexpr float kNesScreenWidth = 256.0f;
    // Area donde esta la camara (ver Tilemap::areas) y el tipo de mundo del
    // nivel cuando el area no fija uno propio.
    int currentArea_ = 0;
    WorldType levelWorldType_ = WorldType::Overworld;

    // Escenografia de fondo (ver gen_scenery.py y el campo "decorations"
    // del nivel): un sheet de un solo frame por pieza.
    static constexpr int kDecorationCount = 13;
    std::array<SpriteSheet, kDecorationCount> decorationSprites_;
    std::array<SpriteSheet, kDecorationCount> decorationSnowSprites_;
    std::array<SpriteSheet, kDecorationCount> decorationGraySprites_;
    // Tuberia en L en piezas de 16x16 (normal, nieve, gris), para la estirada.
    // 0 normal, 1 nieve, 2 gris, 3 subterraneo, 4 castillo (Palettes.h).
    std::array<SpriteSheet, 5> pipeSideSlices_;
    const SpriteSheet& pipeSidePieces() const;

    // Buzzy Beetle de prueba (seccion 5.2, tercer enemigo): fisica y
    // caparazon identicos al Koopa verde caminando (se cae de los bordes,
    // no hay variante roja ni Alado para esta especie), salvo que es inmune
    // al fuego (ver BuzzyBeetle.h). Clase aparte en vez de otra combinacion
    // de KoopaColor/KoopaState porque esas dos diferencias (sin Alado, sin
    // rojo) no encajan como una opcion mas de Koopa. Tecla B lo repone.
    SpriteSheet buzzyBeetleSprite_;
    static constexpr int kMaxBuzzies = 8;
    std::array<BuzzyBeetle, kMaxBuzzies> buzzyBeetles_;
    // El de prueba es el primero del pool (los del nivel usan el resto tambien).
    BuzzyBeetle& buzzyBeetle_ = buzzyBeetles_[0];

    // Pirana Plant de prueba (seccion 5.2, cuarto enemigo): sale de un tile
    // solido angosto en test_enemies.json (no existe el objeto "tuberia"
    // todavia, seccion 5.3) y cicla sola entre escondida y asomada -- no se
    // asoma si algun personaje esta cerca (ver isSafeForPiranhaPlant). Sin
    // caparazon ni pisoton: cualquier toque mientras esta asomada hace dano,
    // solo estrella o fuego la matan (sin parabola, ver PiranhaPlant::kill).
    // Tecla P la repone.
    SpriteSheet piranhaPlantSprite_;
    static constexpr int kMaxPiranhas = 12;
    std::array<PiranhaPlant, kMaxPiranhas> piranhaPlants_;
    PiranhaPlant& piranhaPlant_ = piranhaPlants_[0];

    // Hammer Bro de prueba (seccion 5.2, quinto enemigo): escenario de
    // prueba propio (TestScenario::HammerBro, test_hammerbro.json) en vez
    // de compartir el de Enemigos -- patrulla una plataforma angosta y
    // flotante, mas facil de ver aislado que mezclado con el resto del
    // elenco. El martillo que tira es una entidad separada (Hammer) para
    // poder animar el giro independiente del estado del Bro, ver
    // HammerBro::consumeThrowRequest(). Tecla H lo repone.
    SpriteSheet hammerBroSprite_;
    static constexpr int kMaxHammerBros = 6;
    std::array<HammerBro, kMaxHammerBros> hammerBros_;
    HammerBro& hammerBro_ = hammerBros_[0];
    SpriteSheet hammerSprite_;
    SpriteSheet hammerFlowerSprite_;   // la flor martillo (icono del item)
    SpriteSheet boomerangSprite_;      // el bumeran del traje (4 cuadros de giro)
    SpriteSheet boomerangFlowerSprite_;   // la flor bumeran (20x20)
    // Flor de traje (martillo o bumeran) salida de un bloque: no se mueve,
    // como la de fuego; suitFlowerSize_ es el traje que pone.
    FireFlower suitFlower_;
    MarioSize suitFlowerSize_ = MarioSize::Hammer;
    void checkSuitFlowerPickup(Mario& character);
    void renderSuitFlower();
    std::array<Hammer, kMaxHammers> hammers_;

    // Lakitu de prueba (seccion 5.2, sexto enemigo): escenario propio
    // (TestScenario::LakituSpiny, test_lakitu.json) igual que el Hammer
    // Bro -- vuela siguiendo al personaje mas cercano y tira Spiny (Spiny.h,
    // septimo enemigo) que eclosionan al tocar el piso. Tecla L lo repone.
    SpriteSheet lakituSprite_;
    Lakitu lakitu_;
    // Los Spiny que tira (mismo criterio de pool que Hammer/hammers_): a
    // diferencia del martillo, quedan caminando indefinidamente hasta que
    // los maten, asi que el pool es mas grande (kMaxSpinies). Una sola
    // paleta (ver Spiny.h), su sprite se dibuja siempre con la fila 0.
    SpriteSheet spinySprite_;
    std::array<Spiny, kMaxSpinies> spinies_;

    // Blooper de prueba (seccion 5.2, octavo enemigo): escenario propio
    // (TestScenario::Blooper, test_blooper.json) con worldType_ arrancando
    // en Underwater (nivel de agua) -- flota sin piso, hundiendose despacio
    // entre chorros hacia el personaje mas cercano (ver Blooper.h). Tecla O
    // lo repone (B ya la usa Buzzy Beetle).
    SpriteSheet blooperSprite_;
    static constexpr int kMaxBloopers = 6;
    std::array<Blooper, kMaxBloopers> bloopers_;
    Blooper& blooper_ = bloopers_[0];

    // Cheep Cheep de prueba (seccion 5.2, noveno enemigo): escenario propio
    // (TestScenario::CheepCheep, test_cheepcheep.json) con dos instancias
    // Swimming (una por SwimPattern -- setVariant()/setSwimPattern() se
    // llaman una sola vez en init()) que comparten sprite con la Jumping
    // (una sola paleta, ver CheepCheep.h). Tecla N las repone.
    SpriteSheet cheepCheepSprite_;
    SpriteSheet cheepCheepGraySprite_;   // el gris: una fila por tipo de mundo
    CheepCheep cheepCheepSwim_;     // SwimPattern::Straight
    CheepCheep cheepCheepSwimOsc_;  // SwimPattern::Oscillating
    // Las que nadan en los niveles de agua (campo "entities", tipo "cheep").
    static constexpr int kMaxLevelCheeps = 10;
    std::array<CheepCheep, kMaxLevelCheeps> levelCheeps_;

    // Cheep Cheep Jumping de prueba: escenario propio
    // (TestScenario::CheepCheepJump, test_cheepcheep_jump.json, largo con
    // scroll) en vez de compartir el de arriba -- a diferencia de la
    // Swimming necesita que el personaje camine (worldType_ Overworld, no
    // Underwater) para poder probar el cruce mientras avanza. Pool (mismo
    // criterio que spinies_/hammers_): cheepCheepJumpSpawnTimer_ hace que
    // vaya apareciendo una tras otra desde abajo del borde inferior de
    // pantalla, un poco por delante del personaje mas adelantado, mientras
    // este escenario este activo (ver Game::update). Tecla J fuerza una de
    // una si hay lugar en el pool, sin esperar al timer.
    std::array<CheepCheep, kMaxCheepCheepJumps> cheepCheepJumps_;
    float cheepCheepJumpSpawnTimer_ = 0.0f;
    // Deriva (izquierda/derecha) de la ultima que aparecio: se alterna
    // siempre en Game::spawnNextCheepCheepJump en vez de tirar una moneda
    // cada vez -- con moneda independiente, una racha de varias seguidas
    // para el mismo lado (estadisticamente normal pero se nota mal en la
    // practica) hacia parecer que "solo saltan para un lado".
    bool cheepCheepJumpLastDriftRight_ = false;

    // Bill Bala de prueba (enemigo nuevo, ver BulletBill.h -- surgio de una
    // confusion con el sprite de Cheep Cheep): escenario propio
    // (TestScenario::BulletBill, test_bulletbill.json), dos instancias
    // disparadas en direcciones opuestas para probar el cruce. Tecla I lo
    // repone (B/N/J ya estan tomadas).
    SpriteSheet bulletBillSprite_;
    static constexpr int kMaxBulletBills = 8;
    std::array<BulletBill, kMaxBulletBills> bulletBills_;
    BulletBill& bulletBillA_ = bulletBills_[0];
    BulletBill& bulletBillB_ = bulletBills_[1];

    // Podoboo de prueba (seccion 5.2, decimo enemigo): escenario propio
    // (TestScenario::Podoboo, test_podoboo.json) con worldType_ arrancando
    // en Castle (solo aparece en castillos en el original, ver Podoboo.h).
    // Una sola instancia fija (no un pool: el pozo de lava no se mueve ni se
    // agota, siempre el mismo). Tecla P lo repone.
    SpriteSheet podobooSprite_;
    static constexpr int kMaxPodoboos = 6;
    std::array<Podoboo, kMaxPodoboos> podoboos_;
    Podoboo& podoboo_ = podoboos_[0];

    // Bowser de prueba (seccion 5.2, jefe final): escenario propio
    // (TestScenario::Bowser, test_bowser.json) con worldType_ arrancando en
    // Castle (solo aparece ahi en el original, ver Bowser.h). Solo escupe
    // fuego (a proposito no tira martillos, ver el comentario de Bowser.h):
    // bowserFires_ es su pool propio (mismo criterio que hammers_/spinies_).
    // El hacha (axe_) que lo derrota de una sola vez esta en el otro
    // extremo del nivel de prueba. Tecla Y repone Bowser y el hacha juntos.
    SpriteSheet bowserSprite_;
    Bowser bowser_;
    SpriteSheet bowserFireSprite_;
    std::array<BowserFire, kMaxBowserFires> bowserFires_;
    SpriteSheet axeSprite_;
    Axe axe_;

    // Bandera de meta: solo existe en los niveles que tienen tiles 'F' (ver
    // startScenario). levelClearTimer_ > 0 = nivel completado, se muestra
    // el cartel y al llegar a 0 vuelve al selector -- placeholder hasta que
    // exista el paso al nivel siguiente (seccion 6).
    SpriteSheet flagpoleSprite_;
    SpriteSheet flagSprite_;
    Flagpole flagpole_;
    float levelClearTimer_ = 0.0f;
    // Archivo de TestScenario::CustomLevel (ver startWithLevelFile).
    std::string customLevelPath_;
    SDL_Texture* worldTarget_ = nullptr;
    int customStartCol_ = -1;
    int customStartRow_ = -1;
    std::string dumpMapPath_;
    bool dumping_ = false;
    void dumpLevelMap();
    // El nivel ya se esta terminando (bandera agarrada o hacha tocada): el
    // timer deja de descontar.
    bool levelEnding_ = false;

    // Castillo: puente que se derrumba (tramos pendientes, de adelante para
    // atras) y espera entre que Bowser cae y el cierre del nivel.
    // Piso ('#') y bloque duro ('S'), ver Tilemap::renderTerrain.
    SpriteSheet terrainSprite_;
    SpriteSheet lavaSprite_;
    SpriteSheet bridgeSprite_;
    bool bridgeCollapsing_ = false;
    float bridgeCollapseTimer_ = 0.0f;
    std::vector<SDL_Point> bridgeTilesToCollapse_;
    float castleClearTimer_ = 0.0f;
    // Final con bandera (RunStarFlagObj / GameTimerFireworks / RunFireworks
    // del original): al terminar la cuenta del tiempo sube la bandera con
    // estrella del castillo y, si el ultimo digito del tiempo al llegar al
    // mastil era 1, 3 o 6, salen esos fuegos artificiales (500 cada uno).
    struct EndSequence {
        bool fromFlagpole = false;  // la cuenta empezo por la bandera (no por el hacha)
        int fireworks = 0;          // cuantos faltan
        int lookup = 0;             // indice en la tabla de posiciones
        bool flagShown = false;
        bool active = false;
        float flagX = 0.0f;
        float flagY = 0.0f;
        float flagTopY = 0.0f;
        float spawnTimer = 0.0f;
        float waitTimer = 0.0f;
        struct Burst {
            float x = 0.0f;
            float y = 0.0f;
            float t = 0.0f;
        };
        std::vector<Burst> bursts;
    };
    EndSequence endSequence_;
    SpriteSheet starFlagSprite_;
    void startEndSequence();
    // true mientras siga (la bandera subiendo o fuegos pendientes).
    bool updateEndSequence(float dt);
    void renderStarFlag();
    void renderFireworks();
    // Warp zone (ScrollLockObject_Warp / WriteWarpZoneMessage): el cartel
    // arriba de las tuberias y el mundo al que lleva cada una.
    void renderWarpZoneText();
    // Warp zone (ScrollLockObject_Warp): la camara no pasa de esa columna
    // (el borde derecho de la pantalla queda ahi) y al llegar se van las
    // Piranha Plants de la zona (KillEnemies 0x0D).
    struct WarpZoneLock {
        int col = 0;        // columna del objeto
        int firstPipe = 0;  // primera tuberia de la zona
        bool reached = false;
    };
    std::vector<WarpZoneLock> warpZoneLocks_;
    void updateWarpZones();
    // Bowser falso (BowserIdentities del original): en los mundos 1-7, el
    // que se derrota con fuego cae mostrando el enemigo que era en
    // realidad. -1 = ninguno (el de verdad, o no lo mato el fuego).
    int fakeBowser_ = -1;
    // Ganado el 8-4: la proxima partida es la segunda vuelta (gPrimaryHardMode).
    bool secondQuestNext_ = false;
    // 1-UP escondidos (Hidden1UpFlag / Hidden1UpCoinAmts del original): el
    // bloque solo existe si la marca esta puesta cuando la pantalla llega a
    // el, y al dibujarse la gasta. Se pone al empezar la partida y al
    // terminar un x-3 habiendo juntado ahi 21/35/22/27/23/24/35/99 monedas
    // (segun el mundo).
    bool hidden1UpFlag_ = true;
    // Bloque de muchas monedas: el tiempo que corre desde su primer golpe.
    // Cadena del puente de Bowser: se ve hasta tocar el hacha (ahi se
    // borra antes de que caiga el puente, como el original).
    // Toad / princesa al final del castillo: tirado el puente, el personaje
    // camina solo hasta quedar frente a el y aparece el mensaje (primero
    // "GRACIAS MARIO!", al rato el resto) antes de la cuenta del tiempo.
    SDL_Point retainer_{-1, -1};
    SpriteSheet retainerSprite_;
    bool castleWalk_ = false;
    int castleLines_ = 0;         // renglones del mensaje a la vista (0 = ninguno)
    // Final del juego (la princesa del 8-4), como PrintVictoryMessages del
    // original: un renglon por cada "tic" de 64 cuadros (THANK YOU, dos tics
    // sin nada, YOUR QUEST IS OVER + el tema de la princesa, WE PRESENT YOU
    // A NEW QUEST, PUSH BUTTON B, TO SELECT A WORLD) y despues espera un
    // boton para volver al titulo.
    bool finalEnding_ = false;
    float finalTimer_ = 0.0f;
    int finalLines_ = 0;          // contador de mensajes del original (0..7)
    bool finalMusic_ = false;     // ya suena el tema de la princesa
    bool finalWaiting_ = false;   // todo a la vista: espera un boton
    float castleMessageTimer_ = 0.0f;
    SDL_Point bridgeChain_{-1, -1};
    bool bridgeChainGone_ = false;
    SpriteSheet bridgeChainSprite_;
    bool brickCoinTimerStarted_ = false;
    float brickCoinTimer_ = 0.0f;
    // Partidas: la pantalla de los espacios de guardado (PARTIDA NUEVA
    // arriba y abajo las guardadas, con la vista previa del nivel donde
    // siguen). slotChoice_ -1 es PARTIDA NUEVA; replacingSlot_: estan todos
    // ocupados y se elige cual reemplazar.
    bool choosingSlot_ = false;
    bool replacingSlot_ = false;
    int slotChoice_ = -1;
    bool confirmingOverwrite_ = false;
    int overwriteChoice_ = 1;            // 0 SI, 1 NO
    // CONTINUAR: al elegir un espacio, CONTINUAR o BORRAR (y el SI/NO de
    // borrar).
    // CONTENIDO EXTRA (EXTRAS): la lista de lo desbloqueable.
    bool extraContentOpen_ = false;
    bool confirmingPurchase_ = false;
    // ROCOLA (EXTRAS): la lista de musicas y sonidos.
    bool jukeboxOpen_ = false;
    int jukeboxSelection_ = 0;
    int jukeboxPlaying_ = -1;
    void handleJukeboxInput(MenuInput input);
    void renderJukebox();
    int purchaseChoice_ = 1;             // 0 SI, 1 NO
    int extraContentSelection_ = 0;
    void renderExtraContent();
    int drawCoinAmount(int x, int y, int amount, SDL_Color color, const std::string& suffix = "");
    // Logros: la partida en curso va desde el 1-1 sin recibir dano.
    bool campaignNoDamage_ = false;
    // ... y sin saltearse niveles (sin zonas de warp).
    bool campaignNoSkip_ = false;
    // ... y sin agarrar estrellas (ni la del truco).
    bool campaignNoStar_ = false;
    // ... sin agarrar super champinones / flores de fuego (ni los del truco).
    bool campaignNoMushroom_ = false;
    bool campaignNoFlower_ = false;
    // ... eliminando a todos los Hammer Bros: los de este nivel que ya cayeron
    // (por entidad, sobrevive a las muertes en el mismo nivel).
    bool campaignAllHammerBros_ = false;
    // Tiempo jugado de la partida (logro TIEMPO INFINITO): no corre en la
    // pausa ni fuera de la partida; el espacio lo guarda para seguir.
    float campaignTime_ = 0.0f;
    // Logro LUIGI DE LOST LEVELS: la partida, desde el 1-1, con Luigi (1 jugador).
    bool campaignOnlyLuigi_ = false;
    // Resumen del final: enemigos eliminados y muertes de cada jugador
    // (0 Mario / el de 1 jugador, 1 Luigi); el espacio los guarda.
    int campaignEnemies_ = 0;
    std::array<int, 2> campaignDeaths_{};
    std::array<bool, 2> wasDying_{};          // mario_ / luigi_ el cuadro anterior
    void noteEnemyKilled() {
        if (campaign_) ++campaignEnemies_;
    }
    int finalScore(int player) const;
    // Resumen (summaryStage_ 0), nombre para la tabla (1) y la tabla (2).
    struct SummaryData {
        int enemies = 0;
        std::array<int, 2> deaths{};
        std::array<int, 2> scores{};
        float time = 0.0f;
        bool twoPlayers = false;
        bool luigi = false;
        bool plus = false;
    };
    SummaryData summary_;
    int summaryStage_ = 0;
    bool summaryPending_ = false;             // DemoEnd: al salir, el resumen
    std::vector<int> pendingNames_;           // jugadores que entran en la tabla (0/1)
    std::string nameEntry_;
    int nameCursor_ = 0;
    int highlightScore_ = -1;                 // la fila recien agregada
    void openSummary();
    void handleSummaryInput(MenuInput input);
    bool handleNameTyping(const SDL_Event& event);
    void startNameEntry();
    void commitName();
    void renderSummary();
    void renderHighScores(int top);
    std::string playerLabel(int player) const;
    static constexpr float kInfiniteTimeLimit = 25.0f * 60.0f;
    void storeSlotPlayTime();
    std::set<int> defeatedHammerBros_;
    void onHammerBroStomped();
    void checkAllHammerBrosCleared();
    template <typename T>
    void noteStompKill(const T&) {}
    void noteStompKill(const HammerBro&) { onHammerBroStomped(); }
    // Flores de los trajes al azar en vez de la de fuego (si se compraron).
    BlockItem rollSuitFlower(BlockItem item) const;
    // ITEM: ESTRELLA INFINITA puesta en este nivel.
    bool infiniteStarActive_ = false;
    static constexpr float kInfiniteStarDuration = 100000.0f;
    void unlockExtra(const std::string& id);
    // Aviso de desbloqueo (estilo logro): los que se ganaron, de a uno; el
    // de adelante se esta mostrando desde unlockToastStart_ (ms, 0 = todavia no).
    std::vector<std::string> unlockToasts_;
    Uint32 unlockToastStart_ = 0;
    void renderUnlockToast();
    void checkExtraProgress();
    // El nivel en curso todavia no recibio dano (logro de la estrella).
    bool levelUndamaged_ = true;
    // ITEMS de la pausa (trucos activados): quien pauso, la lista abierta y
    // su seleccion.
    int pausedBy_ = 0;
    bool pauseItemsOpen_ = false;
    int pauseItemSelection_ = 0;
    std::vector<int> enabledItemExtras() const;   // indices en kExtraContent
    void applyCheatItem(int extraIndex);
    bool slotActions_ = false;
    int slotActionChoice_ = 0;           // 0 CONTINUAR, 1 BORRAR
    bool confirmingDelete_ = false;
    int deleteChoice_ = 1;               // 0 SI, 1 NO
    std::function<void()> pendingStart_; // la partida que arranca al elegir espacio
    int currentSlot_ = -1;               // donde se guarda la partida en curso (-1: no se guarda)
    bool worldSelectFlow_ = false;       // ELEGIR MUNDO: no usa espacio
    std::array<SDL_Texture*, Settings::kSaveSlots> slotPreviews_{};
    bool anySaveSlot() const;
    void askSlotThen(std::function<void()> start);
    void handleSlotPickerInput(MenuInput input);
    void startNewGameInSlot(int slot);
    void refreshSlotPreviews();
    SDL_Texture* renderLevelPreview(const std::string& path);
    void writeCurrentSlot(int world, int level);
    void renderSlotPicker();
    // 2 jugadores por turnos (el modo del original): un solo personaje en
    // pantalla; al morir pasa el turno al otro (si le quedan vidas), cada uno
    // con sus vidas, puntaje, monedas, nivel, checkpoint, tamano y su 1-UP
    // escondido (como OffScr_* del original). Luigi juega con los controles
    // del jugador 2.
    struct TurnState {
        int lives = 0;
        int score = 0;
        int coins = 0;
        int world = 1;
        int level = 1;
        int checkpointCol = -1;
        MarioSize size = MarioSize::Small;
        bool hidden1Up = true;
    };
    bool turnMode_ = false;
    int turnPlayer_ = 0;
    std::array<TurnState, 2> turnStates_{};
    bool inputsSwapped_ = false;
    bool choosingTwoMode_ = false;
    int twoModeChoice_ = 0;   // 0 a la vez, 1 por turnos
    void startTurnCampaign();
    void saveTurnState();
    void loadTurnState(int player);
    // Despues de morir: true si le paso el turno al otro.
    bool passTurn();
    int coinTallyForOneUps_ = 0;
    std::vector<SDL_Point> hidden1Ups_;
    void updateHidden1Ups();

    struct Cannon {
        int col;
        int row;
        float timer;
    };
    std::vector<Cannon> cannons_;
    struct Firebar {
        float centerX;  // centro de giro en px
        float centerY;
        int length;     // cantidad de bolas
        float angle;    // grados
        float speed;    // grados/s
    };
    std::vector<Firebar> firebars_;
    // Remolinos de los niveles de agua (entidad "whirlpool"): de x a x+length.
    struct Whirlpool {
        float x;
        float length;
    };
    std::vector<Whirlpool> whirlpools_;
    void updateWhirlpools(Mario& character, float dt);
    // Zonas de Cheep Cheep saltarinas, en px (inicio, fin).
    std::vector<std::pair<float, float>> cheepJumpZones_;
    // X donde el Lakitu del nivel se retira (-1: nunca).
    float lakituEndX_ = -1.0f;
    // Zona del Lakitu en curso (para que vuelva si lo matan).
    bool lakituZoneActive_ = false;
    float lakituSpawnY_ = 48.0f;
    float lakituRespawnTimer_ = 0.0f;
    // Castillo terminado: cartel del Toad (o el de la princesa en el 8-4).
    bool castleMessage_ = false;
    SpriteSheet sceneryTilesSprite_;
    // Trampolines (entidad "spring" del nivel, ver updateSprings).
    struct Spring {
        float x = 0.0f;       // izquierda
        float bottom = 0.0f;  // donde apoya (abajo del sprite)
        int frame = 0;        // 0 estirado, 1 a medias, 2 comprimido
        float timer = 0.0f;   // compresion en curso (> 0)
        bool boost = false;
        bool jumpWasDown = false;   // para pedir una apretada nueva, no mantenida
        Mario* rider = nullptr;
    };
    std::vector<Spring> springs_;
    SpriteSheet springSprite_;
    void updateSprings(float dt);
    void renderSprings();
    // Frenesis del original: Bill Balas o Cheep Cheeps nadadoras desde el
    // borde derecho, o llamas de Bowser, mientras el jugador esta en la zona.
    struct Frenzy {
        enum class Kind { BulletBill, CheepCheep, Flame } kind = Kind::BulletBill;
        float startX = 0.0f;
        float endX = 0.0f;
        float timer = 1.0f;
        bool stopped = false;   // llamas: aparecio Bowser, no salen mas
    };
    // Balanzas (BalancePlatform del original): dos plataformas colgadas de
    // la misma soga; la que se pisa baja y la otra sube lo mismo. Si la que
    // sube llega arriba con alguien parado en la otra, se corta la soga y
    // caen las dos (1000 puntos).
    struct Balance {
        int a = -1;            // indices en platforms_
        int b = -1;
        float velocity = 0.0f; // px/s de a (positivo = baja)
        bool falling = false;
        float fallA = 0.0f;
        float fallB = 0.0f;
        int ropeRow = 3;
    };
    std::vector<Balance> balances_;
    // Enredaderas (VineObjectHandler): crecen desde el bloque hasta arriba
    // del nivel; si llevan a otro lado, llegar trepando arriba de la
    // pantalla cambia de area y se aparece trepando otra desde abajo.
    struct Vine {
        float x = 0.0f;         // centro
        float top = 0.0f;
        float bottom = 0.0f;
        float targetTop = 0.0f;
        int destCol = -1;
        int destRow = -1;
        float growSpeed = 60.0f;   // px/s
        // Hasta donde se puede bajar trepando (la de llegada a otra area: la
        // altura del piso, no se vuelve a bajar por donde se subio).
        float climbBottom = 1.0e9f;
    };
    std::vector<Vine> vines_;
    // Entrada trepando a otra area (PlayerEntrance con enredadera): la
    // enredadera crece desde abajo de la pantalla, despues sube solo hasta
    // Y 0x98 y camina a la derecha hasta X 0x48; recien ahi vuelve el control.
    struct VineEntry {
        enum class Phase { None, Grow, Climb, Walk } phase = Phase::None;
        int vine = -1;
        float walkTargetX = 0.0f;
    };
    VineEntry vineEntry_;
    void updateVineEntry();
    std::vector<SDL_FRect> vineRects_;
    std::vector<uint8_t> vineFloors_;
    SpriteSheet vineSprite_;
    void spawnVine(int col, int row, int destCol, int destRow);
    void updateVines(float dt);
    void renderVines();
    void checkVineWarp(Mario& character);
    SpriteSheet pulleySprite_;
    void updateBalances(float dt);
    void renderBalanceRopes();
    std::vector<Frenzy> frenzies_;
    int flameTimerIndex_ = 0;
    void updateFrenzies(float dt);
    // Laberintos de castillo (ProcLoopCommand del original): al llegar a
    // triggerX hay que estar parado con los pies en feetY; si no, todo
    // vuelve 64 columnas (ExecGameLoopback) y se borran los enemigos. Con
    // group 3 (7-4) cuenta recien al tercer chequeo seguido, y tienen que
    // salir bien los tres. feetY < 0: no se puede pasar (8-4, ahi se sale
    // por tuberias).
    struct LoopCheck {
        float triggerX = 0.0f;
        float feetY = -1.0f;
        int group = 1;
        int back = 64;   // columnas que vuelve (la seccion que repite el original)
        bool armed = true;
    };
    std::vector<LoopCheck> loopChecks_;
    int loopPassCount_ = 0;
    int loopCorrectCount_ = 0;
    void updateLoopChecks();
    void execLoopback(int back);
    // Pantalla de titulo y HUD (gen_title.py): el cartel "SUPER MARIO
    // BROS.", el hongo que marca la opcion, la moneda del HUD (3 frames
    // por WorldType) y Mario chico parado (pantalla "WORLD x-y").
    SpriteSheet titleLogoSprite_;
    SpriteSheet titleCursorSprite_;
    SpriteSheet hudCoinSprite_;
    SpriteSheet introMarioSprite_;
    SpriteSheet introLuigiSprite_;
    // 1 jugador con Luigi: mario_ lleva los sprites de Luigi (ver
    // Mario::swapAppearance) y el HUD dice LUIGI.
    bool playAsLuigi_ = false;
    bool appearanceSwapped_ = false;
    // Titulo: eligiendo personaje para 1 jugador (0 Mario, 1 Luigi).
    bool choosingCharacter_ = false;
    int characterChoice_ = 0;
    // SELECCIONAR MUNDO: cartel para elegir mundo (arriba/abajo) y nivel
    // (izquierda/derecha); despues pregunta con quien jugar y arranca ahi.
    bool choosingLevel_ = false;
    int startWorld_ = 1;
    int startLevel_ = 1;
    void setPlayAsLuigi(bool luigi);

    // Plataformas moviles del nivel (campo "platforms").
    static constexpr int kMaxPlatforms = 12;
    std::array<MovingPlatform, kMaxPlatforms> platforms_;
    SpriteSheet liftSprite_;
    int marioPlatform_ = -1;
    int luigiPlatform_ = -1;
    // Un rectangulo por plataforma (mismo indice que platforms_; ancho 0 si
    // esta inactiva), ver Mario::setPlatforms.
    std::vector<SDL_FRect> platformRects_;

    // Ladrillo rompible (seccion 5.3, tile 'B' en el JSON del nivel, ver
    // Tilemap::isBrickTile): grande/fuego lo rompe saltando de cabeza
    // (Game::checkBrickBlockHit), chico solo lo golpea sin romperlo. El
    // sprite del bloque en si lo dibuja el propio Tilemap::render(); estos
    // dos son solo para los pedazos que salen volando al romperse.
    SpriteSheet brickDebrisSprite_;
    std::array<BrickDebris, kMaxBrickDebris> brickDebris_;
    SpriteSheet brickBlockSprite_;
    std::array<BlockBump, kMaxBlockBumps> blockBumps_;

    // Pool de popups de puntaje (hongo/flor/estrella dan 1000, como en el
    // original; la moneda no muestra popup, solo suma al contador cuando
    // exista). Varios a la vez posibles si los dos jugadores agarran cosas
    // casi juntos.
    std::array<ScorePopup, 4> scorePopups_;
};
